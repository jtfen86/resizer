#define _UNICODE
#include <windows.h>
#include <shellapi.h>
#include <psapi.h>
#include <cwctype>
#include <string>
#include <set>
#include <fstream>

// Custom message and menu item IDs
#define WM_RESIZE_WINDOW (WM_APP + 1)
#define WM_TRAYICON (WM_APP + 2)
#define ID_RESIZE_1280x720 1001
#define ID_RESIZE_1600x900 1002
#define ID_RESIZE_1920x1080 1003
#define ID_EXIT 1004
#define ID_TRAY_EXIT 1005

// Global variables
HHOOK g_hHook = NULL;
HWND g_hMainWnd = NULL;
HINSTANCE g_hInstance;
NOTIFYICONDATA g_nid = {0};
std::set<std::wstring> g_whitelist;

// Structure to pass window handle and click position
struct ResizeInfo {
    HWND hWnd;
    POINT pt;
};

// Mouse hook procedure
LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && wParam == WM_RBUTTONDOWN) {
        MSLLHOOKSTRUCT* pMouseStruct = (MSLLHOOKSTRUCT*)lParam;
        POINT pt = pMouseStruct->pt;
        HWND hWnd = WindowFromPoint(pt);
        if (hWnd) {
            LRESULT hitTest = SendMessage(hWnd, WM_NCHITTEST, 0, MAKELPARAM(pt.x, pt.y));
            if (hitTest == HTCAPTION) {
                ResizeInfo* pInfo = new ResizeInfo;
                pInfo->hWnd = hWnd;
                pInfo->pt = pt;
                PostMessage(g_hMainWnd, WM_RESIZE_WINDOW, 0, (LPARAM)pInfo);
                return 1;
            }
        }
    }
    return CallNextHookEx(g_hHook, nCode, wParam, lParam);
}

// Function to resize and center a window on its monitor
void ResizeAndCenterWindow(HWND hWnd, int width, int height) {
    HMONITOR hMonitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi;
    mi.cbSize = sizeof(mi);
    GetMonitorInfo(hMonitor, &mi);
    RECT rcMonitor = mi.rcMonitor;

    int x = rcMonitor.left + (rcMonitor.right - rcMonitor.left - width) / 2;
    int y = rcMonitor.top + (rcMonitor.bottom - rcMonitor.top - height) / 2;

    SetWindowPos(hWnd, NULL, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
}

// Function to add the system tray icon
void AddTrayIcon(HWND hWnd) {
    g_nid.cbSize = sizeof(NOTIFYICONDATA);
    g_nid.hWnd = hWnd;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WM_TRAYICON;
    g_nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcscpy_s(g_nid.szTip, L"Window Resizer");
    Shell_NotifyIcon(NIM_ADD, &g_nid);
}

// Function to remove the system tray icon
void RemoveTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &g_nid);
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_RESIZE_WINDOW: {
            ResizeInfo* pInfo = (ResizeInfo*)lParam;
            DWORD pid;
            GetWindowThreadProcessId(pInfo->hWnd, &pid);
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
            if (hProcess) {
                WCHAR exeName[MAX_PATH];
                if (GetModuleBaseNameW(hProcess, NULL, exeName, MAX_PATH)) {
                    std::wstring processName = exeName;
                    for (auto& c : processName) {
                        c = towlower(c);
                    }
                    if (g_whitelist.count(processName)) {
                        HMENU hMenu = CreatePopupMenu();
                        AppendMenu(hMenu, MF_STRING, ID_RESIZE_1280x720, L"1280x720");
                        AppendMenu(hMenu, MF_STRING, ID_RESIZE_1600x900, L"1600x900");
                        AppendMenu(hMenu, MF_STRING, ID_RESIZE_1920x1080, L"1920x1080");
                        AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
                        AppendMenu(hMenu, MF_STRING, ID_EXIT, L"Exit");

                        SetForegroundWindow(hWnd);
                        int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pInfo->pt.x, pInfo->pt.y, 0, hWnd, NULL);
                        DestroyMenu(hMenu);

                        if (cmd == ID_RESIZE_1280x720) {
                            ResizeAndCenterWindow(pInfo->hWnd, 1280, 720);
                        }
                        else if (cmd == ID_RESIZE_1600x900) {
                            ResizeAndCenterWindow(pInfo->hWnd, 1600, 900);
                        }
                        else if (cmd == ID_RESIZE_1920x1080) {
                            ResizeAndCenterWindow(pInfo->hWnd, 1920, 1080);
                        }
                        else if (cmd == ID_EXIT) {
                            PostQuitMessage(0);
                        }
                    }
                }
                CloseHandle(hProcess);
            }
            delete pInfo;
            break;
        }
        case WM_TRAYICON: {
            if (lParam == WM_RBUTTONDOWN) {
                POINT pt;
                GetCursorPos(&pt);
                HMENU hMenu = CreatePopupMenu();
                AppendMenu(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");

                SetForegroundWindow(hWnd);
                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, pt.x, pt.y, 0, hWnd, NULL);
                DestroyMenu(hMenu);

                if (cmd == ID_TRAY_EXIT) {
                    PostQuitMessage(0);
                }
            }
            break;
        }
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Main entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;

    // Read whitelist.txt
    std::wifstream file("whitelist.txt");
    if (!file.is_open()) {
        MessageBoxW(NULL, L"Failed to open whitelist.txt", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    std::wstring line;
    while (std::getline(file, line)) {
        line.erase(0, line.find_first_not_of(L" \t\r\n"));
        line.erase(line.find_last_not_of(L" \t\r\n") + 1);
        if (!line.empty()) {
            for (auto& c : line) {
                c = towlower(c);
            }
            g_whitelist.insert(line);
        }
    }
    file.close();

    // Register window class
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"ResizeWindowClass";
    if (!RegisterClassExW(&wc)) {
        MessageBoxW(NULL, L"Window Registration Failed!", L"Error", MB_OK);
        return 1;
    }

    // Create message-only window
    g_hMainWnd = CreateWindowExW(0, L"ResizeWindowClass", L"", 0, 0, 0, 0, 0,
                                 HWND_MESSAGE, NULL, hInstance, NULL);
    if (!g_hMainWnd) {
        MessageBoxW(NULL, L"Window Creation Failed!", L"Error", MB_OK);
        return 1;
    }

    // Set mouse hook
    g_hHook = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, hInstance, 0);
    if (!g_hHook) {
        MessageBoxW(NULL, L"Failed to set mouse hook!", L"Error", MB_OK);
        return 1;
    }

    // Add tray icon
    AddTrayIcon(g_hMainWnd);

    // Message loop
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // Cleanup
    RemoveTrayIcon();
    if (g_hHook) {
        UnhookWindowsHookEx(g_hHook);
    }

    return 0;
}
