// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_STRING_VIEW_UTIL_H_
#define BASE_STRINGS_STRING_VIEW_UTIL_H_

#include <string_view>

#include "base/compiler_specific.h"

namespace base {

// Helper function for creating a std::string_view from a string literal that
// preserves internal NUL characters.
template <class CharT, size_t N>
std::basic_string_view<CharT> MakeStringViewWithNulChars(
    const CharT (&lit LIFETIME_BOUND)[N])
    ENABLE_IF_ATTR(lit[N - 1u] == CharT{0},
                   "requires string literal as input") {
  return std::basic_string_view<CharT>(lit, N - 1u);
}

}  // namespace base

#endif  // BASE_STRINGS_STRING_VIEW_UTIL_H_
