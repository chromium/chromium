// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_split.h"

#include <stddef.h>

#include <string_view>

#include "base/logging.h"
#include "base/strings/string_split_internal.h"
#include "base/strings/string_util.h"
#include "base/third_party/icu/icu_utf.h"

namespace base {

namespace {

// Helper for the various *SplitStringOnce implementations. When returning a
// pair of `std::string_view`, does not include the character at `position`.
std::optional<std::pair<std::string_view, std::string_view>>
SplitStringAtExclusive(std::string_view input, size_t position) {
  if (position == std::string_view::npos) {
    return std::nullopt;
  }

  return std::pair(input.substr(0, position), input.substr(position + 1));
}

bool AppendStringKeyValue(std::string_view input,
                          char delimiter,
                          StringPairs* result) {
  // Always append a new item regardless of success (it might be empty). The
  // below code will copy the strings directly into the result pair.
  result->resize(result->size() + 1);
  auto& result_pair = result->back();

  // Find the delimiter.
  size_t end_key_pos = input.find_first_of(delimiter);
  if (end_key_pos == std::string::npos) {
    DVLOG(1) << "cannot find delimiter in: " << input;
    return false;    // No delimiter.
  }
  result_pair.first = std::string(input.substr(0, end_key_pos));

  // Find the value string.
  std::string_view remains =
      input.substr(end_key_pos, input.size() - end_key_pos);
  size_t begin_value_pos = remains.find_first_not_of(delimiter);
  if (begin_value_pos == std::string_view::npos) {
    DVLOG(1) << "cannot parse value from input: " << input;
    return false;   // No value.
  }

  result_pair.second = std::string(
      remains.substr(begin_value_pos, remains.size() - begin_value_pos));

  return true;
}

}  // namespace

std::optional<std::pair<std::string_view, std::string_view>> SplitStringOnce(
    std::string_view input,
    char separator) {
  return SplitStringAtExclusive(input, input.find(separator));
}

std::optional<std::pair<std::string_view, std::string_view>> SplitStringOnce(
    std::string_view input,
    std::string_view separators) {
  return SplitStringAtExclusive(input, input.find_first_of(separators));
}

std::optional<std::pair<std::string_view, std::string_view>> RSplitStringOnce(
    std::string_view input,
    char separator) {
  return SplitStringAtExclusive(input, input.rfind(separator));
}

std::optional<std::pair<std::string_view, std::string_view>> RSplitStringOnce(
    std::string_view input,
    std::string_view separators) {
  return SplitStringAtExclusive(input, input.find_last_of(separators));
}

std::vector<std::string> SplitString(std::string_view input,
                                     std::string_view separators,
                                     WhitespaceHandling whitespace,
                                     SplitResult result_type) {
  return internal::SplitStringT<std::string>(input, separators, whitespace,
                                             result_type);
}

std::vector<std::u16string> SplitString(std::u16string_view input,
                                        std::u16string_view separators,
                                        WhitespaceHandling whitespace,
                                        SplitResult result_type) {
  return internal::SplitStringT<std::u16string>(input, separators, whitespace,
                                                result_type);
}

std::vector<std::string_view> SplitStringPiece(std::string_view input,
                                               std::string_view separators,
                                               WhitespaceHandling whitespace,
                                               SplitResult result_type) {
  return internal::SplitStringT<std::string_view>(input, separators, whitespace,
                                                  result_type);
}

std::vector<std::u16string_view> SplitStringPiece(
    std::u16string_view input,
    std::u16string_view separators,
    WhitespaceHandling whitespace,
    SplitResult result_type) {
  return internal::SplitStringT<std::u16string_view>(input, separators,
                                                     whitespace, result_type);
}

bool SplitStringIntoKeyValuePairs(std::string_view input,
                                  char key_value_delimiter,
                                  char key_value_pair_delimiter,
                                  StringPairs* key_value_pairs) {
  return SplitStringIntoKeyValuePairsUsingSubstr(
      input, key_value_delimiter,
      std::string_view(&key_value_pair_delimiter, 1), key_value_pairs);
}

bool SplitStringIntoKeyValuePairsUsingSubstr(
    std::string_view input,
    char key_value_delimiter,
    std::string_view key_value_pair_delimiter,
    StringPairs* key_value_pairs) {
  key_value_pairs->clear();

  std::vector<std::string_view> pairs = SplitStringPieceUsingSubstr(
      input, key_value_pair_delimiter, TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
  key_value_pairs->reserve(pairs.size());

  bool success = true;
  for (std::string_view pair : pairs) {
    if (!AppendStringKeyValue(pair, key_value_delimiter, key_value_pairs)) {
      // Don't return here, to allow for pairs without associated
      // value or key; just record that the split failed.
      success = false;
    }
  }
  return success;
}

std::vector<std::u16string> SplitStringUsingSubstr(
    std::u16string_view input,
    std::u16string_view delimiter,
    WhitespaceHandling whitespace,
    SplitResult result_type) {
  return internal::SplitStringUsingSubstrT<std::u16string>(
      input, delimiter, whitespace, result_type);
}

std::vector<std::string> SplitStringUsingSubstr(std::string_view input,
                                                std::string_view delimiter,
                                                WhitespaceHandling whitespace,
                                                SplitResult result_type) {
  return internal::SplitStringUsingSubstrT<std::string>(
      input, delimiter, whitespace, result_type);
}

std::vector<std::u16string_view> SplitStringPieceUsingSubstr(
    std::u16string_view input,
    std::u16string_view delimiter,
    WhitespaceHandling whitespace,
    SplitResult result_type) {
  std::vector<std::u16string_view> result;
  return internal::SplitStringUsingSubstrT<std::u16string_view>(
      input, delimiter, whitespace, result_type);
}

std::vector<std::string_view> SplitStringPieceUsingSubstr(
    std::string_view input,
    std::string_view delimiter,
    WhitespaceHandling whitespace,
    SplitResult result_type) {
  return internal::SplitStringUsingSubstrT<std::string_view>(
      input, delimiter, whitespace, result_type);
}

}  // namespace base
