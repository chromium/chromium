// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_STRING_SPLIT_INTERNAL_H_
#define BASE_STRINGS_STRING_SPLIT_INTERNAL_H_

#include <vector>

#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"

namespace base {

namespace internal {

// Returns either the ASCII or UTF-16 whitespace.
template <typename CharT>
BasicStringPiece<CharT> WhitespaceForType();

template <>
inline StringPiece16 WhitespaceForType<char16_t>() {
  return kWhitespaceUTF16;
}
template <>
inline StringPiece WhitespaceForType<char>() {
  return kWhitespaceASCII;
}

// General string splitter template. Can take 8- or 16-bit input, can produce
// the corresponding string or StringPiece output.
template <typename OutputStringType,
          typename T,
          typename CharT = typename T::value_type>
static std::vector<OutputStringType> SplitStringT(T str,
                                                  T delimiter,
                                                  WhitespaceHandling whitespace,
                                                  SplitResult result_type) {
  std::vector<OutputStringType> result;
  if (str.empty())
    return result;

  size_t start = 0;
  while (start != std::basic_string<CharT>::npos) {
    size_t end = str.find_first_of(delimiter, start);

    BasicStringPiece<CharT> piece;
    if (end == std::basic_string<CharT>::npos) {
      piece = str.substr(start);
      start = std::basic_string<CharT>::npos;
    } else {
      piece = str.substr(start, end - start);
      start = end + 1;
    }

    if (whitespace == TRIM_WHITESPACE)
      piece = TrimString(piece, WhitespaceForType<CharT>(), TRIM_ALL);

    if (result_type == SPLIT_WANT_ALL || !piece.empty())
      result.emplace_back(piece);
  }
  return result;
}

template <typename OutputStringType,
          typename T,
          typename CharT = typename T::value_type>
std::vector<OutputStringType> SplitStringUsingSubstrT(
    T input,
    T delimiter,
    WhitespaceHandling whitespace,
    SplitResult result_type) {
  using Piece = BasicStringPiece<CharT>;
  using size_type = typename Piece::size_type;

  std::vector<OutputStringType> result;
  if (delimiter.size() == 0) {
    result.emplace_back(input);
    return result;
  }

  for (size_type begin_index = 0, end_index = 0; end_index != Piece::npos;
       begin_index = end_index + delimiter.size()) {
    end_index = input.find(delimiter, begin_index);
    Piece term = end_index == Piece::npos
                     ? input.substr(begin_index)
                     : input.substr(begin_index, end_index - begin_index);

    if (whitespace == TRIM_WHITESPACE)
      term = TrimString(term, WhitespaceForType<CharT>(), TRIM_ALL);

    if (result_type == SPLIT_WANT_ALL || !term.empty())
      result.emplace_back(term);
  }

  return result;
}

}  // namespace internal

}  // namespace base

#endif  // BASE_STRINGS_STRING_SPLIT_INTERNAL_H_
