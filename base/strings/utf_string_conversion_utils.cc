// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/strings/utf_string_conversion_utils.h"

#include "base/third_party/icu/icu_utf.h"
#include "build/build_config.h"

namespace base {

// CountUnicodeCharacters ------------------------------------------------------

std::optional<size_t> CountUnicodeCharacters(std::string_view text,
                                             size_t limit) {
  base_icu::UChar32 unused = 0;
  size_t count = 0;
  for (size_t index = 0; count < limit && index < text.size();
       ++count, ++index) {
    if (!ReadUnicodeCharacter(text.data(), text.size(), &index, &unused)) {
      return std::nullopt;
    }
  }
  return count;
}

// ReadUnicodeCharacter --------------------------------------------------------

bool ReadUnicodeCharacter(const char* src,
                          size_t src_len,
                          size_t* char_index,
                          base_icu::UChar32* code_point_out) {
  base_icu::UChar32 code_point;
  CBU8_NEXT(reinterpret_cast<const uint8_t*>(src), *char_index, src_len,
            code_point);
  *code_point_out = code_point;

  // The ICU macro above moves to the next char, we want to point to the last
  // char consumed.
  (*char_index)--;

  // Validate the decoded value.
  return IsValidCodepoint(code_point);
}

bool ReadUnicodeCharacter(const char16_t* src,
                          size_t src_len,
                          size_t* char_index,
                          base_icu::UChar32* code_point) {
  if (CBU16_IS_SURROGATE(src[*char_index])) {
    if (!CBU16_IS_SURROGATE_LEAD(src[*char_index]) || !src_len ||
        *char_index >= src_len - 1 || !CBU16_IS_TRAIL(src[*char_index + 1])) {
      // Invalid surrogate pair.
      return false;
    }

    // Valid surrogate pair.
    *code_point = CBU16_GET_SUPPLEMENTARY(src[*char_index],
                                          src[*char_index + 1]);
    (*char_index)++;
  } else {
    // Not a surrogate, just one 16-bit word.
    *code_point = src[*char_index];
  }

  return IsValidCodepoint(*code_point);
}

#if defined(WCHAR_T_IS_32_BIT)
bool ReadUnicodeCharacter(const wchar_t* src,
                          size_t src_len,
                          size_t* char_index,
                          base_icu::UChar32* code_point) {
  // Conversion is easy since the source is 32-bit.
  *code_point = static_cast<base_icu::UChar32>(src[*char_index]);

  // Validate the value.
  return IsValidCodepoint(*code_point);
}
#endif  // defined(WCHAR_T_IS_32_BIT)

// WriteUnicodeCharacter -------------------------------------------------------

size_t WriteUnicodeCharacter(base_icu::UChar32 code_point,
                             std::string* output) {
  if (code_point >= 0 && code_point <= 0x7f) {
    // Fast path the common case of one byte.
    output->push_back(static_cast<char>(code_point));
    return 1;
  }

  // CBU8_APPEND_UNSAFE can append up to 4 bytes.
  size_t char_offset = output->length();
  size_t original_char_offset = char_offset;
  output->resize(char_offset + CBU8_MAX_LENGTH);

  CBU8_APPEND_UNSAFE(reinterpret_cast<uint8_t*>(output->data()), char_offset,
                     code_point);

  // CBU8_APPEND_UNSAFE will advance our pointer past the inserted character, so
  // it will represent the new length of the string.
  output->resize(char_offset);
  return char_offset - original_char_offset;
}

size_t WriteUnicodeCharacter(base_icu::UChar32 code_point,
                             std::u16string* output) {
  if (CBU16_LENGTH(code_point) == 1) {
    // The code point is in the Basic Multilingual Plane (BMP).
    output->push_back(static_cast<char16_t>(code_point));
    return 1;
  }
  // Non-BMP characters use a double-character encoding.
  size_t char_offset = output->length();
  output->resize(char_offset + CBU16_MAX_LENGTH);
  CBU16_APPEND_UNSAFE(&(*output)[0], char_offset, code_point);
  return CBU16_MAX_LENGTH;
}

// Generalized Unicode converter -----------------------------------------------

template<typename CHAR>
void PrepareForUTF8Output(const CHAR* src,
                          size_t src_len,
                          std::string* output) {
  output->clear();
  if (src_len == 0)
    return;
  if (src[0] < 0x80) {
    // Assume that the entire input will be ASCII.
    output->reserve(src_len);
  } else {
    // Assume that the entire input is non-ASCII and will have 3 bytes per char.
    output->reserve(src_len * 3);
  }
}

// Instantiate versions we know callers will need.
#if !BUILDFLAG(IS_WIN)
// wchar_t and char16_t are the same thing on Windows.
template void PrepareForUTF8Output(const wchar_t*, size_t, std::string*);
#endif
template void PrepareForUTF8Output(const char16_t*, size_t, std::string*);

template<typename STRING>
void PrepareForUTF16Or32Output(const char* src,
                               size_t src_len,
                               STRING* output) {
  output->clear();
  if (src_len == 0)
    return;
  if (static_cast<unsigned char>(src[0]) < 0x80) {
    // Assume the input is all ASCII, which means 1:1 correspondence.
    output->reserve(src_len);
  } else {
    // Otherwise assume that the UTF-8 sequences will have 2 bytes for each
    // character.
    output->reserve(src_len / 2);
  }
}

// Instantiate versions we know callers will need.
#if !BUILDFLAG(IS_WIN)
// std::wstring and std::u16string are the same thing on Windows.
template void PrepareForUTF16Or32Output(const char*, size_t, std::wstring*);
#endif
template void PrepareForUTF16Or32Output(const char*, size_t, std::u16string*);

}  // namespace base
