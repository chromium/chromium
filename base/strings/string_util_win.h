// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_STRING_UTIL_WIN_H_
#define BASE_STRINGS_STRING_UTIL_WIN_H_

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "base/logging.h"

namespace base {

// Chromium code style is to not use malloc'd strings; this is only for use
// for interaction with APIs that require it.
inline char* strdup(const char* str) {
  return _strdup(str);
}

inline int vsnprintf(char* buffer, size_t size,
                     const char* format, va_list arguments) {
  int length = vsnprintf_s(buffer, size, size - 1, format, arguments);
  if (length < 0)
    return _vscprintf(format, arguments);
  return length;
}

inline int vswprintf(wchar_t* buffer, size_t size,
                     const wchar_t* format, va_list arguments) {
  DCHECK(IsWprintfFormatPortable(format));

  int length = _vsnwprintf_s(buffer, size, size - 1, format, arguments);
  if (length < 0)
    return _vscwprintf(format, arguments);
  return length;
}

// Windows only overload of base::WriteInto for std::wstring. See the comment
// above the cross-platform version in //base/strings/string_util.h for details.
// TODO(crbug.com/911896): Rename this to WriteInto once base::string16 is
// std::u16string on all platforms and using the name WriteInto here no longer
// causes redefinition errors.
inline wchar_t* WriteIntoW(std::wstring* str, size_t length_with_null) {
  // Note: As of C++11 std::strings are guaranteed to be 0-terminated. Thus it
  // is enough to reserve space for one char less.
  DCHECK_GE(length_with_null, 1u);
  str->resize(length_with_null - 1);
  return &((*str)[0]);
}

}  // namespace base

#endif  // BASE_STRINGS_STRING_UTIL_WIN_H_
