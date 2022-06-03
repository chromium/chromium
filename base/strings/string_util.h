// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines utility functions for working with strings.

#ifndef BASE_STRINGS_STRING_UTIL_H_
#define BASE_STRINGS_STRING_UTIL_H_

#include <ctype.h>
#include <stdarg.h>   // va_list
#include <stddef.h>
#include <stdint.h>

#include <initializer_list>
#include <string>
#include <type_traits>
#include <vector>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "base/cxx20_to_address.h"
#include "base/strings/string_piece.h"  // For implicit conversions.
#include "build/build_config.h"

namespace base {

// C standard-library functions that aren't cross-platform are provided as
// "base::...", and their prototypes are listed below. These functions are
// then implemented as inline calls to the platform-specific equivalents in the
// platform-specific headers.

// Wrapper for vsnprintf that always null-terminates and always returns the
// number of characters that would be in an untruncated formatted
// string, even when truncation occurs.
int vsnprintf(char* buffer, size_t size, const char* format, va_list arguments)
    PRINTF_FORMAT(3, 0);

// Some of these implementations need to be inlined.

// We separate the declaration from the implementation of this inline
// function just so the PRINTF_FORMAT works.
inline int snprintf(char* buffer, size_t size, const char* format, ...)
    PRINTF_FORMAT(3, 4);
inline int snprintf(char* buffer, size_t size, const char* format, ...) {
  va_list arguments;
  va_start(arguments, format);
  int result = vsnprintf(buffer, size, format, arguments);
  va_end(arguments);
  return result;
}

// BSD-style safe and consistent string copy functions.
// Copies |src| to |dst|, where |dst_size| is the total allocated size of |dst|.
// Copies at most |dst_size|-1 characters, and always NULL terminates |dst|, as
// long as |dst_size| is not 0.  Returns the length of |src| in characters.
// If the return value is >= dst_size, then the output was truncated.
// NOTE: All sizes are in number of characters, NOT in bytes.
BASE_EXPORT size_t strlcpy(char* dst, const char* src, size_t dst_size);
BASE_EXPORT size_t wcslcpy(wchar_t* dst, const wchar_t* src, size_t dst_size);

// Scan a wprintf format string to determine whether it's portable across a
// variety of systems.  This function only checks that the conversion
// specifiers used by the format string are supported and have the same meaning
// on a variety of systems.  It doesn't check for other errors that might occur
// within a format string.
//
// Nonportable conversion specifiers for wprintf are:
//  - 's' and 'c' without an 'l' length modifier.  %s and %c operate on char
//     data on all systems except Windows, which treat them as wchar_t data.
//     Use %ls and %lc for wchar_t data instead.
//  - 'S' and 'C', which operate on wchar_t data on all systems except Windows,
//     which treat them as char data.  Use %ls and %lc for wchar_t data
//     instead.
//  - 'F', which is not identified by Windows wprintf documentation.
//  - 'D', 'O', and 'U', which are deprecated and not available on all systems.
//     Use %ld, %lo, and %lu instead.
//
// Note that there is no portable conversion specifier for char data when
// working with wprintf.
//
// This function is intended to be called from base::vswprintf.
BASE_EXPORT bool IsWprintfFormatPortable(const wchar_t* format);

// Simplified implementation of C++20's std::basic_string_view(It, End).
// Reference: https://wg21.link/string.view.cons
template <typename CharT, typename Iter>
constexpr BasicStringPiece<CharT> MakeBasicStringPiece(Iter begin, Iter end) {
  DCHECK_GE(end - begin, 0);
  return {base::to_address(begin), static_cast<size_t>(end - begin)};
}

// Explicit instantiations of MakeBasicStringPiece for the BasicStringPiece
// aliases defined in base/strings/string_piece_forward.h
template <typename Iter>
constexpr StringPiece MakeStringPiece(Iter begin, Iter end) {
  return MakeBasicStringPiece<char>(begin, end);
}

template <typename Iter>
constexpr StringPiece16 MakeStringPiece16(Iter begin, Iter end) {
  return MakeBasicStringPiece<char16_t>(begin, end);
}

template <typename Iter>
constexpr WStringPiece MakeWStringPiece(Iter begin, Iter end) {
  return MakeBasicStringPiece<wchar_t>(begin, end);
}

// ASCII-specific tolower.  The standard library's tolower is locale sensitive,
// so we don't want to use it here.
template <typename CharT,
          typename = std::enable_if_t<std::is_integral<CharT>::value>>
CharT ToLowerASCII(CharT c) {
  return (c >= 'A' && c <= 'Z') ? (c + ('a' - 'A')) : c;
}

// ASCII-specific toupper.  The standard library's toupper is locale sensitive,
// so we don't want to use it here.
template <typename CharT,
          typename = std::enable_if_t<std::is_integral<CharT>::value>>
CharT ToUpperASCII(CharT c) {
  return (c >= 'a' && c <= 'z') ? (c + ('A' - 'a')) : c;
}

// Converts the given string to it's ASCII-lowercase equivalent.
BASE_EXPORT std::string ToLowerASCII(StringPiece str);
BASE_EXPORT std::u16string ToLowerASCII(StringPiece16 str);

// Converts the given string to it's ASCII-uppercase equivalent.
BASE_EXPORT std::string ToUpperASCII(StringPiece str);
BASE_EXPORT std::u16string ToUpperASCII(StringPiece16 str);

// Functor for case-insensitive ASCII comparisons for STL algorithms like
// std::search.
//
// Note that a full Unicode version of this functor is not possible to write
// because case mappings might change the number of characters, depend on
// context (combining accents), and require handling UTF-16. If you need
// proper Unicode support, use base::i18n::ToLower/FoldCase and then just
// use a normal operator== on the result.
template<typename Char> struct CaseInsensitiveCompareASCII {
 public:
  bool operator()(Char x, Char y) const {
    return ToLowerASCII(x) == ToLowerASCII(y);
  }
};

// Like strcasecmp for case-insensitive ASCII characters only. Returns:
//   -1  (a < b)
//    0  (a == b)
//    1  (a > b)
// (unlike strcasecmp which can return values greater or less than 1/-1). For
// full Unicode support, use base::i18n::ToLower or base::i18h::FoldCase
// and then just call the normal string operators on the result.
BASE_EXPORT int CompareCaseInsensitiveASCII(StringPiece a, StringPiece b);
BASE_EXPORT int CompareCaseInsensitiveASCII(StringPiece16 a, StringPiece16 b);

// Equality for ASCII case-insensitive comparisons. For full Unicode support,
// use base::i18n::ToLower or base::i18h::FoldCase and then compare with either
// == or !=.
BASE_EXPORT bool EqualsCaseInsensitiveASCII(StringPiece a, StringPiece b);
BASE_EXPORT bool EqualsCaseInsensitiveASCII(StringPiece16 a, StringPiece16 b);

// These threadsafe functions return references to globally unique empty
// strings.
//
// It is likely faster to construct a new empty string object (just a few
// instructions to set the length to 0) than to get the empty string instance
// returned by these functions (which requires threadsafe static access).
//
// Therefore, DO NOT USE THESE AS A GENERAL-PURPOSE SUBSTITUTE FOR DEFAULT
// CONSTRUCTORS. There is only one case where you should use these: functions
// which need to return a string by reference (e.g. as a class member
// accessor), and don't have an empty string to use (e.g. in an error case).
// These should not be used as initializers, function arguments, or return
// values for functions which return by value or outparam.
BASE_EXPORT const std::string& EmptyString();
BASE_EXPORT const std::u16string& EmptyString16();

// Contains the set of characters representing whitespace in the corresponding
// encoding. Null-terminated. The ASCII versions are the whitespaces as defined
// by HTML5, and don't include control characters.
BASE_EXPORT extern const wchar_t kWhitespaceWide[];  // Includes Unicode.
BASE_EXPORT extern const char16_t kWhitespaceUTF16[];  // Includes Unicode.
BASE_EXPORT extern const char16_t
    kWhitespaceNoCrLfUTF16[];  // Unicode w/o CR/LF.
BASE_EXPORT extern const char kWhitespaceASCII[];
BASE_EXPORT extern const char16_t kWhitespaceASCIIAs16[];  // No unicode.

// Null-terminated string representing the UTF-8 byte order mark.
BASE_EXPORT extern const char kUtf8ByteOrderMark[];

// Removes characters in |remove_chars| from anywhere in |input|.  Returns true
// if any characters were removed.  |remove_chars| must be null-terminated.
// NOTE: Safe to use the same variable for both |input| and |output|.
BASE_EXPORT bool RemoveChars(StringPiece16 input,
                             StringPiece16 remove_chars,
                             std::u16string* output);
BASE_EXPORT bool RemoveChars(StringPiece input,
                             StringPiece remove_chars,
                             std::string* output);

// Replaces characters in |replace_chars| from anywhere in |input| with
// |replace_with|.  Each character in |replace_chars| will be replaced with
// the |replace_with| string.  Returns true if any characters were replaced.
// |replace_chars| must be null-terminated.
// NOTE: Safe to use the same variable for both |input| and |output|.
BASE_EXPORT bool ReplaceChars(StringPiece16 input,
                              StringPiece16 replace_chars,
                              StringPiece16 replace_with,
                              std::u16string* output);
BASE_EXPORT bool ReplaceChars(StringPiece input,
                              StringPiece replace_chars,
                              StringPiece replace_with,
                              std::string* output);

enum TrimPositions {
  TRIM_NONE     = 0,
  TRIM_LEADING  = 1 << 0,
  TRIM_TRAILING = 1 << 1,
  TRIM_ALL      = TRIM_LEADING | TRIM_TRAILING,
};

// Removes characters in |trim_chars| from the beginning and end of |input|.
// The 8-bit version only works on 8-bit characters, not UTF-8. Returns true if
// any characters were removed.
//
// It is safe to use the same variable for both |input| and |output| (this is
// the normal usage to trim in-place).
BASE_EXPORT bool TrimString(StringPiece16 input,
                            StringPiece16 trim_chars,
                            std::u16string* output);
BASE_EXPORT bool TrimString(StringPiece input,
                            StringPiece trim_chars,
                            std::string* output);

// StringPiece versions of the above. The returned pieces refer to the original
// buffer.
BASE_EXPORT StringPiece16 TrimString(StringPiece16 input,
                                     StringPiece16 trim_chars,
                                     TrimPositions positions);
BASE_EXPORT StringPiece TrimString(StringPiece input,
                                   StringPiece trim_chars,
                                   TrimPositions positions);

// Truncates a string to the nearest UTF-8 character that will leave
// the string less than or equal to the specified byte size.
BASE_EXPORT void TruncateUTF8ToByteSize(const std::string& input,
                                        const size_t byte_size,
                                        std::string* output);

// Trims any whitespace from either end of the input string.
//
// The StringPiece versions return a substring referencing the input buffer.
// The ASCII versions look only for ASCII whitespace.
//
// The std::string versions return where whitespace was found.
// NOTE: Safe to use the same variable for both input and output.
BASE_EXPORT TrimPositions TrimWhitespace(StringPiece16 input,
                                         TrimPositions positions,
                                         std::u16string* output);
BASE_EXPORT StringPiece16 TrimWhitespace(StringPiece16 input,
                                         TrimPositions positions);
BASE_EXPORT TrimPositions TrimWhitespaceASCII(StringPiece input,
                                              TrimPositions positions,
                                              std::string* output);
BASE_EXPORT StringPiece TrimWhitespaceASCII(StringPiece input,
                                            TrimPositions positions);

// Searches for CR or LF characters.  Removes all contiguous whitespace
// strings that contain them.  This is useful when trying to deal with text
// copied from terminals.
// Returns |text|, with the following three transformations:
// (1) Leading and trailing whitespace is trimmed.
// (2) If |trim_sequences_with_line_breaks| is true, any other whitespace
//     sequences containing a CR or LF are trimmed.
// (3) All other whitespace sequences are converted to single spaces.
BASE_EXPORT std::u16string CollapseWhitespace(
    StringPiece16 text,
    bool trim_sequences_with_line_breaks);
BASE_EXPORT std::string CollapseWhitespaceASCII(
    StringPiece text,
    bool trim_sequences_with_line_breaks);

// Returns true if |input| is empty or contains only characters found in
// |characters|.
BASE_EXPORT bool ContainsOnlyChars(StringPiece input, StringPiece characters);
BASE_EXPORT bool ContainsOnlyChars(StringPiece16 input,
                                   StringPiece16 characters);

// Returns true if |str| is structurally valid UTF-8 and also doesn't
// contain any non-character code point (e.g. U+10FFFE). Prohibiting
// non-characters increases the likelihood of detecting non-UTF-8 in
// real-world text, for callers which do not need to accept
// non-characters in strings.
BASE_EXPORT bool IsStringUTF8(StringPiece str);

// Returns true if |str| contains valid UTF-8, allowing non-character
// code points.
BASE_EXPORT bool IsStringUTF8AllowingNoncharacters(StringPiece str);

// Returns true if |str| contains only valid ASCII character values.
// Note 1: IsStringASCII executes in time determined solely by the
// length of the string, not by its contents, so it is robust against
// timing attacks for all strings of equal length.
// Note 2: IsStringASCII assumes the input is likely all ASCII, and
// does not leave early if it is not the case.
BASE_EXPORT bool IsStringASCII(StringPiece str);
BASE_EXPORT bool IsStringASCII(StringPiece16 str);

#if defined(WCHAR_T_IS_UTF32)
BASE_EXPORT bool IsStringASCII(WStringPiece str);
#endif

// Compare the lower-case form of the given string against the given
// previously-lower-cased ASCII string (typically a constant).
BASE_EXPORT bool LowerCaseEqualsASCII(StringPiece str,
                                      StringPiece lowercase_ascii);
BASE_EXPORT bool LowerCaseEqualsASCII(StringPiece16 str,
                                      StringPiece lowercase_ascii);

// Performs a case-sensitive string compare of the given 16-bit string against
// the given 8-bit ASCII string (typically a constant). The behavior is
// undefined if the |ascii| string is not ASCII.
BASE_EXPORT bool EqualsASCII(StringPiece16 str, StringPiece ascii);

// Indicates case sensitivity of comparisons. Only ASCII case insensitivity
// is supported. Full Unicode case-insensitive conversions would need to go in
// base/i18n so it can use ICU.
//
// If you need to do Unicode-aware case-insensitive StartsWith/EndsWith, it's
// best to call base::i18n::ToLower() or base::i18n::FoldCase() (see
// base/i18n/case_conversion.h for usage advice) on the arguments, and then use
// the results to a case-sensitive comparison.
enum class CompareCase {
  SENSITIVE,
  INSENSITIVE_ASCII,
};

BASE_EXPORT bool StartsWith(
    StringPiece str,
    StringPiece search_for,
    CompareCase case_sensitivity = CompareCase::SENSITIVE);
BASE_EXPORT bool StartsWith(
    StringPiece16 str,
    StringPiece16 search_for,
    CompareCase case_sensitivity = CompareCase::SENSITIVE);
BASE_EXPORT bool EndsWith(
    StringPiece str,
    StringPiece search_for,
    CompareCase case_sensitivity = CompareCase::SENSITIVE);
BASE_EXPORT bool EndsWith(
    StringPiece16 str,
    StringPiece16 search_for,
    CompareCase case_sensitivity = CompareCase::SENSITIVE);

// Determines the type of ASCII character, independent of locale (the C
// library versions will change based on locale).
template <typename Char>
inline bool IsAsciiWhitespace(Char c) {
  return c == ' ' || c == '\r' || c == '\n' || c == '\t' || c == '\f';
}
template <typename Char>
inline bool IsAsciiAlpha(Char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
template <typename Char>
inline bool IsAsciiUpper(Char c) {
  return c >= 'A' && c <= 'Z';
}
template <typename Char>
inline bool IsAsciiLower(Char c) {
  return c >= 'a' && c <= 'z';
}
template <typename Char>
inline bool IsAsciiDigit(Char c) {
  return c >= '0' && c <= '9';
}
template <typename Char>
inline bool IsAsciiPrintable(Char c) {
  return c >= ' ' && c <= '~';
}

template <typename Char>
inline bool IsHexDigit(Char c) {
  return (c >= '0' && c <= '9') ||
         (c >= 'A' && c <= 'F') ||
         (c >= 'a' && c <= 'f');
}

// Returns the integer corresponding to the given hex character. For example:
//    '4' -> 4
//    'a' -> 10
//    'B' -> 11
// Assumes the input is a valid hex character. DCHECKs in debug builds if not.
BASE_EXPORT char HexDigitToInt(wchar_t c);

// Returns true if it's a Unicode whitespace character.
BASE_EXPORT bool IsUnicodeWhitespace(wchar_t c);

// Return a byte string in human-readable format with a unit suffix. Not
// appropriate for use in any UI; use of FormatBytes and friends in ui/base is
// highly recommended instead. TODO(avi): Figure out how to get callers to use
// FormatBytes instead; remove this.
BASE_EXPORT std::u16string FormatBytesUnlocalized(int64_t bytes);

// Starting at |start_offset| (usually 0), replace the first instance of
// |find_this| with |replace_with|.
BASE_EXPORT void ReplaceFirstSubstringAfterOffset(std::u16string* str,
                                                  size_t start_offset,
                                                  StringPiece16 find_this,
                                                  StringPiece16 replace_with);
BASE_EXPORT void ReplaceFirstSubstringAfterOffset(
    std::string* str,
    size_t start_offset,
    StringPiece find_this,
    StringPiece replace_with);

// Starting at |start_offset| (usually 0), look through |str| and replace all
// instances of |find_this| with |replace_with|.
//
// This does entire substrings; use std::replace in <algorithm> for single
// characters, for example:
//   std::replace(str.begin(), str.end(), 'a', 'b');
BASE_EXPORT void ReplaceSubstringsAfterOffset(std::u16string* str,
                                              size_t start_offset,
                                              StringPiece16 find_this,
                                              StringPiece16 replace_with);
BASE_EXPORT void ReplaceSubstringsAfterOffset(
    std::string* str,
    size_t start_offset,
    StringPiece find_this,
    StringPiece replace_with);

// Reserves enough memory in |str| to accommodate |length_with_null| characters,
// sets the size of |str| to |length_with_null - 1| characters, and returns a
// pointer to the underlying contiguous array of characters.  This is typically
// used when calling a function that writes results into a character array, but
// the caller wants the data to be managed by a string-like object.  It is
// convenient in that is can be used inline in the call, and fast in that it
// avoids copying the results of the call from a char* into a string.
//
// Internally, this takes linear time because the resize() call 0-fills the
// underlying array for potentially all
// (|length_with_null - 1| * sizeof(string_type::value_type)) bytes.  Ideally we
// could avoid this aspect of the resize() call, as we expect the caller to
// immediately write over this memory, but there is no other way to set the size
// of the string, and not doing that will mean people who access |str| rather
// than str.c_str() will get back a string of whatever size |str| had on entry
// to this function (probably 0).
BASE_EXPORT char* WriteInto(std::string* str, size_t length_with_null);
BASE_EXPORT char16_t* WriteInto(std::u16string* str, size_t length_with_null);

// Joins a list of strings into a single string, inserting |separator| (which
// may be empty) in between all elements.
//
// Note this is inverse of SplitString()/SplitStringPiece() defined in
// string_split.h.
//
// If possible, callers should build a vector of StringPieces and use the
// StringPiece variant, so that they do not create unnecessary copies of
// strings. For example, instead of using SplitString, modifying the vector,
// then using JoinString, use SplitStringPiece followed by JoinString so that no
// copies of those strings are created until the final join operation.
//
// Use StrCat (in base/strings/strcat.h) if you don't need a separator.
BASE_EXPORT std::string JoinString(span<const std::string> parts,
                                   StringPiece separator);
BASE_EXPORT std::u16string JoinString(span<const std::u16string> parts,
                                      StringPiece16 separator);
BASE_EXPORT std::string JoinString(span<const StringPiece> parts,
                                   StringPiece separator);
BASE_EXPORT std::u16string JoinString(span<const StringPiece16> parts,
                                      StringPiece16 separator);
// Explicit initializer_list overloads are required to break ambiguity when used
// with a literal initializer list (otherwise the compiler would not be able to
// decide between the string and StringPiece overloads).
BASE_EXPORT std::string JoinString(std::initializer_list<StringPiece> parts,
                                   StringPiece separator);
BASE_EXPORT std::u16string JoinString(
    std::initializer_list<StringPiece16> parts,
    StringPiece16 separator);

// Replace $1-$2-$3..$9 in the format string with values from |subst|.
// Additionally, any number of consecutive '$' characters is replaced by that
// number less one. Eg $$->$, $$$->$$, etc. The offsets parameter here can be
// NULL. This only allows you to use up to nine replacements.
BASE_EXPORT std::u16string ReplaceStringPlaceholders(
    StringPiece16 format_string,
    const std::vector<std::u16string>& subst,
    std::vector<size_t>* offsets);

BASE_EXPORT std::string ReplaceStringPlaceholders(
    StringPiece format_string,
    const std::vector<std::string>& subst,
    std::vector<size_t>* offsets);

// Single-string shortcut for ReplaceStringHolders. |offset| may be NULL.
BASE_EXPORT std::u16string ReplaceStringPlaceholders(
    const std::u16string& format_string,
    const std::u16string& a,
    size_t* offset);

}  // namespace base

#if defined(OS_WIN)
#include "base/strings/string_util_win.h"
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
#include "base/strings/string_util_posix.h"
#else
#error Define string operations appropriately for your platform
#endif

#endif  // BASE_STRINGS_STRING_UTIL_H_
