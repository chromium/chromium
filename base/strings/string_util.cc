// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wchar.h>
#include <wctype.h>

#include <algorithm>
#include <limits>
#include <type_traits>
#include <vector>

#include "base/check_op.h"
#include "base/cxx17_backports.h"
#include "base/no_destructor.h"
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

std::string ToLowerASCII(StringPiece str) {
  return internal::ToLowerASCIIImpl(str);
}

std::u16string ToLowerASCII(StringPiece16 str) {
  return internal::ToLowerASCIIImpl(str);
}

std::string ToUpperASCII(StringPiece str) {
  return internal::ToUpperASCIIImpl(str);
}

std::u16string ToUpperASCII(StringPiece16 str) {
  return internal::ToUpperASCIIImpl(str);
}

int CompareCaseInsensitiveASCII(StringPiece a, StringPiece b) {
  return internal::CompareCaseInsensitiveASCIIT(a, b);
}

int CompareCaseInsensitiveASCII(StringPiece16 a, StringPiece16 b) {
  return internal::CompareCaseInsensitiveASCIIT(a, b);
}

bool EqualsCaseInsensitiveASCII(StringPiece a, StringPiece b) {
  return a.size() == b.size() &&
         internal::CompareCaseInsensitiveASCIIT(a, b) == 0;
}

bool EqualsCaseInsensitiveASCII(StringPiece16 a, StringPiece16 b) {
  return a.size() == b.size() &&
         internal::CompareCaseInsensitiveASCIIT(a, b) == 0;
}

const std::string& EmptyString() {
  static const base::NoDestructor<std::string> s;
  return *s;
}

const std::u16string& EmptyString16() {
  static const base::NoDestructor<std::u16string> s16;
  return *s16;
}

bool ReplaceChars(StringPiece16 input,
                  StringPiece16 replace_chars,
                  StringPiece16 replace_with,
                  std::u16string* output) {
  return internal::ReplaceCharsT(input, replace_chars, replace_with, output);
}

bool ReplaceChars(StringPiece input,
                  StringPiece replace_chars,
                  StringPiece replace_with,
                  std::string* output) {
  return internal::ReplaceCharsT(input, replace_chars, replace_with, output);
}

bool RemoveChars(StringPiece16 input,
                 StringPiece16 remove_chars,
                 std::u16string* output) {
  return internal::ReplaceCharsT(input, remove_chars, StringPiece16(), output);
}

bool RemoveChars(StringPiece input,
                 StringPiece remove_chars,
                 std::string* output) {
  return internal::ReplaceCharsT(input, remove_chars, StringPiece(), output);
}

bool TrimString(StringPiece16 input,
                StringPiece16 trim_chars,
                std::u16string* output) {
  return internal::TrimStringT(input, trim_chars, TRIM_ALL, output) !=
         TRIM_NONE;
}

bool TrimString(StringPiece input,
                StringPiece trim_chars,
                std::string* output) {
  return internal::TrimStringT(input, trim_chars, TRIM_ALL, output) !=
         TRIM_NONE;
}

StringPiece16 TrimString(StringPiece16 input,
                         StringPiece16 trim_chars,
                         TrimPositions positions) {
  return internal::TrimStringPieceT(input, trim_chars, positions);
}

StringPiece TrimString(StringPiece input,
                       StringPiece trim_chars,
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
    CBU8_NEXT(data, char_index, truncation_length, code_point);
    if (!IsValidCharacter(code_point) ||
        !IsValidCodepoint(code_point)) {
      char_index = prev - 1;
    } else {
      break;
    }
  }

  if (char_index >= 0 )
    *output = input.substr(0, char_index);
  else
    output->clear();
}

TrimPositions TrimWhitespace(StringPiece16 input,
                             TrimPositions positions,
                             std::u16string* output) {
  return internal::TrimStringT(input, StringPiece16(kWhitespaceUTF16),
                               positions, output);
}

StringPiece16 TrimWhitespace(StringPiece16 input,
                             TrimPositions positions) {
  return internal::TrimStringPieceT(input, StringPiece16(kWhitespaceUTF16),
                                    positions);
}

TrimPositions TrimWhitespaceASCII(StringPiece input,
                                  TrimPositions positions,
                                  std::string* output) {
  return internal::TrimStringT(input, StringPiece(kWhitespaceASCII), positions,
                               output);
}

StringPiece TrimWhitespaceASCII(StringPiece input, TrimPositions positions) {
  return internal::TrimStringPieceT(input, StringPiece(kWhitespaceASCII),
                                    positions);
}

std::u16string CollapseWhitespace(StringPiece16 text,
                                  bool trim_sequences_with_line_breaks) {
  return internal::CollapseWhitespaceT(text, trim_sequences_with_line_breaks);
}

std::string CollapseWhitespaceASCII(StringPiece text,
                                    bool trim_sequences_with_line_breaks) {
  return internal::CollapseWhitespaceT(text, trim_sequences_with_line_breaks);
}

bool ContainsOnlyChars(StringPiece input, StringPiece characters) {
  return input.find_first_not_of(characters) == StringPiece::npos;
}

bool ContainsOnlyChars(StringPiece16 input, StringPiece16 characters) {
  return input.find_first_not_of(characters) == StringPiece16::npos;
}


bool IsStringASCII(StringPiece str) {
  return internal::DoIsStringASCII(str.data(), str.length());
}

bool IsStringASCII(StringPiece16 str) {
  return internal::DoIsStringASCII(str.data(), str.length());
}

#if defined(WCHAR_T_IS_UTF32)
bool IsStringASCII(WStringPiece str) {
  return internal::DoIsStringASCII(str.data(), str.length());
}
#endif

bool IsStringUTF8(StringPiece str) {
  return internal::DoIsStringUTF8<IsValidCharacter>(str);
}

bool IsStringUTF8AllowingNoncharacters(StringPiece str) {
  return internal::DoIsStringUTF8<IsValidCodepoint>(str);
}

bool LowerCaseEqualsASCII(StringPiece str, StringPiece lowercase_ascii) {
  return internal::DoLowerCaseEqualsASCII(str, lowercase_ascii);
}

bool LowerCaseEqualsASCII(StringPiece16 str, StringPiece lowercase_ascii) {
  return internal::DoLowerCaseEqualsASCII(str, lowercase_ascii);
}

bool EqualsASCII(StringPiece16 str, StringPiece ascii) {
  return std::equal(ascii.begin(), ascii.end(), str.begin(), str.end());
}

bool StartsWith(StringPiece str,
                StringPiece search_for,
                CompareCase case_sensitivity) {
  return internal::StartsWithT(str, search_for, case_sensitivity);
}

bool StartsWith(StringPiece16 str,
                StringPiece16 search_for,
                CompareCase case_sensitivity) {
  return internal::StartsWithT(str, search_for, case_sensitivity);
}

bool EndsWith(StringPiece str,
              StringPiece search_for,
              CompareCase case_sensitivity) {
  return internal::EndsWithT(str, search_for, case_sensitivity);
}

bool EndsWith(StringPiece16 str,
              StringPiece16 search_for,
              CompareCase case_sensitivity) {
  return internal::EndsWithT(str, search_for, case_sensitivity);
}

char HexDigitToInt(wchar_t c) {
  DCHECK(IsHexDigit(c));
  if (c >= '0' && c <= '9')
    return static_cast<char>(c - '0');
  if (c >= 'A' && c <= 'F')
    return static_cast<char>(c - 'A' + 10);
  if (c >= 'a' && c <= 'f')
    return static_cast<char>(c - 'a' + 10);
  return 0;
}

bool IsUnicodeWhitespace(wchar_t c) {
  // kWhitespaceWide is a NULL-terminated string
  for (const wchar_t* cur = kWhitespaceWide; *cur; ++cur) {
    if (*cur == c)
      return true;
  }
  return false;
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
         dimension < base::size(kByteStringsUnlocalized) - 1) {
    unit_amount /= kKilo;
    dimension++;
  }

  char buf[64];
  if (bytes != 0 && dimension > 0 && unit_amount < 100) {
    base::snprintf(buf, base::size(buf), "%.1lf%s", unit_amount,
                   kByteStringsUnlocalized[dimension]);
  } else {
    base::snprintf(buf, base::size(buf), "%.0lf%s", unit_amount,
                   kByteStringsUnlocalized[dimension]);
  }

  return ASCIIToUTF16(buf);
}

void ReplaceFirstSubstringAfterOffset(std::u16string* str,
                                      size_t start_offset,
                                      StringPiece16 find_this,
                                      StringPiece16 replace_with) {
  internal::DoReplaceMatchesAfterOffset(
      str, start_offset, internal::MakeSubstringMatcher(find_this),
      replace_with, internal::ReplaceType::REPLACE_FIRST);
}

void ReplaceFirstSubstringAfterOffset(std::string* str,
                                      size_t start_offset,
                                      StringPiece find_this,
                                      StringPiece replace_with) {
  internal::DoReplaceMatchesAfterOffset(
      str, start_offset, internal::MakeSubstringMatcher(find_this),
      replace_with, internal::ReplaceType::REPLACE_FIRST);
}

void ReplaceSubstringsAfterOffset(std::u16string* str,
                                  size_t start_offset,
                                  StringPiece16 find_this,
                                  StringPiece16 replace_with) {
  internal::DoReplaceMatchesAfterOffset(
      str, start_offset, internal::MakeSubstringMatcher(find_this),
      replace_with, internal::ReplaceType::REPLACE_ALL);
}

void ReplaceSubstringsAfterOffset(std::string* str,
                                  size_t start_offset,
                                  StringPiece find_this,
                                  StringPiece replace_with) {
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

std::string JoinString(span<const std::string> parts, StringPiece separator) {
  return internal::JoinStringT(parts, separator);
}

std::u16string JoinString(span<const std::u16string> parts,
                          StringPiece16 separator) {
  return internal::JoinStringT(parts, separator);
}

std::string JoinString(span<const StringPiece> parts, StringPiece separator) {
  return internal::JoinStringT(parts, separator);
}

std::u16string JoinString(span<const StringPiece16> parts,
                          StringPiece16 separator) {
  return internal::JoinStringT(parts, separator);
}

std::string JoinString(std::initializer_list<StringPiece> parts,
                       StringPiece separator) {
  return internal::JoinStringT(parts, separator);
}

std::u16string JoinString(std::initializer_list<StringPiece16> parts,
                          StringPiece16 separator) {
  return internal::JoinStringT(parts, separator);
}

std::u16string ReplaceStringPlaceholders(
    StringPiece16 format_string,
    const std::vector<std::u16string>& subst,
    std::vector<size_t>* offsets) {
  return internal::DoReplaceStringPlaceholders(format_string, subst, offsets);
}

std::string ReplaceStringPlaceholders(StringPiece format_string,
                                      const std::vector<std::string>& subst,
                                      std::vector<size_t>* offsets) {
  return internal::DoReplaceStringPlaceholders(format_string, subst, offsets);
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

size_t strlcpy(char* dst, const char* src, size_t dst_size) {
  return internal::lcpyT(dst, src, dst_size);
}
size_t wcslcpy(wchar_t* dst, const wchar_t* src, size_t dst_size) {
  return internal::lcpyT(dst, src, dst_size);
}

}  // namespace base
