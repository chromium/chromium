// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_STRING_VIEW_UTIL_H_
#define BASE_STRINGS_STRING_VIEW_UTIL_H_

#include <string_view>

#include "base/compiler_specific.h"
#include "base/containers/span.h"

namespace base {

// Helper function for creating a std::string_view from a string literal that
// preserves internal NUL characters.
template <class CharT, size_t N>
constexpr std::basic_string_view<CharT> MakeStringViewWithNulChars(
    const CharT (&lit LIFETIME_BOUND)[N])
    ENABLE_IF_ATTR(lit[N - 1u] == CharT{0},
                   "requires string literal as input") {
  // SAFETY: length of string literal is deduced by the compiler.
  return UNSAFE_BUFFERS(std::basic_string_view<CharT>(lit, N - 1u));
}

// Converts a span over byte-like elements to `std::string_view`.
//
// std:: has no direct equivalent for this; however, it eases span adoption in
// Chromium, which uses `string`s and `string_view`s in many cases that
// rightfully should be containers of `uint8_t`.
//
// TODO(C++23): Replace with direct use of the `std::string_view` range
// constructor to avoid marking UNSAFE_BUFFERS.
constexpr auto as_string_view(span<const char> s) {
  // SAFETY: legitimate spans are valid from `begin()` to `end()`.
  return UNSAFE_BUFFERS(std::string_view(s.begin(), s.end()));
}
constexpr auto as_string_view(span<const unsigned char> s) {
  return as_string_view(as_chars(s));
}
constexpr auto as_string_view(span<const char16_t> s) {
  // SAFETY: legitimate spans are valid from `begin()` to `end()`.
  return UNSAFE_BUFFERS(std::u16string_view(s.begin(), s.end()));
}
constexpr auto as_string_view(span<const wchar_t> s) {
  // SAFETY: legitimate spans are valid from `begin()` to `end()`.
  return UNSAFE_BUFFERS(std::wstring_view(s.begin(), s.end()));
}

}  // namespace base

#endif  // BASE_STRINGS_STRING_VIEW_UTIL_H_
