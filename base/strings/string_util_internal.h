// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_STRING_UTIL_INTERNAL_H_
#define BASE_STRINGS_STRING_UTIL_INTERNAL_H_

#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"

namespace base::internal {

// ASCII-specific tolower.  The standard library's tolower is locale sensitive,
// so we don't want to use it here.
template <typename CharT,
          typename = std::enable_if_t<std::is_integral<CharT>::value>>
constexpr CharT ToLowerASCII(CharT c) {
  return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

template <typename T, typename CharT = typename T::value_type>
constexpr int CompareCaseInsensitiveASCIIT(T a, T b) {
  // Find the first characters that aren't equal and compare them.  If the end
  // of one of the strings is found before a nonequal character, the lengths
  // of the strings are compared.
  size_t i = 0;
  while (i < a.length() && i < b.length()) {
    CharT lower_a = ToLowerASCII(a[i]);
    CharT lower_b = ToLowerASCII(b[i]);
    if (lower_a < lower_b)
      return -1;
    if (lower_a > lower_b)
      return 1;
    i++;
  }

  // End of one string hit before finding a different character. Expect the
  // common case to be "strings equal" at this point so check that first.
  if (a.length() == b.length())
    return 0;

  if (a.length() < b.length())
    return -1;
  return 1;
}

template <typename CharT, typename CharU>
inline bool EqualsCaseInsensitiveASCIIT(BasicStringPiece<CharT> a,
                                        BasicStringPiece<CharU> b) {
  return ranges::equal(a, b, [](auto lhs, auto rhs) {
    return ToLowerASCII(lhs) == ToLowerASCII(rhs);
  });
}

}  // namespace base::internal

#endif  // BASE_STRINGS_STRING_UTIL_INTERNAL_H_
