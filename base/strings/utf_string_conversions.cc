// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"

#include <limits.h>
#include <stdint.h>

#include <algorithm>
#include <atomic>
#include <concepts>
#include <ostream>
#include <string_view>
#include <type_traits>

#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_ostream_operators.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/third_party/icu/icu_utf.h"
#include "build/build_config.h"

namespace base {

namespace {

std::atomic_bool g_is_fast_ascii_enabled{false};

bool IsFastConversionForASCIIEnabled() {
  return g_is_fast_ascii_enabled.load(std::memory_order_relaxed);
}

constexpr base_icu::UChar32 kErrorCodePoint = 0xFFFD;

// Size coefficient ----------------------------------------------------------
// The maximum number of codeunits in the destination encoding corresponding to
// one codeunit in the source encoding.

template <typename SrcChar, typename DestChar>
struct SizeCoefficient {
  static_assert(sizeof(SrcChar) < sizeof(DestChar),
                "Default case: from a smaller encoding to the bigger one");

  // ASCII symbols are encoded by one codeunit in all encodings.
  static constexpr int value = 1;
};

template <>
struct SizeCoefficient<char16_t, char> {
  // One UTF-16 codeunit corresponds to at most 3 codeunits in UTF-8.
  static constexpr int value = 3;
};

#if defined(WCHAR_T_IS_32_BIT)
template <>
struct SizeCoefficient<wchar_t, char> {
  // UTF-8 uses at most 4 codeunits per character.
  static constexpr int value = 4;
};

template <>
struct SizeCoefficient<wchar_t, char16_t> {
  // UTF-16 uses at most 2 codeunits per character.
  static constexpr int value = 2;
};
#endif  // defined(WCHAR_T_IS_32_BIT)

template <typename SrcChar, typename DestChar>
constexpr int size_coefficient_v =
    SizeCoefficient<std::decay_t<SrcChar>, std::decay_t<DestChar>>::value;

// UnicodeAppendUnsafe --------------------------------------------------------
// Function overloads that write code_point to the output string. Output string
// has to have enough space for the codepoint.

// Convenience typedef that checks whether the passed in type is integral (i.e.
// bool, char, int or their extended versions) and is of the correct size.
template <typename Char, size_t N>
concept BitsAre = std::integral<Char> && CHAR_BIT * sizeof(Char) == N;

template <typename Char>
  requires(BitsAre<Char, 8>)
void UnicodeAppendUnsafe(Char* out,
                         size_t* size,
                         base_icu::UChar32 code_point) {
  UNSAFE_TODO(
      CBU8_APPEND_UNSAFE(reinterpret_cast<uint8_t*>(out), *size, code_point));
}

template <typename Char>
  requires(BitsAre<Char, 16>)
void UnicodeAppendUnsafe(Char* out,
                         size_t* size,
                         base_icu::UChar32 code_point) {
  UNSAFE_TODO(CBU16_APPEND_UNSAFE(out, *size, code_point));
}

template <typename Char>
  requires(BitsAre<Char, 32>)
void UnicodeAppendUnsafe(Char* out,
                         size_t* size,
                         base_icu::UChar32 code_point) {
  UNSAFE_TODO(out[(*size)++]) = static_cast<Char>(code_point);
}

// DoUTFConversion ------------------------------------------------------------
// Main driver of UTFConversion specialized for different Src encodings.
// dest has to have enough room for the converted text.

template <typename DestChar>
bool DoUTFConversion(const char* src,
                     size_t src_len,
                     DestChar* dest,
                     size_t* dest_len,
                     bool fast_ascii) {
  bool success = true;

  for (size_t i = 0; i < src_len;) {
    if (fast_ascii && UNSAFE_TODO(static_cast<uint8_t>(src[i])) <= 0x7F) {
      // If the next codepoint is ASCII, it's likely that there are more ASCII
      // characters following it. Find the end of this ASCII range and directly
      // perform a widening copy instead of passing it to the full decoder.
      size_t ascii_len = FindFirstNonASCII(
          UNSAFE_TODO(std::string_view(src + i, src_len - i)));
      UNSAFE_TODO(std::copy_n(src + i, ascii_len, dest + *dest_len));
      i += ascii_len;
      *dest_len += ascii_len;
      continue;
    }

    base_icu::UChar32 code_point;
    UNSAFE_TODO(CBU8_NEXT(reinterpret_cast<const uint8_t*>(src), i, src_len,
                          code_point));

    if (!IsValidCodepoint(code_point)) {
      success = false;
      code_point = kErrorCodePoint;
    }

    UnicodeAppendUnsafe(dest, dest_len, code_point);
  }

  return success;
}

template <typename DestChar>
bool DoUTFConversion(const char16_t* src,
                     size_t src_len,
                     DestChar* dest,
                     size_t* dest_len,
                     bool fast_ascii) {
  bool success = true;

  auto ConvertSingleChar = [&success](char16_t in) -> base_icu::UChar32 {
    if (!CBU16_IS_SINGLE(in) || !IsValidCodepoint(in)) {
      success = false;
      return kErrorCodePoint;
    }
    return in;
  };

  size_t i = 0;
  while (i + 1 < src_len) {
    if (fast_ascii && UNSAFE_TODO(src[i]) <= 0x7F) {
      // If the next codepoint is ASCII, it's likely that there are more ASCII
      // characters following it. Find the end of this ASCII range and directly
      // perform a copy instead of passing it to the full decoder.
      size_t ascii_len = FindFirstNonASCII(
          UNSAFE_TODO(std::u16string_view(src + i, src_len - i)));
      UNSAFE_TODO(std::copy_n(src + i, ascii_len, dest + *dest_len));
      i += ascii_len;
      *dest_len += ascii_len;
      continue;
    }

    base_icu::UChar32 code_point;

    if (UNSAFE_TODO(CBU16_IS_LEAD(src[i])) &&
        UNSAFE_TODO(CBU16_IS_TRAIL(src[i + 1]))) {
      code_point = UNSAFE_TODO(CBU16_GET_SUPPLEMENTARY(src[i], src[i + 1]));
      if (!IsValidCodepoint(code_point)) {
        code_point = kErrorCodePoint;
        success = false;
      }
      i += 2;
    } else {
      code_point = ConvertSingleChar(UNSAFE_TODO(src[i]));
      ++i;
    }

    UnicodeAppendUnsafe(dest, dest_len, code_point);
  }

  if (i < src_len) {
    UnicodeAppendUnsafe(dest, dest_len, ConvertSingleChar(UNSAFE_TODO(src[i])));
  }

  return success;
}

#if defined(WCHAR_T_IS_32_BIT)

template <typename DestChar>
bool DoUTFConversion(const wchar_t* src,
                     size_t src_len,
                     DestChar* dest,
                     size_t* dest_len,
                     bool fast_ascii) {
  bool success = true;

  for (size_t i = 0; i < src_len; ++i) {
    auto code_point = static_cast<base_icu::UChar32>(UNSAFE_TODO(src[i]));

    if (!IsValidCodepoint(code_point)) {
      success = false;
      code_point = kErrorCodePoint;
    }

    UnicodeAppendUnsafe(dest, dest_len, code_point);
  }

  return success;
}

#endif  // defined(WCHAR_T_IS_32_BIT)

// UTFConversion --------------------------------------------------------------
// Function template for generating all UTF conversions.

template <typename InputString, typename DestString>
bool UTFConversion(const InputString& src_str, DestString* dest_str) {
  const bool fast_ascii = IsFastConversionForASCIIEnabled();
  if (fast_ascii) {
    // Find the first non ASCII character in this string. If there isn't any,
    // the string can just be directly copied to the destination. Otherwise, we
    // can copy the ASCII preamble to the destination and keep processing the
    // rest of the string. This is more performant for full ASCII string than
    // just passing to DoUTFConversion (which also has an ASCII fast path)
    // because it allows allocating a destination string of the exact right size
    // instead of allocating a string able to fit the worst case destination
    // size and shrinking.
    size_t first_non_ascii = FindFirstNonASCII(src_str);
    if (first_non_ascii == src_str.length()) {
      dest_str->assign_range(src_str);
      return true;
    }

    size_t src_len = src_str.length();
    size_t remaining_len = src_len - first_non_ascii;
    size_t max_dest_len =
        first_non_ascii +
        remaining_len * size_coefficient_v<typename InputString::value_type,
                                           typename DestString::value_type>;

    dest_str->resize(max_dest_len);
    std::copy_n(src_str.data(), first_non_ascii, dest_str->begin());

    // SAFETY: dest_str is of length max_dest_len, which is at least large
    // enough to contain the ASCII preamble + the worst case result of the
    // decoding operation. max_dest_len is guaranteed to be larger than
    // first_non_ascii, so dest_str->data() + first_non_ascii is safe, and the
    // resulting pointer points to a buffer that is guaranteed to be large
    // enough to contain the result of the decoding operation. This code uses
    // direct pointer arithmetic for performance.
    auto* dest = UNSAFE_BUFFERS(dest_str->data() + first_non_ascii);
    size_t dest_len = 0;

    // SAFETY: See other UNSAFE_BUFFERS call above.
    bool res = DoUTFConversion(UNSAFE_BUFFERS(src_str.data() + first_non_ascii),
                               remaining_len, dest, &dest_len, fast_ascii);

    dest_str->resize(first_non_ascii + dest_len);
    dest_str->shrink_to_fit();
    return res;
  }

  if (IsStringASCII(src_str)) {
    dest_str->assign(src_str.begin(), src_str.end());
    return true;
  }

  dest_str->resize(src_str.length() *
                   size_coefficient_v<typename InputString::value_type,
                                      typename DestString::value_type>);

  // Empty string is ASCII => it OK to call operator[].
  auto* dest = &(*dest_str)[0];

  // ICU requires 32 bit numbers.
  size_t src_len = src_str.length();
  size_t dest_len = 0;

  bool res =
      DoUTFConversion(src_str.data(), src_len, dest, &dest_len, fast_ascii);

  dest_str->resize(dest_len);
  dest_str->shrink_to_fit();

  return res;
}

}  // namespace

// UTF16 <-> UTF8 --------------------------------------------------------------

bool UTF8ToUTF16(const char* src, size_t src_len, std::u16string* output) {
  return UTFConversion(std::string_view(src, src_len), output);
}

std::u16string UTF8ToUTF16(std::string_view utf8) {
  std::u16string ret;
  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  UTF8ToUTF16(utf8.data(), utf8.size(), &ret);
  return ret;
}

bool UTF16ToUTF8(const char16_t* src, size_t src_len, std::string* output) {
  return UTFConversion(std::u16string_view(src, src_len), output);
}

std::string UTF16ToUTF8(std::u16string_view utf16) {
  std::string ret;
  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  UTF16ToUTF8(utf16.data(), utf16.length(), &ret);
  return ret;
}

// UTF-16 <-> Wide -------------------------------------------------------------

#if defined(WCHAR_T_IS_16_BIT)
// When wide == UTF-16 the conversions are a NOP.

bool WideToUTF16(const wchar_t* src, size_t src_len, std::u16string* output) {
  output->assign(src, UNSAFE_TODO(src + src_len));
  return true;
}

std::u16string WideToUTF16(std::wstring_view wide) {
  return std::u16string(wide.begin(), wide.end());
}

bool UTF16ToWide(const char16_t* src, size_t src_len, std::wstring* output) {
  output->assign(src, UNSAFE_TODO(src + src_len));
  return true;
}

std::wstring UTF16ToWide(std::u16string_view utf16) {
  return std::wstring(utf16.begin(), utf16.end());
}

#elif defined(WCHAR_T_IS_32_BIT)

bool WideToUTF16(const wchar_t* src, size_t src_len, std::u16string* output) {
  return UTFConversion(std::wstring_view(src, src_len), output);
}

std::u16string WideToUTF16(std::wstring_view wide) {
  std::u16string ret;
  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  WideToUTF16(wide.data(), wide.length(), &ret);
  return ret;
}

bool UTF16ToWide(const char16_t* src, size_t src_len, std::wstring* output) {
  return UTFConversion(std::u16string_view(src, src_len), output);
}

std::wstring UTF16ToWide(std::u16string_view utf16) {
  std::wstring ret;
  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  UTF16ToWide(utf16.data(), utf16.length(), &ret);
  return ret;
}

#endif  // defined(WCHAR_T_IS_32_BIT)

// UTF-8 <-> Wide --------------------------------------------------------------

// UTF8ToWide is the same code, regardless of whether wide is 16 or 32 bits

bool UTF8ToWide(const char* src, size_t src_len, std::wstring* output) {
  return UTFConversion(std::string_view(src, src_len), output);
}

std::wstring UTF8ToWide(std::string_view utf8) {
  std::wstring ret;
  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  UTF8ToWide(utf8.data(), utf8.length(), &ret);
  return ret;
}

#if defined(WCHAR_T_IS_16_BIT)
// Easy case since we can use the "utf" versions we already wrote above.

bool WideToUTF8(const wchar_t* src, size_t src_len, std::string* output) {
  return UTF16ToUTF8(as_u16cstr(src), src_len, output);
}

std::string WideToUTF8(std::wstring_view wide) {
  return UTF16ToUTF8(std::u16string_view(as_u16cstr(wide), wide.size()));
}

#elif defined(WCHAR_T_IS_32_BIT)

bool WideToUTF8(const wchar_t* src, size_t src_len, std::string* output) {
  return UTFConversion(std::wstring_view(src, src_len), output);
}

std::string WideToUTF8(std::wstring_view wide) {
  std::string ret;
  // Ignore the success flag of this call, it will do the best it can for
  // invalid input, which is what we want here.
  WideToUTF8(wide.data(), wide.length(), &ret);
  return ret;
}

#endif  // defined(WCHAR_T_IS_32_BIT)

std::u16string ASCIIToUTF16(std::string_view ascii) {
  DCHECK(IsStringASCII(ascii)) << ascii;
  return std::u16string(ascii.begin(), ascii.end());
}

std::string UTF16ToASCII(std::u16string_view utf16) {
  DCHECK(IsStringASCII(utf16)) << UTF16ToUTF8(utf16);
  return std::string(utf16.begin(), utf16.end());
}

#if defined(WCHAR_T_IS_16_BIT)
std::wstring ASCIIToWide(std::string_view ascii) {
  DCHECK(IsStringASCII(ascii)) << ascii;
  return std::wstring(ascii.begin(), ascii.end());
}

std::string WideToASCII(std::wstring_view wide) {
  DCHECK(IsStringASCII(wide)) << wide;
  return std::string(wide.begin(), wide.end());
}
#endif  // defined(WCHAR_T_IS_16_BIT)

namespace strings_internal {

void InitializeUtfStringConversionsFeatures() {
  // Some targets (like the v8 snapshot creator) use this UTF conversion code
  // without setting up the feature list. It's fine for UTF8 decoding to be
  // slightly slower until the feature list is ready.
  g_is_fast_ascii_enabled.store(
      FeatureList::GetInstance() &&
          FeatureList::IsEnabled(features::kUtfConversionAsciiFastPath),
      std::memory_order_relaxed);
}

}  // namespace strings_internal

}  // namespace base
