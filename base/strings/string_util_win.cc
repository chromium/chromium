// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util_win.h"

#include <optional>
#include <string_view>

#include "base/ranges/algorithm.h"
#include "base/strings/string_util_impl_helpers.h"

namespace base {

bool IsStringASCII(std::wstring_view str) {
  return internal::DoIsStringASCII(str.data(), str.length());
}

std::wstring ToLowerASCII(std::wstring_view str) {
  return internal::ToLowerASCIIImpl(str);
}

std::wstring ToUpperASCII(std::wstring_view str) {
  return internal::ToUpperASCIIImpl(str);
}

int CompareCaseInsensitiveASCII(std::wstring_view a, std::wstring_view b) {
  return internal::CompareCaseInsensitiveASCIIT(a, b);
}

bool RemoveChars(std::wstring_view input,
                 std::wstring_view remove_chars,
                 std::wstring* output) {
  return internal::ReplaceCharsT(input, remove_chars, std::wstring_view(),
                                 output);
}

bool ReplaceChars(std::wstring_view input,
                  std::wstring_view replace_chars,
                  std::wstring_view replace_with,
                  std::wstring* output) {
  return internal::ReplaceCharsT(input, replace_chars, replace_with, output);
}

bool TrimString(std::wstring_view input,
                std::wstring_view trim_chars,
                std::wstring* output) {
  return internal::TrimStringT(input, trim_chars, TRIM_ALL, output) !=
         TRIM_NONE;
}

std::wstring_view TrimString(std::wstring_view input,
                             std::wstring_view trim_chars,
                             TrimPositions positions) {
  return internal::TrimStringPieceT(input, trim_chars, positions);
}

TrimPositions TrimWhitespace(std::wstring_view input,
                             TrimPositions positions,
                             std::wstring* output) {
  return internal::TrimStringT(input, std::wstring_view(kWhitespaceWide),
                               positions, output);
}

std::wstring_view TrimWhitespace(std::wstring_view input,
                                 TrimPositions positions) {
  return internal::TrimStringPieceT(input, std::wstring_view(kWhitespaceWide),
                                    positions);
}

std::wstring CollapseWhitespace(std::wstring_view text,
                                bool trim_sequences_with_line_breaks) {
  return internal::CollapseWhitespaceT(text, trim_sequences_with_line_breaks);
}

bool ContainsOnlyChars(std::wstring_view input, std::wstring_view characters) {
  return input.find_first_not_of(characters) == std::string_view::npos;
}

bool EqualsASCII(std::wstring_view str, std::string_view ascii) {
  return ranges::equal(ascii, str);
}

bool StartsWith(std::wstring_view str,
                std::wstring_view search_for,
                CompareCase case_sensitivity) {
  return internal::StartsWithT(str, search_for, case_sensitivity);
}

bool EndsWith(std::wstring_view str,
              std::wstring_view search_for,
              CompareCase case_sensitivity) {
  return internal::EndsWithT(str, search_for, case_sensitivity);
}

void ReplaceFirstSubstringAfterOffset(std::wstring* str,
                                      size_t start_offset,
                                      std::wstring_view find_this,
                                      std::wstring_view replace_with) {
  internal::DoReplaceMatchesAfterOffset(
      str, start_offset, internal::MakeSubstringMatcher(find_this),
      replace_with, internal::ReplaceType::REPLACE_FIRST);
}

void ReplaceSubstringsAfterOffset(std::wstring* str,
                                  size_t start_offset,
                                  std::wstring_view find_this,
                                  std::wstring_view replace_with) {
  internal::DoReplaceMatchesAfterOffset(
      str, start_offset, internal::MakeSubstringMatcher(find_this),
      replace_with, internal::ReplaceType::REPLACE_ALL);
}

wchar_t* WriteInto(std::wstring* str, size_t length_with_null) {
  return internal::WriteIntoT(str, length_with_null);
}

std::wstring JoinString(span<const std::wstring> parts,
                        std::wstring_view separator) {
  return internal::JoinStringT(parts, separator);
}

std::wstring JoinString(span<const std::wstring_view> parts,
                        std::wstring_view separator) {
  return internal::JoinStringT(parts, separator);
}

std::wstring JoinString(std::initializer_list<std::wstring_view> parts,
                        std::wstring_view separator) {
  return internal::JoinStringT(parts, separator);
}

std::wstring ReplaceStringPlaceholders(std::wstring_view format_string,
                                       const std::vector<std::wstring>& subst,
                                       std::vector<size_t>* offsets) {
  std::optional<std::wstring> replacement =
      internal::DoReplaceStringPlaceholders(
          format_string, subst,
          /*placeholder_prefix*/ L'$',
          /*should_escape_multiple_placeholder_prefixes*/ true,
          /*is_strict_mode*/ false, offsets);

  DCHECK(replacement);
  return replacement.value();
}

}  // namespace base
