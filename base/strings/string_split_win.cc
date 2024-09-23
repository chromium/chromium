// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_split_win.h"

#include <string>
#include <string_view>
#include <vector>

#include "base/strings/string_split_internal.h"

namespace base {

namespace internal {

template <>
inline std::wstring_view WhitespaceForType<wchar_t>() {
  return kWhitespaceWide;
}

}  // namespace internal

std::vector<std::wstring> SplitString(std::wstring_view input,
                                      std::wstring_view separators,
                                      WhitespaceHandling whitespace,
                                      SplitResult result_type) {
  return internal::SplitStringT<std::wstring>(input, separators, whitespace,
                                              result_type);
}

std::vector<std::wstring_view> SplitStringPiece(std::wstring_view input,
                                                std::wstring_view separators,
                                                WhitespaceHandling whitespace,
                                                SplitResult result_type) {
  return internal::SplitStringT<std::wstring_view>(input, separators,
                                                   whitespace, result_type);
}

std::vector<std::wstring> SplitStringUsingSubstr(std::wstring_view input,
                                                 std::wstring_view delimiter,
                                                 WhitespaceHandling whitespace,
                                                 SplitResult result_type) {
  return internal::SplitStringUsingSubstrT<std::wstring>(
      input, delimiter, whitespace, result_type);
}

std::vector<std::wstring_view> SplitStringPieceUsingSubstr(
    std::wstring_view input,
    std::wstring_view delimiter,
    WhitespaceHandling whitespace,
    SplitResult result_type) {
  return internal::SplitStringUsingSubstrT<std::wstring_view>(
      input, delimiter, whitespace, result_type);
}

}  // namespace base
