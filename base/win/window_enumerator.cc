// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/window_enumerator.h"

#include <windows.h>

#include <string>

#include "base/functional/callback.h"

namespace base::win {

namespace {

BOOL CALLBACK OnWindowProc(HWND hwnd, LPARAM lparam) {
  return !reinterpret_cast<WindowEnumeratorCallback*>(lparam)->Run(hwnd);
}

}  // namespace

void EnumerateChildWindows(HWND parent, WindowEnumeratorCallback filter) {
  ::EnumChildWindows(parent, &OnWindowProc, reinterpret_cast<LPARAM>(&filter));
}

bool IsTopmostWindow(HWND hwnd) {
  return ::GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST;
}

bool IsSystemDialog(HWND hwnd) {
  static constexpr wchar_t kSystemDialogClass[] = L"#32770";
  return GetWindowClass(hwnd) == kSystemDialogClass;
}

bool IsShellWindow(HWND hwnd) {
  const std::wstring class_name = GetWindowClass(hwnd);

  // 'Button' is the start button, 'Shell_TrayWnd' the taskbar, and
  // 'Shell_SecondaryTrayWnd' is the taskbar on non-primary displays.
  return class_name == L"Button" || class_name == L"Shell_TrayWnd" ||
         class_name == L"Shell_SecondaryTrayWnd";
}

std::wstring GetWindowClass(HWND hwnd) {
  constexpr int kMaxWindowClassNameLength = 256;
  std::wstring window_class(kMaxWindowClassNameLength, L'\0');
  const int name_len =
      ::GetClassName(hwnd, window_class.data(), kMaxWindowClassNameLength);
  if (name_len <= 0 || name_len > kMaxWindowClassNameLength) {
    return {};
  }
  window_class.resize(static_cast<size_t>(name_len));
  return window_class;
}

std::wstring GetWindowTextString(HWND hwnd) {
  auto num_chars = ::GetWindowTextLength(hwnd);
  if (num_chars <= 0) {
    return {};
  }
  std::wstring text(static_cast<size_t>(num_chars), L'\0');
  // MSDN says that GetWindowText will not write anything other than a string
  // terminator to the last position in the buffer.
  auto len = ::GetWindowText(hwnd, text.data(), num_chars + 1);
  if (len <= 0) {
    return std::wstring();
  }
  // GetWindowText may return a shorter string than reported above.
  text.resize(static_cast<size_t>(len));
  return text;
}

}  // namespace base::win
