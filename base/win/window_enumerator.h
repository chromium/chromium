// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_WINDOW_ENUMERATOR_H_
#define BASE_WIN_WINDOW_ENUMERATOR_H_

#include <windows.h>

#include <string>

#include "base/base_export.h"
#include "base/functional/callback.h"

namespace base::win {

// Enumerates immediate child windows of `parent`, and calls `filter.Run` for
// each window:
// * If `filter.Run` returns `false`, continues enumerating.
// * If `filter.Run` returns `true`, stops enumerating.
class BASE_EXPORT WindowEnumerator {
 public:
  WindowEnumerator(HWND parent,
                   base::RepeatingCallback<bool(HWND hwnd)> filter);
  WindowEnumerator(const WindowEnumerator&) = delete;
  WindowEnumerator& operator=(const WindowEnumerator&) = delete;
  ~WindowEnumerator();

  void Run() const;

  // Returns true if `hwnd` is an always-on-top window.
  static bool IsTopmostWindow(HWND hwnd);

  // Returns true if `hwnd` is a system dialog.
  static bool IsSystemDialog(HWND hwnd);

  // Returns true if `hwnd` is a window owned by the Windows shell.
  static bool IsShellWindow(HWND hwnd);

  // Returns the class name of `hwnd` or an empty string on error.
  static std::wstring GetWindowClass(HWND hwnd);

  // Returns the window text for `hwnd`, or an empty string on error.
  static std::wstring GetWindowText(HWND hwnd);

 private:
  // Main processing function run for each window.
  bool OnWindow(HWND hwnd) const;

  // An EnumWindowsProc invoked by EnumWindows once for each window.
  static BOOL CALLBACK OnWindowProc(HWND hwnd, LPARAM lparam);

  const HWND parent_;
  base::RepeatingCallback<bool(HWND hwnd)> filter_;
};

}  // namespace base::win

#endif  // BASE_WIN_WINDOW_ENUMERATOR_H_
