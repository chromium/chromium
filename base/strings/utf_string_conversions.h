// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_STRINGS_UTF_STRING_CONVERSIONS_H_
#define BASE_STRINGS_UTF_STRING_CONVERSIONS_H_

#include <stddef.h>

#include <string>

#include "base/base_export.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"

namespace base {

// These convert between UTF-8, -16, and -32 strings. They are potentially slow,
// so avoid unnecessary conversions. The low-level versions return a boolean
// indicating whether the conversion was 100% valid. In this case, it will still
// do the best it can and put the result in the output buffer. The versions that
// return strings ignore this error and just return the best conversion
// possible.
BASE_EXPORT bool WideToUTF8(const wchar_t* src, size_t src_len,
                            std::string* output);
BASE_EXPORT std::string WideToUTF8(WStringPiece wide) WARN_UNUSED_RESULT;
BASE_EXPORT bool UTF8ToWide(const char* src, size_t src_len,
                            std::wstring* output);
BASE_EXPORT std::wstring UTF8ToWide(StringPiece utf8) WARN_UNUSED_RESULT;

BASE_EXPORT bool WideToUTF16(const wchar_t* src,
                             size_t src_len,
                             std::u16string* output);
BASE_EXPORT std::u16string WideToUTF16(WStringPiece wide) WARN_UNUSED_RESULT;
BASE_EXPORT bool UTF16ToWide(const char16_t* src,
                             size_t src_len,
                             std::wstring* output);
BASE_EXPORT std::wstring UTF16ToWide(StringPiece16 utf16) WARN_UNUSED_RESULT;

BASE_EXPORT bool UTF8ToUTF16(const char* src,
                             size_t src_len,
                             std::u16string* output);
BASE_EXPORT std::u16string UTF8ToUTF16(StringPiece utf8) WARN_UNUSED_RESULT;
BASE_EXPORT bool UTF16ToUTF8(const char16_t* src,
                             size_t src_len,
                             std::string* output);
BASE_EXPORT std::string UTF16ToUTF8(StringPiece16 utf16) WARN_UNUSED_RESULT;

// This converts an ASCII string, typically a hardcoded constant, to a UTF16
// string.
BASE_EXPORT std::u16string ASCIIToUTF16(StringPiece ascii) WARN_UNUSED_RESULT;

// Converts to 7-bit ASCII by truncating. The result must be known to be ASCII
// beforehand.
BASE_EXPORT std::string UTF16ToASCII(StringPiece16 utf16) WARN_UNUSED_RESULT;

#if defined(WCHAR_T_IS_UTF16)
// This converts an ASCII string, typically a hardcoded constant, to a wide
// string.
BASE_EXPORT std::wstring ASCIIToWide(StringPiece ascii) WARN_UNUSED_RESULT;

// Converts to 7-bit ASCII by truncating. The result must be known to be ASCII
// beforehand.
BASE_EXPORT std::string WideToASCII(WStringPiece wide) WARN_UNUSED_RESULT;
#endif  // defined(WCHAR_T_IS_UTF16)

// The conversion functions in this file should not be used to convert string
// literals. Instead, the corresponding prefixes (e.g. u"" for UTF16 or L"" for
// Wide) should be used. Deleting the overloads here catches these cases at
// compile time.
template <size_t N>
std::u16string WideToUTF16(const wchar_t (&str)[N]) {
  static_assert(N == 0, "Error: Use the u\"...\" prefix instead.");
  return std::u16string();
}

template <size_t N>
std::u16string UTF8ToUTF16(const char (&str)[N]) {
  static_assert(N == 0, "Error: Use the u\"...\" prefix instead.");
  return std::u16string();
}

template <size_t N>
std::u16string ASCIIToUTF16(const char (&str)[N]) {
  static_assert(N == 0, "Error: Use the u\"...\" prefix instead.");
  return std::u16string();
}

// Mutable character arrays are usually only populated during runtime. Continue
// to allow this conversion.
template <size_t N>
std::u16string ASCIIToUTF16(char (&str)[N]) {
  return ASCIIToUTF16(StringPiece(str));
}

}  // namespace base

#endif  // BASE_STRINGS_UTF_STRING_CONVERSIONS_H_
