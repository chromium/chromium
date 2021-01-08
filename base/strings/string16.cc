// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string16.h"

#include <string>

#include "base/strings/utf_string_conversions.h"

#if defined(WCHAR_T_IS_UTF32)
std::ostream& std::operator<<(std::ostream& out, const std::u16string& str16) {
  return out << base::UTF16ToUTF8(str16);
}
#endif

namespace base {

int c16memcmp(const char16* s1, const char16* s2, size_t n) {
  return std::char_traits<char16>::compare(s1, s2, n);
}

size_t c16len(const char16* s) {
  return std::char_traits<char16>::length(s);
}

char16* c16memcpy(char16* s1, const char16* s2, size_t n) {
  return std::char_traits<char16>::copy(s1, s2, n);
}

}  // namespace base
