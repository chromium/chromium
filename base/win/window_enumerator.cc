// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/window_enumerator.h"

#include <windows.h>

#include <string>

#include "base/functional/callback.h"

namespace base::win {

WindowEnumerator::WindowEnumerator(
    HWND parent,
    base::RepeatingCallback<bool(HWND hwnd)> filter)
    : parent_(parent), filter_(filter) {}
WindowEnumerator::~WindowEnumerator() = default;

void WindowEnumerator::Run() const {
  ::EnumChildWindows(parent_, &OnWindowProc, reinterpret_cast<LPARAM>(this));
}

// static
bool WindowEnumerator::IsTopmostWindow(HWND hwnd) {
  return ::GetWindowLong(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST;
}

// static
bool WindowEnumerator::IsSystemDialog(HWND hwnd) {
  constexpr wchar_t kSystemDialogClass[] = L"#32770";
  return GetWindowClass(hwnd) == kSystemDialogClass;
}

// static
bool WindowEnumerator::IsShellWindow(HWND hwnd) {
  const std::wstring class_name = GetWindowClass(hwnd);

  // 'Button' is the start button, 'Shell_TrayWnd' the taskbar, and
  // 'Shell_SecondaryTrayWnd' is the taskbar on non-primary displays.
  return class_name == L"Button" || class_name == L"Shell_TrayWnd" ||
         class_name == L"Shell_SecondaryTrayWnd";
}

// static
std::wstring WindowEnumerator::GetWindowClass(HWND hwnd) {
  constexpr int kMaxWindowClassNameLength = 256;
  std::wstring window_class(kMaxWindowClassNameLength, L'\0');
  const int name_len =
      ::GetClassName(hwnd, &window_class[0], kMaxWindowClassNameLength);
  if (name_len <= 0 || name_len > kMaxWindowClassNameLength) {
    return {};
  }
  window_class.resize(static_cast<size_t>(name_len));
  return window_class;
}

// static
std::wstring WindowEnumerator::GetWindowText(HWND hwnd) {
  int num_chars = ::GetWindowTextLength(hwnd);
  if (!num_chars) {
    return {};
  }
  std::wstring text(static_cast<size_t>(++num_chars), L'\0');
  return ::GetWindowText(hwnd, &text[0], num_chars) ? text : std::wstring();
}

bool WindowEnumerator::OnWindow(HWND hwnd) const {
  return !filter_.Run(hwnd);
}

// static
BOOL CALLBACK WindowEnumerator::OnWindowProc(HWND hwnd, LPARAM lparam) {
  return reinterpret_cast<WindowEnumerator*>(lparam)->OnWindow(hwnd);
}

}  // namespace base::win
