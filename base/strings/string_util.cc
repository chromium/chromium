// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/strings/string_util.h"

#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>

#include <limits>
#include <optional>
#include <string_view>
#include <type_traits>
#include <vector>

#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util_impl_helpers.h"
#include "base/strings/string_util_internal.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "base/third_party/icu/icu_utf.h"
#include "build/build_config.h"

namespace base {

bool IsWprintfFormatPortable(const wchar_t* format) {
  for (const wchar_t* position = format; *position != '\0'; ++position) {
    if (*position == '%') {
      bool in_specification = true;
      bool modifier_l = false;
      while (in_specification) {
        // Eat up characters until reaching a known specifier.
        if (*++position == '\0') {
          // The format string ended in the middle of a specification.  Call
          // it portable because no unportable specifications were found.  The
          // string is equally broken on all platforms.
          return true;
        }

        if (*position == 'l') {
          // 'l' is the only thing that can save the 's' and 'c' specifiers.
          modifier_l = true;
        } else if (((*position == 's' || *position == 'c') && !modifier_l) ||
                   *position == 'S' || *position == 'C' || *position == 'F' ||
                   *position == 'D' || *position == 'O' || *position == 'U') {
          // Not portable.
          return false;
        }

        if (wcschr(L"diouxXeEfgGaAcspn%", *position)) {
          // Portable, keep scanning the rest of the format string.
          in_specification = false;
        }
      }
    }
  }

  return true;
}

std::string ToLowerASCII(std::string_view str) {
  return internal::ToLowerASCIIImpl(str);
}

std::u16string ToLowerASCII(std::u16string_view str) {
  return internal::ToLowerASCIIImpl(str);
}

std::string ToUpperASCII(std::string_view str) {
  return internal::ToUpperASCIIImpl(str);
}

std::u16string ToUpperASCII(std::u16string_view str) {
  return internal::ToUpperASCIIImpl(str);
}

const std::string& EmptyString() {
  static const base::NoDestructor<std::string> s;
  return *s;
}

const std::u16string& EmptyString16() {
  static const base::NoDestructor<std::u16string> s16;
  return *s16;
}

bool ReplaceChars(std::u16string_view input,
                  std::u16string_view replace_chars,
                  std::u16string_view replace_with,
                  std::u16string* output) {
  return internal::ReplaceCharsT(input, replace_chars, replace_with, output);
}

bool ReplaceChars(std::string_view input,
                  std::string_view replace_chars,
                  std::string_view replace_with,
                  std::string* output) {
  return internal::ReplaceCharsT(input, replace_chars, replace_with, output);
}

bool RemoveChars(std::u16string_view input,
                 std::u16string_view remove_chars,
                 std::u16string* output) {
  return internal::ReplaceCharsT(input, remove_chars, std::u16string_view(),
                                 output);
}

bool RemoveChars(std::string_view input,
                 std::string_view remove_chars,
                 std::string* output) {
  return internal::ReplaceCharsT(input, remove_chars, std::string_view(),
                                 output);
}

bool TrimString(std::u16string_view input,
                std::u16string_view trim_chars,
                std::u16string* output) {
  return internal::TrimStringT(input, trim_chars, TRIM_ALL, output) !=
         TRIM_NONE;
}

bool TrimString(std::string_view input,
                std::string_view trim_chars,
                std::string* output) {
  return internal::TrimStringT(input, trim_chars, TRIM_ALL, output) !=
         TRIM_NONE;
}

std::u16string_view TrimString(std::u16string_view input,
                               std::u16string_view trim_chars,
                               TrimPositions positions) {
  return internal::TrimStringPieceT(input, trim_chars, positions);
}

std::string_view TrimString(std::string_view input,
                            std::string_view trim_chars,
                            TrimPositions positions) {
  return internal::TrimStringPieceT(input, trim_chars, positions);
}

void TruncateUTF8ToByteSize(const std::string& input,
                            const size_t byte_size,
                            std::string* output) {
  DCHECK(output);
  if (byte_size > input.length()) {
    *output = input;
    return;
  }
  DCHECK_LE(byte_size,
            static_cast<uint32_t>(std::numeric_limits<int32_t>::max()));
  // Note: This cast is necessary because CBU8_NEXT uses int32_ts.
  int32_t truncation_length = static_cast<int32_t>(byte_size);
  int32_t char_index = truncation_length - 1;
  const char* data = input.data();

  // Using CBU8, we will move backwards from the truncation point
  // to the beginning of the string looking for a valid UTF8
  // character.  Once a full UTF8 character is found, we will
  // truncate the string to the end of that character.
  while (char_index >= 0) {
    int32_t prev = char_index;
    base_icu::UChar32 code_point = 0;
    CBU8_NEXT(reinterpret_cast<const uint8_t*>(data), char_index,
              truncation_length, code_point);
    if (!IsValidCharacter(code_point)) {
      char_index = prev - 1;
    } else {
      break;
    }
  }

  if (char_index >= 0 )
    *output = input.substr(0, static_cast<size_t>(char_index));
  else
    output->clear();
}

TrimPositions TrimWhitespace(std::u16string_view input,
                             TrimPositions positions,
                             std::u16string* output) {
  return internal::TrimStringT(input, std::u16string_view(kWhitespaceUTF16),
                               positions, output);
}

std::u16string_view TrimWhitespace(std::u16string_view input,
                                   TrimPositions positions) {
  return internal::TrimStringPieceT(
      input, std::u16string_view(kWhitespaceUTF16), positions);
}

TrimPositions TrimWhitespaceASCII(std::string_view input,
                                  TrimPositions positions,
                                  std::string* output) {
  return internal::TrimStringT(input, std::string_view(kWhitespaceASCII),
                               positions, output);
}

std::string_view TrimWhitespaceASCII(std::string_view input,
                                     TrimPositions positions) {
  return internal::TrimStringPieceT(input, std::string_view(kWhitespaceASCII),
                                    positions);
}

std::u16string CollapseWhitespace(std::u16string_view text,
                                  bool trim_sequences_with_line_breaks) {
  return internal::CollapseWhitespaceT(text, trim_sequences_with_line_breaks);
}

std::string CollapseWhitespaceASCII(std::string_view text,
                                    bool trim_sequences_with_line_breaks) {
  return internal::CollapseWhitespaceT(text, trim_sequences_with_line_breaks);
}

bool ContainsOnlyChars(std::string_view input, std::string_view characters) {
  return input.find_first_not_of(characters) == std::string_view::npos;
}

bool ContainsOnlyChars(std::u16string_view input,
                       std::u16string_view characters) {
  return input.find_first_not_of(characters) == std::u16string_view::npos;
}

bool IsStringASCII(std::string_view str) {
  return internal::DoIsStringASCII(str.data(), str.length());
}

bool IsStringASCII(std::u16string_view str) {
  return internal::DoIsStringASCII(str.data(), str.length());
}

#if defined(WCHAR_T_IS_32_BIT)
bool IsStringASCII(std::wstring_view str) {
  return internal::DoIsStringASCII(str.data(), str.length());
}
#endif

bool IsStringUTF8(std::string_view str) {
  return internal::DoIsStringUTF8<IsValidCharacter>(str);
}

bool IsStringUTF8AllowingNoncharacters(std::string_view str) {
  return internal::DoIsStringUTF8<IsValidCodepoint>(str);
}

bool EqualsASCII(std::u16string_view str, std::string_view ascii) {
  return ranges::equal(ascii, str);
}

bool StartsWith(std::string_view str,
                std::string_view search_for,
                CompareCase case_sensitivity) {
  return internal::StartsWithT(str, search_for, case_sensitivity);
}

bool StartsWith(std::u16string_view str,
                std::u16string_view search_for,
                CompareCase case_sensitivity) {
  return internal::StartsWithT(str, search_for, case_sensitivity);
}

bool EndsWith(std::string_view str,
              std::string_view search_for,
              CompareCase case_sensitivity) {
  return internal::EndsWithT(str, search_for, case_sensitivity);
}

bool EndsWith(std::u16string_view str,
              std::u16string_view search_for,
              CompareCase case_sensitivity) {
  return internal::EndsWithT(str, search_for, case_sensitivity);
}

char HexDigitToInt(char c) {
  DCHECK(IsHexDigit(c));
  if (c >= '0' && c <= '9')
    return static_cast<char>(c - '0');
  return (c >= 'A' && c <= 'F') ? static_cast<char>(c - 'A' + 10)
                                : static_cast<char>(c - 'a' + 10);
}

static const char* const kByteStringsUnlocalized[] = {
  " B",
  " kB",
  " MB",
  " GB",
  " TB",
  " PB"
};

std::u16string FormatBytesUnlocalized(int64_t bytes) {
  double unit_amount = static_cast<double>(bytes);
  size_t dimension = 0;
  const int kKilo = 1024;
  while (unit_amount >= kKilo &&
         dimension < std::size(kByteStringsUnlocalized) - 1) {
    unit_amount /= kKilo;
    dimension++;
  }

  char buf[64];
  if (bytes != 0 && dimension > 0 && unit_amount < 100) {
    base::snprintf(buf, std::size(buf), "%.1lf%s", unit_amount,
                   kByteStringsUnlocalized[dimension]);
  } else {
    base::snprintf(buf, std::size(buf), "%.0lf%s", unit_amount,
                   kByteStringsUnlocalized[dimension]);
  }

  return ASCIIToUTF16(buf);
}

void ReplaceFirstSubstringAfterOffset(std::u16string* str,
                                      size_t start_offset,
                                      std::u16string_view find_this,
                                      std::u16string_view replace_with) {
  internal::DoReplaceMatchesAfterOffset(
      str, start_offset, internal::MakeSubstringMatcher(find_this),
      replace_with, internal::ReplaceType::REPLACE_FIRST);
}

void ReplaceFirstSubstringAfterOffset(std::string* str,
                                      size_t start_offset,
                                      std::string_view find_this,
                                      std::string_view replace_with) {
  internal::DoReplaceMatchesAfterOffset(
      str, start_offset, internal::MakeSubstringMatcher(find_this),
      replace_with, internal::ReplaceType::REPLACE_FIRST);
}

void ReplaceSubstringsAfterOffset(std::u16string* str,
                                  size_t start_offset,
                                  std::u16string_view find_this,
                                  std::u16string_view replace_with) {
  internal::DoReplaceMatchesAfterOffset(
      str, start_offset, internal::MakeSubstringMatcher(find_this),
      replace_with, internal::ReplaceType::REPLACE_ALL);
}

void ReplaceSubstringsAfterOffset(std::string* str,
                                  size_t start_offset,
                                  std::string_view find_this,
                                  std::string_view replace_with) {
  internal::DoReplaceMatchesAfterOffset(
      str, start_offset, internal::MakeSubstringMatcher(find_this),
      replace_with, internal::ReplaceType::REPLACE_ALL);
}

char* WriteInto(std::string* str, size_t length_with_null) {
  return internal::WriteIntoT(str, length_with_null);
}

char16_t* WriteInto(std::u16string* str, size_t length_with_null) {
  return internal::WriteIntoT(str, length_with_null);
}

std::string JoinString(span<const std::string> parts,
                       std::string_view separator) {
  return internal::JoinStringT(parts, separator);
}

std::u16string JoinString(span<const std::u16string> parts,
                          std::u16string_view separator) {
  return internal::JoinStringT(parts, separator);
}

std::string JoinString(span<const std::string_view> parts,
                       std::string_view separator) {
  return internal::JoinStringT(parts, separator);
}

std::u16string JoinString(span<const std::u16string_view> parts,
                          std::u16string_view separator) {
  return internal::JoinStringT(parts, separator);
}

std::string JoinString(std::initializer_list<std::string_view> parts,
                       std::string_view separator) {
  return internal::JoinStringT(parts, separator);
}

std::u16string JoinString(std::initializer_list<std::u16string_view> parts,
                          std::u16string_view separator) {
  return internal::JoinStringT(parts, separator);
}

std::u16string ReplaceStringPlaceholders(
    std::u16string_view format_string,
    const std::vector<std::u16string>& subst,
    std::vector<size_t>* offsets) {
  std::optional<std::u16string> replacement =
      internal::DoReplaceStringPlaceholders(
          format_string, subst,
          /*placeholder_prefix*/ u'$',
          /*should_escape_multiple_placeholder_prefixes*/ true,
          /*is_strict_mode*/ false, offsets);

  DCHECK(replacement);
  return replacement.value();
}

std::string ReplaceStringPlaceholders(std::string_view format_string,
                                      const std::vector<std::string>& subst,
                                      std::vector<size_t>* offsets) {
  std::optional<std::string> replacement =
      internal::DoReplaceStringPlaceholders(
          format_string, subst,
          /*placeholder_prefix*/ '$',
          /*should_escape_multiple_placeholder_prefixes*/ true,
          /*is_strict_mode*/ false, offsets);

  DCHECK(replacement);
  return replacement.value();
}

std::u16string ReplaceStringPlaceholders(const std::u16string& format_string,
                                         const std::u16string& a,
                                         size_t* offset) {
  std::vector<size_t> offsets;
  std::u16string result =
      ReplaceStringPlaceholders(format_string, {a}, &offsets);

  DCHECK_EQ(1U, offsets.size());
  if (offset)
    *offset = offsets[0];
  return result;
}

size_t strlcpy(span<char> dst, std::string_view src) {
  return internal::lcpyT(dst, src);
}

size_t u16cstrlcpy(span<char16_t> dst, std::u16string_view src) {
  return internal::lcpyT(dst, src);
}

size_t wcslcpy(span<wchar_t> dst, std::wstring_view src) {
  return internal::lcpyT(dst, src);
}

size_t strlcpy(char* dst, const char* src, size_t dst_size) {
  return internal::lcpyT(
      UNSAFE_TODO(base::span(dst, dst_size), std::string_view(src)));
}

size_t u16cstrlcpy(char16_t* dst, const char16_t* src, size_t dst_size) {
  return internal::lcpyT(UNSAFE_TODO(base::span(dst, dst_size)),
                         std::u16string_view(src));
}

size_t wcslcpy(wchar_t* dst, const wchar_t* src, size_t dst_size) {
  return internal::lcpyT(UNSAFE_TODO(base::span(dst, dst_size)),
                         std::wstring_view(src));
}

}  // namespace base
