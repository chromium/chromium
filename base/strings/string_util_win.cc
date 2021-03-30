// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util_win.h"

#include "base/strings/string_util_internal.h"

namespace base {

bool IsStringASCII(WStringPiece str) {
  return internal::DoIsStringASCII(str.data(), str.length());
}

std::wstring ToLowerASCII(WStringPiece str) {
  return internal::ToLowerASCIIImpl(str);
}

std::wstring ToUpperASCII(WStringPiece str) {
  return internal::ToUpperASCIIImpl(str);
}

int CompareCaseInsensitiveASCII(WStringPiece a, WStringPiece b) {
  return internal::CompareCaseInsensitiveASCIIT(a, b);
}

bool EqualsCaseInsensitiveASCII(WStringPiece a, WStringPiece b) {
  return a.size() == b.size() &&
         internal::CompareCaseInsensitiveASCIIT(a, b) == 0;
}

bool RemoveChars(WStringPiece input,
                 WStringPiece remove_chars,
                 std::wstring* output) {
  return internal::ReplaceCharsT(input, remove_chars, WStringPiece(), output);
}

bool ReplaceChars(WStringPiece input,
                  WStringPiece replace_chars,
                  WStringPiece replace_with,
                  std::wstring* output) {
  return internal::ReplaceCharsT(input, replace_chars, replace_with, output);
}

bool TrimString(WStringPiece input,
                WStringPiece trim_chars,
                std::wstring* output) {
  return internal::TrimStringT(input, trim_chars, TRIM_ALL, output) !=
         TRIM_NONE;
}

WStringPiece TrimString(WStringPiece input,
                        WStringPiece trim_chars,
                        TrimPositions positions) {
  return internal::TrimStringPieceT(input, trim_chars, positions);
}

TrimPositions TrimWhitespace(WStringPiece input,
                             TrimPositions positions,
                             std::wstring* output) {
  return internal::TrimStringT(input, WStringPiece(kWhitespaceWide), positions,
                               output);
}

WStringPiece TrimWhitespace(WStringPiece input, TrimPositions positions) {
  return internal::TrimStringPieceT(input, WStringPiece(kWhitespaceWide),
                                    positions);
}

std::wstring CollapseWhitespace(WStringPiece text,
                                bool trim_sequences_with_line_breaks) {
  return internal::CollapseWhitespaceT(text, trim_sequences_with_line_breaks);
}

bool ContainsOnlyChars(WStringPiece input, WStringPiece characters) {
  return input.find_first_not_of(characters) == StringPiece::npos;
}

bool LowerCaseEqualsASCII(WStringPiece str, StringPiece lowercase_ascii) {
  return internal::DoLowerCaseEqualsASCII(str, lowercase_ascii);
}

bool EqualsASCII(WStringPiece str, StringPiece ascii) {
  return std::equal(ascii.begin(), ascii.end(), str.begin(), str.end());
}

bool StartsWith(WStringPiece str,
                WStringPiece search_for,
                CompareCase case_sensitivity) {
  return internal::StartsWithT(str, search_for, case_sensitivity);
}

bool EndsWith(WStringPiece str,
              WStringPiece search_for,
              CompareCase case_sensitivity) {
  return internal::EndsWithT(str, search_for, case_sensitivity);
}

void ReplaceFirstSubstringAfterOffset(std::wstring* str,
                                      size_t start_offset,
                                      WStringPiece find_this,
                                      WStringPiece replace_with) {
  internal::DoReplaceMatchesAfterOffset(
      str, start_offset, internal::MakeSubstringMatcher(find_this),
      replace_with, internal::ReplaceType::REPLACE_FIRST);
}

void ReplaceSubstringsAfterOffset(std::wstring* str,
                                  size_t start_offset,
                                  WStringPiece find_this,
                                  WStringPiece replace_with) {
  internal::DoReplaceMatchesAfterOffset(
      str, start_offset, internal::MakeSubstringMatcher(find_this),
      replace_with, internal::ReplaceType::REPLACE_ALL);
}

wchar_t* WriteInto(std::wstring* str, size_t length_with_null) {
  return internal::WriteIntoT(str, length_with_null);
}

std::wstring JoinString(span<const std::wstring> parts,
                        WStringPiece separator) {
  return internal::JoinStringT(parts, separator);
}

std::wstring JoinString(span<const WStringPiece> parts,
                        WStringPiece separator) {
  return internal::JoinStringT(parts, separator);
}

std::wstring JoinString(std::initializer_list<WStringPiece> parts,
                        WStringPiece separator) {
  return internal::JoinStringT(parts, separator);
}

std::wstring ReplaceStringPlaceholders(WStringPiece format_string,
                                       const std::vector<std::wstring>& subst,
                                       std::vector<size_t>* offsets) {
  return internal::DoReplaceStringPlaceholders(format_string, subst, offsets);
}

}  // namespace base
