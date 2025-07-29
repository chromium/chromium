// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/webrtc/desktop_capture_devices_util_win.h"

#include <windows.h>

#include <array>

#include "base/base_paths_win.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/win/scoped_handle.h"
#include "base/win/window_enumerator.h"
#include "base/win/windows_types.h"

namespace {

bool IsUWPApp(const base::FilePath& app_path) {
  base::FilePath system_path;
  if (!base::PathService::Get(base::DIR_SYSTEM, &system_path)) {
    return false;
  }

  // The ApplicationFrameHost.exe is the host process for UWP apps. It is
  // located in the system directory (usually C:\Windows\System32).
  if (base::FilePath::CompareEqualIgnoreCase(system_path.value(),
                                             app_path.DirName().value()) &&
      base::FilePath::CompareEqualIgnoreCase(app_path.BaseName().value(),
                                             L"ApplicationFrameHost.exe")) {
    return true;
  }

  return false;
}

// Callback to be provided to `base::win::EnumerateChildWindows` to find the
// child window of the UWP app with the "Windows.UI.Core.CoreWindow" class name.
// When the target window is found, this function should return `true`,
// signaling to `base::win::EnumerateChildWindows` that it should stop
// enumerating and store the found window handle in `out_uwp_app_core_window`.
// https://learn.microsoft.com/en-us/previous-versions/windows/desktop/legacy/ms633493(v=vs.85)
bool IsUWPAppCoreWindow(HWND& out_uwp_app_core_window, HWND hwnd) {
  std::wstring class_name = base::win::GetWindowClass(hwnd);
  // Check if the class name matches the UWP app's core window class name.
  if (class_name == L"Windows.UI.Core.CoreWindow") {
    out_uwp_app_core_window = hwnd;
    return true;
  }

  return false;
}

// Given a window handle `hwnd` for a UWP app, finds the pid of the app's main
// process.
base::ProcessId GetUWPAppCoreWindowProcessId(HWND hwnd) {
  HWND main_uwp_app_core_window = nullptr;

  // For UWP apps, we need to find the child window which has the class name
  // Windows.UI.Core.CoreWindow and get the process ID from it.
  base::win::EnumerateChildWindows(
      hwnd, base::BindRepeating(&IsUWPAppCoreWindow,
                                std::ref(main_uwp_app_core_window)));
  // There is no child window with the class name Windows.UI.Core.CoreWindow.
  if (!main_uwp_app_core_window) {
    return base::kNullProcessId;
  }

  DWORD main_process_id;
  DWORD thread_id =
      GetWindowThreadProcessId(main_uwp_app_core_window, &main_process_id);
  if (!thread_id || !main_process_id) {
    return base::kNullProcessId;
  }

  return main_process_id;
}

// Returns the executable's path for the given process handle.
base::FilePath GetProcessExecutablePath(HANDLE process_handle) {
  std::wstring image_path(MAX_PATH, L'\0');
  DWORD path_length = image_path.size();
  BOOL success = QueryFullProcessImageNameW(process_handle, 0,
                                            image_path.data(), &path_length);
  if (!success && GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
    // Process name is potentially greater than MAX_PATH, try larger max size.
    // https://docs.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation
    image_path.resize(UNICODE_STRING_MAX_CHARS);
    path_length = image_path.size();
    success = QueryFullProcessImageName(process_handle, 0, image_path.data(),
                                        &path_length);
    if (!success) {
      return base::FilePath();
    }
  }

  return base::FilePath(image_path);
}

}  // namespace

base::ProcessId GetAppMainProcessId(intptr_t window_id) {
  HWND hwnd = reinterpret_cast<HWND>(window_id);
  DWORD process_id;
  DWORD thread_id = GetWindowThreadProcessId(hwnd, &process_id);
  if (!thread_id || !process_id) {
    return base::kNullProcessId;
  }

  base::win::ScopedHandle process_handle(
      OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                  /*bInheritHandle=*/FALSE, process_id));
  if (!process_handle.is_valid()) {
    return base::kNullProcessId;
  }

  // UWP apps' UI follow a hierarchy where the top-level window is created by
  // ApplicationFrameHost.exe and the actual app window is a child window of the
  // top-level window. Therefore, in this situation, we need to look down in the
  // window hierarchy to find the correct process id.
  base::FilePath app_path = GetProcessExecutablePath(process_handle.get());
  // Checks if process is a UWP app.
  if (IsUWPApp(app_path)) {
    return GetUWPAppCoreWindowProcessId(hwnd);
  }

  base::ProcessId main_process_id_candidate = process_id;
  base::ProcessId parent_id = base::GetParentProcessId(process_handle.get());
  if (parent_id <= 0) {
    // If there is no parent process, we return the current process id.
    return main_process_id_candidate;
  }

  base::win::ScopedHandle parent_process_handle(OpenProcess(
      PROCESS_QUERY_LIMITED_INFORMATION, /*bInheritHandle=*/FALSE, parent_id));
  while (parent_process_handle.is_valid()) {
    base::FilePath parent_path =
        GetProcessExecutablePath(parent_process_handle.get());
    if (parent_path.empty()) {
      return main_process_id_candidate;
    }

    // If the executables are different, we return the last PID whose executable
    // was the same as `app_path`
    if (!base::FilePath::CompareEqualIgnoreCase(parent_path.value(),
                                                app_path.value())) {
      return main_process_id_candidate;
    }

    main_process_id_candidate = parent_id;
    parent_id = base::GetParentProcessId(parent_process_handle.Get());
    parent_process_handle.Set(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                          /*bInheritHandle=*/FALSE, parent_id));
  }

  return main_process_id_candidate;
}
