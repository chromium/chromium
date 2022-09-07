// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_split_win.h"

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/strings/string_split_internal.h"

namespace base {

namespace internal {

template <>
inline WStringPiece WhitespaceForType<wchar_t>() {
  return kWhitespaceWide;
}

}  // namespace internal

std::vector<std::wstring> SplitString(WStringPiece input,
                                      WStringPiece separators,
                                      WhitespaceHandling whitespace,
                                      SplitResult result_type) {
  return internal::SplitStringT<std::wstring>(input, separators, whitespace,
                                              result_type);
}

std::vector<WStringPiece> SplitStringPiece(WStringPiece input,
                                           WStringPiece separators,
                                           WhitespaceHandling whitespace,
                                           SplitResult result_type) {
  return internal::SplitStringT<WStringPiece>(input, separators, whitespace,
                                              result_type);
}

std::vector<std::wstring> SplitStringUsingSubstr(WStringPiece input,
                                                 WStringPiece delimiter,
                                                 WhitespaceHandling whitespace,
                                                 SplitResult result_type) {
  return internal::SplitStringUsingSubstrT<std::wstring>(
      input, delimiter, whitespace, result_type);
}

std::vector<WStringPiece> SplitStringPieceUsingSubstr(
    WStringPiece input,
    WStringPiece delimiter,
    WhitespaceHandling whitespace,
    SplitResult result_type) {
  return internal::SplitStringUsingSubstrT<WStringPiece>(
      input, delimiter, whitespace, result_type);
}

}  // namespace base
