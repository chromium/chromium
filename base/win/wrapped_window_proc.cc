// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/wrapped_window_proc.h"

#include <atomic>

#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"

namespace {

std::atomic<base::win::WinProcExceptionFilter> s_exception_filter = nullptr;
static_assert(
    std::atomic<base::win::WinProcExceptionFilter>::is_always_lock_free,
    "");

HMODULE GetModuleFromWndProc(WNDPROC window_proc) {
  HMODULE instance = nullptr;
  // Converting a pointer-to-function to a void* is undefined behavior, but
  // Windows (and POSIX) APIs require it to work.
  void* address = reinterpret_cast<void*>(window_proc);
  if (!::GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            static_cast<char*>(address), &instance)) {
    NOTREACHED();
  }
  return instance;
}

}  // namespace.

namespace base {
namespace win {

WinProcExceptionFilter SetWinProcExceptionFilter(
    WinProcExceptionFilter filter) {
  return s_exception_filter.exchange(filter, std::memory_order_relaxed);
}

int CallExceptionFilter(EXCEPTION_POINTERS* info) {
  base::win::WinProcExceptionFilter filter =
      s_exception_filter.load(std::memory_order_relaxed);
  return filter ? filter(info) : EXCEPTION_CONTINUE_SEARCH;
}

BASE_EXPORT void InitializeWindowClass(const wchar_t* class_name,
                                       WNDPROC window_proc,
                                       UINT style,
                                       int class_extra,
                                       int window_extra,
                                       HCURSOR cursor,
                                       HBRUSH background,
                                       const wchar_t* menu_name,
                                       HICON large_icon,
                                       HICON small_icon,
                                       WNDCLASSEX* class_out) {
  class_out->cbSize = sizeof(WNDCLASSEX);
  class_out->style = style;
  class_out->lpfnWndProc = window_proc;
  class_out->cbClsExtra = class_extra;
  class_out->cbWndExtra = window_extra;
  // RegisterClassEx uses a handle of the module containing the window procedure
  // to distinguish identically named classes registered in different modules.
  class_out->hInstance = GetModuleFromWndProc(window_proc);
  class_out->hIcon = large_icon;
  class_out->hCursor = cursor;
  class_out->hbrBackground = background;
  class_out->lpszMenuName = menu_name;
  class_out->lpszClassName = class_name;
  class_out->hIconSm = small_icon;

  // Check if |window_proc| is valid.
  DCHECK(class_out->hInstance != nullptr);
}

}  // namespace win
}  // namespace base
