// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_split.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/strings/string_split_internal.h"
#include "base/strings/string_util.h"
#include "base/third_party/icu/icu_utf.h"

namespace base {

namespace {

bool AppendStringKeyValue(StringPiece input,
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
  StringPiece remains = input.substr(end_key_pos, input.size() - end_key_pos);
  size_t begin_value_pos = remains.find_first_not_of(delimiter);
  if (begin_value_pos == StringPiece::npos) {
    DVLOG(1) << "cannot parse value from input: " << input;
    return false;   // No value.
  }

  result_pair.second = std::string(
      remains.substr(begin_value_pos, remains.size() - begin_value_pos));

  return true;
}

}  // namespace

std::vector<std::string> SplitString(StringPiece input,
                                     StringPiece separators,
                                     WhitespaceHandling whitespace,
                                     SplitResult result_type) {
  return internal::SplitStringT<std::string>(input, separators, whitespace,
                                             result_type);
}

std::vector<std::u16string> SplitString(StringPiece16 input,
                                        StringPiece16 separators,
                                        WhitespaceHandling whitespace,
                                        SplitResult result_type) {
  return internal::SplitStringT<std::u16string>(input, separators, whitespace,
                                                result_type);
}

std::vector<StringPiece> SplitStringPiece(StringPiece input,
                                          StringPiece separators,
                                          WhitespaceHandling whitespace,
                                          SplitResult result_type) {
  return internal::SplitStringT<StringPiece>(input, separators, whitespace,
                                             result_type);
}

std::vector<StringPiece16> SplitStringPiece(StringPiece16 input,
                                            StringPiece16 separators,
                                            WhitespaceHandling whitespace,
                                            SplitResult result_type) {
  return internal::SplitStringT<StringPiece16>(input, separators, whitespace,
                                               result_type);
}

bool SplitStringIntoKeyValuePairs(StringPiece input,
                                  char key_value_delimiter,
                                  char key_value_pair_delimiter,
                                  StringPairs* key_value_pairs) {
  return SplitStringIntoKeyValuePairsUsingSubstr(
      input, key_value_delimiter, StringPiece(&key_value_pair_delimiter, 1),
      key_value_pairs);
}

bool SplitStringIntoKeyValuePairsUsingSubstr(
    StringPiece input,
    char key_value_delimiter,
    StringPiece key_value_pair_delimiter,
    StringPairs* key_value_pairs) {
  key_value_pairs->clear();

  std::vector<StringPiece> pairs = SplitStringPieceUsingSubstr(
      input, key_value_pair_delimiter, TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
  key_value_pairs->reserve(pairs.size());

  bool success = true;
  for (const StringPiece& pair : pairs) {
    if (!AppendStringKeyValue(pair, key_value_delimiter, key_value_pairs)) {
      // Don't return here, to allow for pairs without associated
      // value or key; just record that the split failed.
      success = false;
    }
  }
  return success;
}

std::vector<std::u16string> SplitStringUsingSubstr(
    StringPiece16 input,
    StringPiece16 delimiter,
    WhitespaceHandling whitespace,
    SplitResult result_type) {
  return internal::SplitStringUsingSubstrT<std::u16string>(
      input, delimiter, whitespace, result_type);
}

std::vector<std::string> SplitStringUsingSubstr(StringPiece input,
                                                StringPiece delimiter,
                                                WhitespaceHandling whitespace,
                                                SplitResult result_type) {
  return internal::SplitStringUsingSubstrT<std::string>(
      input, delimiter, whitespace, result_type);
}

std::vector<StringPiece16> SplitStringPieceUsingSubstr(
    StringPiece16 input,
    StringPiece16 delimiter,
    WhitespaceHandling whitespace,
    SplitResult result_type) {
  std::vector<StringPiece16> result;
  return internal::SplitStringUsingSubstrT<StringPiece16>(
      input, delimiter, whitespace, result_type);
}

std::vector<StringPiece> SplitStringPieceUsingSubstr(
    StringPiece input,
    StringPiece delimiter,
    WhitespaceHandling whitespace,
    SplitResult result_type) {
  return internal::SplitStringUsingSubstrT<StringPiece>(
      input, delimiter, whitespace, result_type);
}

}  // namespace base
