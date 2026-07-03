// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_STRING_SPLIT_INTERNAL_H_
#define BASE_STRINGS_STRING_SPLIT_INTERNAL_H_

#include <string_view>
#include <vector>

#include "base/strings/trim_string_internal.h"
#include "base/strings/whitespace_constants.h"

namespace base::internal {

// Returns either the ASCII or UTF-16 whitespace.
template <typename CharT>
constexpr std::basic_string_view<CharT> WhitespaceForType();

template <>
inline constexpr std::u16string_view WhitespaceForType<char16_t>() {
  return kWhitespaceUTF16;
}
template <>
inline constexpr std::string_view WhitespaceForType<char>() {
  return kWhitespaceASCII;
}

// General string splitter template. Can take 8- or 16-bit input, can
// produce the corresponding string or std::string_view output.
template <typename OutputStringType,
          typename T,
          typename CharT = typename T::value_type>
constexpr std::vector<OutputStringType> SplitStringT(
    T str,
    T delimiter,
    WhitespaceHandling whitespace,
    SplitResult result_type) {
  std::vector<OutputStringType> result;
  if (str.empty()) {
    return result;
  }

  size_t start = 0;
  while (start != std::basic_string<CharT>::npos) {
    size_t end = str.find_first_of(delimiter, start);

    std::basic_string_view<CharT> piece;
    if (end == std::basic_string<CharT>::npos) {
      piece = str.substr(start);
      start = std::basic_string<CharT>::npos;
    } else {
      piece = str.substr(start, end - start);
      start = end + 1;
    }

    if (whitespace == TRIM_WHITESPACE) {
      piece = TrimStringPieceT<std::basic_string_view<CharT>>(
          piece, WhitespaceForType<CharT>(), true, true);
    }

    if (result_type == SPLIT_WANT_ALL || !piece.empty()) {
      result.emplace_back(piece);
    }
  }
  return result;
}

template <typename OutputStringType,
          typename T,
          typename CharT = typename T::value_type>
constexpr std::vector<OutputStringType> SplitStringUsingSubstrT(
    T input,
    T delimiter,
    WhitespaceHandling whitespace,
    SplitResult result_type) {
  using Piece = std::basic_string_view<CharT>;
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

    if (whitespace == TRIM_WHITESPACE) {
      term =
          TrimStringPieceT<Piece>(term, WhitespaceForType<CharT>(), true, true);
    }

    if (result_type == SPLIT_WANT_ALL || !term.empty()) {
      result.emplace_back(term);
    }
  }

  return result;
}

}  // namespace base::internal

#endif  // BASE_STRINGS_STRING_SPLIT_INTERNAL_H_
