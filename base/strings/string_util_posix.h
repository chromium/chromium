// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_STRING_UTIL_POSIX_H_
#define BASE_STRINGS_STRING_UTIL_POSIX_H_

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "base/check.h"
#include "base/compiler_specific.h"

namespace base {

// Chromium code style is to not use malloc'd strings; this is only for use
// for interaction with APIs that require it.
inline char* strdup(const char* str) {
  return UNSAFE_TODO(::strdup(str));
}

inline int vsnprintf(char* buffer,
                     size_t size,
                     const char* format,
                     va_list arguments) {
  return UNSAFE_TODO(::vsnprintf(buffer, size, format, arguments));
}

// TODO(crbug.com/40284755): implement spanified version, or just remove
// this entirely as it has ~no non-test uses.
// inline int vswprintf(base::span<wchar_t> buffer,
//                      const wchar_t* format,
//                      va_list arguments);
inline int vswprintf(wchar_t* buffer,
                     size_t size,
                     const wchar_t* format,
                     va_list arguments) {
  DCHECK(IsWprintfFormatPortable(format));
  return UNSAFE_TODO(::vswprintf(buffer, size, format, arguments));
}

}  // namespace base

#endif  // BASE_STRINGS_STRING_UTIL_POSIX_H_
