// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/strings/string_util.h"

#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <type_traits>

#include "base/bits.h"
#include "base/strings/utf_ostream_operators.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace base {

namespace {

const struct trim_case {
  const wchar_t* input;
  const TrimPositions positions;
  const wchar_t* output;
  const TrimPositions return_value;
} trim_cases[] = {
    {L" Google Video ", TRIM_LEADING, L"Google Video ", TRIM_LEADING},
    {L" Google Video ", TRIM_TRAILING, L" Google Video", TRIM_TRAILING},
    {L" Google Video ", TRIM_ALL, L"Google Video", TRIM_ALL},
    {L"Google Video", TRIM_ALL, L"Google Video", TRIM_NONE},
    {L"", TRIM_ALL, L"", TRIM_NONE},
    {L"  ", TRIM_LEADING, L"", TRIM_LEADING},
    {L"  ", TRIM_TRAILING, L"", TRIM_TRAILING},
    {L"  ", TRIM_ALL, L"", TRIM_ALL},
    {L"\t\rTest String\n", TRIM_ALL, L"Test String", TRIM_ALL},
    {L"\x2002Test String\x00A0\x3000", TRIM_ALL, L"Test String", TRIM_ALL},
};

const struct trim_case_ascii {
  const char* input;
  const TrimPositions positions;
  const char* output;
  const TrimPositions return_value;
} trim_cases_ascii[] = {
    {" Google Video ", TRIM_LEADING, "Google Video ", TRIM_LEADING},
    {" Google Video ", TRIM_TRAILING, " Google Video", TRIM_TRAILING},
    {" Google Video ", TRIM_ALL, "Google Video", TRIM_ALL},
    {"Google Video", TRIM_ALL, "Google Video", TRIM_NONE},
    {"", TRIM_ALL, "", TRIM_NONE},
    {"  ", TRIM_LEADING, "", TRIM_LEADING},
    {"  ", TRIM_TRAILING, "", TRIM_TRAILING},
    {"  ", TRIM_ALL, "", TRIM_ALL},
    {"\t\rTest String\n", TRIM_ALL, "Test String", TRIM_ALL},
};

// Helper used to test TruncateUTF8ToByteSize.
bool Truncated(const std::string& input,
               const size_t byte_size,
               std::string* output) {
    size_t prev = input.length();
    TruncateUTF8ToByteSize(input, byte_size, output);
    return prev != output->length();
}

using TestFunction = bool (*)(std::string_view str);

// Helper used to test IsStringUTF8[AllowingNoncharacters].
void TestStructurallyValidUtf8(TestFunction fn) {
  EXPECT_TRUE(fn("abc"));
  EXPECT_TRUE(fn("\xC2\x81"));
  EXPECT_TRUE(fn("\xE1\x80\xBF"));
  EXPECT_TRUE(fn("\xF1\x80\xA0\xBF"));
  EXPECT_TRUE(fn("\xF1\x80\xA0\xBF"));
  EXPECT_TRUE(fn("a\xC2\x81\xE1\x80\xBF\xF1\x80\xA0\xBF"));

  // U+FEFF used as UTF-8 BOM.
  // clang-format off
  EXPECT_TRUE(fn("\xEF\xBB\xBF" "abc"));
  // clang-format on

  // Embedded nulls in canonical UTF-8 representation.
  using std::string_literals::operator""s;
  const std::string kEmbeddedNull = "embedded\0null"s;
  EXPECT_TRUE(fn(kEmbeddedNull));
}

// Helper used to test IsStringUTF8[AllowingNoncharacters].
void TestStructurallyInvalidUtf8(TestFunction fn) {
  // Invalid encoding of U+1FFFE (0x8F instead of 0x9F)
  EXPECT_FALSE(fn("\xF0\x8F\xBF\xBE"));

  // Surrogate code points
  EXPECT_FALSE(fn("\xED\xA0\x80\xED\xBF\xBF"));
  EXPECT_FALSE(fn("\xED\xA0\x8F"));
  EXPECT_FALSE(fn("\xED\xBF\xBF"));

  // Overlong sequences
  EXPECT_FALSE(fn("\xC0\x80"));                  // U+0000
  EXPECT_FALSE(fn("\xC1\x80\xC1\x81"));          // "AB"
  EXPECT_FALSE(fn("\xE0\x80\x80"));              // U+0000
  EXPECT_FALSE(fn("\xE0\x82\x80"));              // U+0080
  EXPECT_FALSE(fn("\xE0\x9F\xBF"));              // U+07FF
  EXPECT_FALSE(fn("\xF0\x80\x80\x8D"));          // U+000D
  EXPECT_FALSE(fn("\xF0\x80\x82\x91"));          // U+0091
  EXPECT_FALSE(fn("\xF0\x80\xA0\x80"));          // U+0800
  EXPECT_FALSE(fn("\xF0\x8F\xBB\xBF"));          // U+FEFF (BOM)
  EXPECT_FALSE(fn("\xF8\x80\x80\x80\xBF"));      // U+003F
  EXPECT_FALSE(fn("\xFC\x80\x80\x80\xA0\xA5"));  // U+00A5

  // Beyond U+10FFFF (the upper limit of Unicode codespace)
  EXPECT_FALSE(fn("\xF4\x90\x80\x80"));          // U+110000
  EXPECT_FALSE(fn("\xF8\xA0\xBF\x80\xBF"));      // 5 bytes
  EXPECT_FALSE(fn("\xFC\x9C\xBF\x80\xBF\x80"));  // 6 bytes

  // BOM in UTF-16(BE|LE)
  EXPECT_FALSE(fn("\xFE\xFF"));
  EXPECT_FALSE(fn("\xFF\xFE"));

  // Strings in legacy encodings. We can certainly make up strings
  // in a legacy encoding that are valid in UTF-8, but in real data,
  // most of them are invalid as UTF-8.

  // cafe with U+00E9 in ISO-8859-1
  EXPECT_FALSE(fn("caf\xE9"));
  // U+AC00, U+AC001 in EUC-KR
  EXPECT_FALSE(fn("\xB0\xA1\xB0\xA2"));
  // U+4F60 U+597D in Big5
  EXPECT_FALSE(fn("\xA7\x41\xA6\x6E"));
  // "abc" with U+201[CD] in windows-125[0-8]
  // clang-format off
  EXPECT_FALSE(fn("\x93" "abc\x94"));
  // clang-format on
  // U+0639 U+064E U+0644 U+064E in ISO-8859-6
  EXPECT_FALSE(fn("\xD9\xEE\xE4\xEE"));
  // U+03B3 U+03B5 U+03B9 U+03AC in ISO-8859-7
  EXPECT_FALSE(fn("\xE3\xE5\xE9\xDC"));

  // BOM in UTF-32(BE|LE)
  using std::string_literals::operator""s;
  const std::string kUtf32BeBom = "\x00\x00\xFE\xFF"s;
  EXPECT_FALSE(fn(kUtf32BeBom));
  const std::string kUtf32LeBom = "\xFF\xFE\x00\x00"s;
  EXPECT_FALSE(fn(kUtf32LeBom));
}

// Helper used to test IsStringUTF8[AllowingNoncharacters].
void TestNoncharacters(TestFunction fn, bool expected_result) {
  EXPECT_EQ(fn("\xEF\xB7\x90"), expected_result);      // U+FDD0
  EXPECT_EQ(fn("\xEF\xB7\x9F"), expected_result);      // U+FDDF
  EXPECT_EQ(fn("\xEF\xB7\xAF"), expected_result);      // U+FDEF
  EXPECT_EQ(fn("\xEF\xBF\xBE"), expected_result);      // U+FFFE
  EXPECT_EQ(fn("\xEF\xBF\xBF"), expected_result);      // U+FFFF
  EXPECT_EQ(fn("\xF0\x9F\xBF\xBE"), expected_result);  // U+01FFFE
  EXPECT_EQ(fn("\xF0\x9F\xBF\xBF"), expected_result);  // U+01FFFF
  EXPECT_EQ(fn("\xF0\xAF\xBF\xBE"), expected_result);  // U+02FFFE
  EXPECT_EQ(fn("\xF0\xAF\xBF\xBF"), expected_result);  // U+02FFFF
  EXPECT_EQ(fn("\xF0\xBF\xBF\xBE"), expected_result);  // U+03FFFE
  EXPECT_EQ(fn("\xF0\xBF\xBF\xBF"), expected_result);  // U+03FFFF
  EXPECT_EQ(fn("\xF1\x8F\xBF\xBE"), expected_result);  // U+04FFFE
  EXPECT_EQ(fn("\xF1\x8F\xBF\xBF"), expected_result);  // U+04FFFF
  EXPECT_EQ(fn("\xF1\x9F\xBF\xBE"), expected_result);  // U+05FFFE
  EXPECT_EQ(fn("\xF1\x9F\xBF\xBF"), expected_result);  // U+05FFFF
  EXPECT_EQ(fn("\xF1\xAF\xBF\xBE"), expected_result);  // U+06FFFE
  EXPECT_EQ(fn("\xF1\xAF\xBF\xBF"), expected_result);  // U+06FFFF
  EXPECT_EQ(fn("\xF1\xBF\xBF\xBE"), expected_result);  // U+07FFFE
  EXPECT_EQ(fn("\xF1\xBF\xBF\xBF"), expected_result);  // U+07FFFF
  EXPECT_EQ(fn("\xF2\x8F\xBF\xBE"), expected_result);  // U+08FFFE
  EXPECT_EQ(fn("\xF2\x8F\xBF\xBF"), expected_result);  // U+08FFFF
  EXPECT_EQ(fn("\xF2\x9F\xBF\xBE"), expected_result);  // U+09FFFE
  EXPECT_EQ(fn("\xF2\x9F\xBF\xBF"), expected_result);  // U+09FFFF
  EXPECT_EQ(fn("\xF2\xAF\xBF\xBE"), expected_result);  // U+0AFFFE
  EXPECT_EQ(fn("\xF2\xAF\xBF\xBF"), expected_result);  // U+0AFFFF
  EXPECT_EQ(fn("\xF2\xBF\xBF\xBE"), expected_result);  // U+0BFFFE
  EXPECT_EQ(fn("\xF2\xBF\xBF\xBF"), expected_result);  // U+0BFFFF
  EXPECT_EQ(fn("\xF3\x8F\xBF\xBE"), expected_result);  // U+0CFFFE
  EXPECT_EQ(fn("\xF3\x8F\xBF\xBF"), expected_result);  // U+0CFFFF
  EXPECT_EQ(fn("\xF3\x9F\xBF\xBE"), expected_result);  // U+0DFFFE
  EXPECT_EQ(fn("\xF3\x9F\xBF\xBF"), expected_result);  // U+0DFFFF
  EXPECT_EQ(fn("\xF3\xAF\xBF\xBE"), expected_result);  // U+0EFFFE
  EXPECT_EQ(fn("\xF3\xAF\xBF\xBF"), expected_result);  // U+0EFFFF
  EXPECT_EQ(fn("\xF3\xBF\xBF\xBE"), expected_result);  // U+0FFFFE
  EXPECT_EQ(fn("\xF3\xBF\xBF\xBF"), expected_result);  // U+0FFFFF
  EXPECT_EQ(fn("\xF4\x8F\xBF\xBE"), expected_result);  // U+10FFFE
  EXPECT_EQ(fn("\xF4\x8F\xBF\xBF"), expected_result);  // U+10FFFF
}

TEST(StringUtilTest, TruncateUTF8ToByteSize) {
  std::string output;

  // Empty strings and invalid byte_size arguments
  EXPECT_FALSE(Truncated(std::string(), 0, &output));
  EXPECT_EQ(output, "");
  EXPECT_TRUE(Truncated("\xe1\x80\xbf", 0, &output));
  EXPECT_EQ(output, "");
  EXPECT_FALSE(Truncated("\xe1\x80\xbf", static_cast<size_t>(-1), &output));
  EXPECT_FALSE(Truncated("\xe1\x80\xbf", 4, &output));

  // Testing the truncation of valid UTF8 correctly
  EXPECT_TRUE(Truncated("abc", 2, &output));
  EXPECT_EQ(output, "ab");
  EXPECT_TRUE(Truncated("\xc2\x81\xc2\x81", 2, &output));
  EXPECT_EQ(output.compare("\xc2\x81"), 0);
  EXPECT_TRUE(Truncated("\xc2\x81\xc2\x81", 3, &output));
  EXPECT_EQ(output.compare("\xc2\x81"), 0);
  EXPECT_FALSE(Truncated("\xc2\x81\xc2\x81", 4, &output));
  EXPECT_EQ(output.compare("\xc2\x81\xc2\x81"), 0);

  {
    const char array[] = "\x00\x00\xc2\x81\xc2\x81";
    const std::string array_string(array, std::size(array));
    EXPECT_TRUE(Truncated(array_string, 4, &output));
    EXPECT_EQ(output.compare(std::string("\x00\x00\xc2\x81", 4)), 0);
  }

  {
    const char array[] = "\x00\xc2\x81\xc2\x81";
    const std::string array_string(array, std::size(array));
    EXPECT_TRUE(Truncated(array_string, 4, &output));
    EXPECT_EQ(output.compare(std::string("\x00\xc2\x81", 3)), 0);
  }

  // Testing invalid UTF8
  EXPECT_TRUE(Truncated("\xed\xa0\x80\xed\xbf\xbf", 6, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xed\xa0\x8f", 3, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xed\xbf\xbf", 3, &output));
  EXPECT_EQ(output.compare(""), 0);

  // Testing invalid UTF8 mixed with valid UTF8
  EXPECT_FALSE(Truncated("\xe1\x80\xbf", 3, &output));
  EXPECT_EQ(output.compare("\xe1\x80\xbf"), 0);
  EXPECT_FALSE(Truncated("\xf1\x80\xa0\xbf", 4, &output));
  EXPECT_EQ(output.compare("\xf1\x80\xa0\xbf"), 0);
  EXPECT_FALSE(Truncated("a\xc2\x81\xe1\x80\xbf\xf1\x80\xa0\xbf",
              10, &output));
  EXPECT_EQ(output.compare("a\xc2\x81\xe1\x80\xbf\xf1\x80\xa0\xbf"), 0);
  EXPECT_TRUE(Truncated("a\xc2\x81\xe1\x80\xbf\xf1""a""\x80\xa0",
              10, &output));
  EXPECT_EQ(output.compare("a\xc2\x81\xe1\x80\xbf\xf1""a"), 0);
  EXPECT_FALSE(Truncated("\xef\xbb\xbf" "abc", 6, &output));
  EXPECT_EQ(output.compare("\xef\xbb\xbf" "abc"), 0);

  // Overlong sequences
  EXPECT_TRUE(Truncated("\xc0\x80", 2, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xc1\x80\xc1\x81", 4, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xe0\x80\x80", 3, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xe0\x82\x80", 3, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xe0\x9f\xbf", 3, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xf0\x80\x80\x8D", 4, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xf0\x80\x82\x91", 4, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xf0\x80\xa0\x80", 4, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xf0\x8f\xbb\xbf", 4, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xf8\x80\x80\x80\xbf", 5, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xfc\x80\x80\x80\xa0\xa5", 6, &output));
  EXPECT_EQ(output.compare(""), 0);

  // Beyond U+10FFFF (the upper limit of Unicode codespace)
  EXPECT_TRUE(Truncated("\xf4\x90\x80\x80", 4, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xf8\xa0\xbf\x80\xbf", 5, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xfc\x9c\xbf\x80\xbf\x80", 6, &output));
  EXPECT_EQ(output.compare(""), 0);

  // BOMs in UTF-16(BE|LE) and UTF-32(BE|LE)
  EXPECT_TRUE(Truncated("\xfe\xff", 2, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xff\xfe", 2, &output));
  EXPECT_EQ(output.compare(""), 0);

  {
    const char array[] = "\x00\x00\xfe\xff";
    const std::string array_string(array, std::size(array));
    EXPECT_TRUE(Truncated(array_string, 4, &output));
    EXPECT_EQ(output.compare(std::string("\x00\x00", 2)), 0);
  }

  // Variants on the previous test
  {
    const char array[] = "\xff\xfe\x00\x00";
    const std::string array_string(array, 4);
    EXPECT_FALSE(Truncated(array_string, 4, &output));
    EXPECT_EQ(output.compare(std::string("\xff\xfe\x00\x00", 4)), 0);
  }
  {
    const char array[] = "\xff\x00\x00\xfe";
    const std::string array_string(array, std::size(array));
    EXPECT_TRUE(Truncated(array_string, 4, &output));
    EXPECT_EQ(output.compare(std::string("\xff\x00\x00", 3)), 0);
  }

  // Non-characters : U+xxFFF[EF] where xx is 0x00 through 0x10 and <FDD0,FDEF>
  EXPECT_TRUE(Truncated("\xef\xbf\xbe", 3, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xf0\x8f\xbf\xbe", 4, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xf3\xbf\xbf\xbf", 4, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xef\xb7\x90", 3, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_TRUE(Truncated("\xef\xb7\xaf", 3, &output));
  EXPECT_EQ(output.compare(""), 0);

  // Strings in legacy encodings that are valid in UTF-8, but
  // are invalid as UTF-8 in real data.
  EXPECT_TRUE(Truncated("caf\xe9", 4, &output));
  EXPECT_EQ(output.compare("caf"), 0);
  EXPECT_TRUE(Truncated("\xb0\xa1\xb0\xa2", 4, &output));
  EXPECT_EQ(output.compare(""), 0);
  EXPECT_FALSE(Truncated("\xa7\x41\xa6\x6e", 4, &output));
  EXPECT_EQ(output.compare("\xa7\x41\xa6\x6e"), 0);
  EXPECT_TRUE(Truncated("\xa7\x41\xa6\x6e\xd9\xee\xe4\xee", 7,
              &output));
  EXPECT_EQ(output.compare("\xa7\x41\xa6\x6e"), 0);

  // Testing using the same string as input and output.
  EXPECT_FALSE(Truncated(output, 4, &output));
  EXPECT_EQ(output.compare("\xa7\x41\xa6\x6e"), 0);
  EXPECT_TRUE(Truncated(output, 3, &output));
  EXPECT_EQ(output.compare("\xa7\x41"), 0);

  // "abc" with U+201[CD] in windows-125[0-8]
  EXPECT_TRUE(Truncated("\x93" "abc\x94", 5, &output));
  EXPECT_EQ(output.compare("\x93" "abc"), 0);

  // U+0639 U+064E U+0644 U+064E in ISO-8859-6
  EXPECT_TRUE(Truncated("\xd9\xee\xe4\xee", 4, &output));
  EXPECT_EQ(output.compare(""), 0);

  // U+03B3 U+03B5 U+03B9 U+03AC in ISO-8859-7
  EXPECT_TRUE(Truncated("\xe3\xe5\xe9\xdC", 4, &output));
  EXPECT_EQ(output.compare(""), 0);
}

#if defined(WCHAR_T_IS_16_BIT)
TEST(StringUtilTest, as_wcstr) {
  char16_t rw_buffer[10] = {};
  static_assert(
      std::is_same_v<wchar_t*, decltype(as_writable_wcstr(rw_buffer))>, "");
  EXPECT_EQ(static_cast<void*>(rw_buffer), as_writable_wcstr(rw_buffer));

  std::u16string rw_str(10, '\0');
  static_assert(std::is_same_v<wchar_t*, decltype(as_writable_wcstr(rw_str))>,
                "");
  EXPECT_EQ(static_cast<const void*>(rw_str.data()), as_writable_wcstr(rw_str));

  const char16_t ro_buffer[10] = {};
  static_assert(std::is_same_v<const wchar_t*, decltype(as_wcstr(ro_buffer))>,
                "");
  EXPECT_EQ(static_cast<const void*>(ro_buffer), as_wcstr(ro_buffer));

  const std::u16string ro_str(10, '\0');
  static_assert(std::is_same_v<const wchar_t*, decltype(as_wcstr(ro_str))>, "");
  EXPECT_EQ(static_cast<const void*>(ro_str.data()), as_wcstr(ro_str));

  std::u16string_view piece = ro_buffer;
  static_assert(std::is_same_v<const wchar_t*, decltype(as_wcstr(piece))>, "");
  EXPECT_EQ(static_cast<const void*>(piece.data()), as_wcstr(piece));
}

TEST(StringUtilTest, as_u16cstr) {
  wchar_t rw_buffer[10] = {};
  static_assert(
      std::is_same_v<char16_t*, decltype(as_writable_u16cstr(rw_buffer))>, "");
  EXPECT_EQ(static_cast<void*>(rw_buffer), as_writable_u16cstr(rw_buffer));

  std::wstring rw_str(10, '\0');
  static_assert(
      std::is_same_v<char16_t*, decltype(as_writable_u16cstr(rw_str))>, "");
  EXPECT_EQ(static_cast<const void*>(rw_str.data()),
            as_writable_u16cstr(rw_str));

  const wchar_t ro_buffer[10] = {};
  static_assert(
      std::is_same_v<const char16_t*, decltype(as_u16cstr(ro_buffer))>, "");
  EXPECT_EQ(static_cast<const void*>(ro_buffer), as_u16cstr(ro_buffer));

  const std::wstring ro_str(10, '\0');
  static_assert(std::is_same_v<const char16_t*, decltype(as_u16cstr(ro_str))>,
                "");
  EXPECT_EQ(static_cast<const void*>(ro_str.data()), as_u16cstr(ro_str));

  std::wstring_view piece = ro_buffer;
  static_assert(std::is_same_v<const char16_t*, decltype(as_u16cstr(piece))>,
                "");
  EXPECT_EQ(static_cast<const void*>(piece.data()), as_u16cstr(piece));
}
#endif  // defined(WCHAR_T_IS_16_BIT)

TEST(StringUtilTest, TrimWhitespace) {
  std::u16string output;  // Allow contents to carry over to next testcase
  for (const auto& value : trim_cases) {
    EXPECT_EQ(value.return_value,
              TrimWhitespace(WideToUTF16(value.input), value.positions,
                             &output));
    EXPECT_EQ(WideToUTF16(value.output), output);
  }

  // Test that TrimWhitespace() can take the same string for input and output
  output = u"  This is a test \r\n";
  EXPECT_EQ(TRIM_ALL, TrimWhitespace(output, TRIM_ALL, &output));
  EXPECT_EQ(u"This is a test", output);

  // Once more, but with a string of whitespace
  output = u"  \r\n";
  EXPECT_EQ(TRIM_ALL, TrimWhitespace(output, TRIM_ALL, &output));
  EXPECT_EQ(std::u16string(), output);

  std::string output_ascii;
  for (const auto& value : trim_cases_ascii) {
    EXPECT_EQ(value.return_value,
              TrimWhitespaceASCII(value.input, value.positions, &output_ascii));
    EXPECT_EQ(value.output, output_ascii);
  }
}

static const struct collapse_case {
  const wchar_t* input;
  const bool trim;
  const wchar_t* output;
} collapse_cases[] = {
  {L" Google Video ", false, L"Google Video"},
  {L"Google Video", false, L"Google Video"},
  {L"", false, L""},
  {L"  ", false, L""},
  {L"\t\rTest String\n", false, L"Test String"},
  {L"\x2002Test String\x00A0\x3000", false, L"Test String"},
  {L"    Test     \n  \t String    ", false, L"Test String"},
  {L"\x2002Test\x1680 \x2028 \tString\x00A0\x3000", false, L"Test String"},
  {L"   Test String", false, L"Test String"},
  {L"Test String    ", false, L"Test String"},
  {L"Test String", false, L"Test String"},
  {L"", true, L""},
  {L"\n", true, L""},
  {L"  \r  ", true, L""},
  {L"\nFoo", true, L"Foo"},
  {L"\r  Foo  ", true, L"Foo"},
  {L" Foo bar ", true, L"Foo bar"},
  {L"  \tFoo  bar  \n", true, L"Foo bar"},
  {L" a \r b\n c \r\n d \t\re \t f \n ", true, L"abcde f"},
};

TEST(StringUtilTest, CollapseWhitespace) {
  for (const auto& value : collapse_cases) {
    EXPECT_EQ(WideToUTF16(value.output),
              CollapseWhitespace(WideToUTF16(value.input), value.trim));
  }
}

static const struct collapse_case_ascii {
  const char* input;
  const bool trim;
  const char* output;
} collapse_cases_ascii[] = {
    {" Google Video ", false, "Google Video"},
    {"Google Video", false, "Google Video"},
    {"", false, ""},
    {"  ", false, ""},
    {"\t\rTest String\n", false, "Test String"},
    {"    Test     \n  \t String    ", false, "Test String"},
    {"   Test String", false, "Test String"},
    {"Test String    ", false, "Test String"},
    {"Test String", false, "Test String"},
    {"", true, ""},
    {"\n", true, ""},
    {"  \r  ", true, ""},
    {"\nFoo", true, "Foo"},
    {"\r  Foo  ", true, "Foo"},
    {" Foo bar ", true, "Foo bar"},
    // \u00A0 is whitespace, but not _ASCII_ whitespace, so it should not be
    // collapsed by CollapseWhitespaceASCII().
    {"Foo\u00A0bar", true, "Foo\u00A0bar"},
    {"  \tFoo  bar  \n", true, "Foo bar"},
    {" a \r b\n c \r\n d \t\re \t f \n ", true, "abcde f"},
};

TEST(StringUtilTest, CollapseWhitespaceASCII) {
  for (const auto& value : collapse_cases_ascii) {
    EXPECT_EQ(value.output, CollapseWhitespaceASCII(value.input, value.trim));
  }
}

TEST(StringUtilTest, IsStringUTF8) {
  {
    SCOPED_TRACE("IsStringUTF8");
    TestStructurallyValidUtf8(&IsStringUTF8);
    TestStructurallyInvalidUtf8(&IsStringUTF8);
    TestNoncharacters(&IsStringUTF8, false);
  }

  {
    SCOPED_TRACE("IsStringUTF8AllowingNoncharacters");
    TestStructurallyValidUtf8(&IsStringUTF8AllowingNoncharacters);
    TestStructurallyInvalidUtf8(&IsStringUTF8AllowingNoncharacters);
    TestNoncharacters(&IsStringUTF8AllowingNoncharacters, true);
  }
}

TEST(StringUtilTest, IsStringASCII) {
  static char char_ascii[] =
      "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF";
  static char16_t char16_ascii[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8',
                                    '9', '0', 'A', 'B', 'C', 'D', 'E', 'F', '0',
                                    '1', '2', '3', '4', '5', '6', '7', '8', '9',
                                    '0', 'A', 'B', 'C', 'D', 'E', 'F', 0};
  static std::wstring wchar_ascii(
      L"0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF");

  // Test a variety of the fragment start positions and lengths in order to make
  // sure that bit masking in IsStringASCII works correctly.
  // Also, test that a non-ASCII character will be detected regardless of its
  // position inside the string.
  {
    const size_t string_length = std::size(char_ascii) - 1;
    for (size_t offset = 0; offset < 8; ++offset) {
      for (size_t len = 0, max_len = string_length - offset; len < max_len;
           ++len) {
        EXPECT_TRUE(IsStringASCII(std::string_view(char_ascii + offset, len)));
        for (size_t char_pos = offset; char_pos < len; ++char_pos) {
          char_ascii[char_pos] |= '\x80';
          EXPECT_FALSE(
              IsStringASCII(std::string_view(char_ascii + offset, len)));
          char_ascii[char_pos] &= ~'\x80';
        }
      }
    }
  }

  {
    const size_t string_length = std::size(char16_ascii) - 1;
    for (size_t offset = 0; offset < 4; ++offset) {
      for (size_t len = 0, max_len = string_length - offset; len < max_len;
           ++len) {
        EXPECT_TRUE(
            IsStringASCII(std::u16string_view(char16_ascii + offset, len)));
        for (size_t char_pos = offset; char_pos < len; ++char_pos) {
          char16_ascii[char_pos] |= 0x80;
          EXPECT_FALSE(
              IsStringASCII(std::u16string_view(char16_ascii + offset, len)));
          char16_ascii[char_pos] &= ~0x80;
          // Also test when the upper half is non-zero.
          char16_ascii[char_pos] |= 0x100;
          EXPECT_FALSE(
              IsStringASCII(std::u16string_view(char16_ascii + offset, len)));
          char16_ascii[char_pos] &= ~0x100;
        }
      }
    }
  }

#if defined(WCHAR_T_IS_32_BIT)
  {
    const size_t string_length = wchar_ascii.length();
    for (size_t len = 0; len < string_length; ++len) {
      EXPECT_TRUE(IsStringASCII(wchar_ascii.substr(0, len)));
      for (size_t char_pos = 0; char_pos < len; ++char_pos) {
        wchar_ascii[char_pos] |= 0x80;
        EXPECT_FALSE(IsStringASCII(wchar_ascii.substr(0, len)));
        wchar_ascii[char_pos] &= ~0x80;
        wchar_ascii[char_pos] |= 0x100;
        EXPECT_FALSE(IsStringASCII(wchar_ascii.substr(0, len)));
        wchar_ascii[char_pos] &= ~0x100;
        wchar_ascii[char_pos] |= 0x10000;
        EXPECT_FALSE(IsStringASCII(wchar_ascii.substr(0, len)));
        wchar_ascii[char_pos] &= ~0x10000;
      }
    }
  }
#endif  // WCHAR_T_IS_32_BIT
}

TEST(StringUtilTest, ConvertASCII) {
  static const char* const char_cases[] = {
    "Google Video",
    "Hello, world\n",
    "0123ABCDwxyz \a\b\t\r\n!+,.~"
  };

  static const wchar_t* const wchar_cases[] = {
    L"Google Video",
    L"Hello, world\n",
    L"0123ABCDwxyz \a\b\t\r\n!+,.~"
  };

  for (size_t i = 0; i < std::size(char_cases); ++i) {
    EXPECT_TRUE(IsStringASCII(char_cases[i]));
    std::u16string utf16 = ASCIIToUTF16(char_cases[i]);
    EXPECT_EQ(WideToUTF16(wchar_cases[i]), utf16);

    std::string ascii = UTF16ToASCII(WideToUTF16(wchar_cases[i]));
    EXPECT_EQ(char_cases[i], ascii);
  }

  EXPECT_FALSE(IsStringASCII("Google \x80Video"));

  // Convert empty strings.
  std::u16string empty16;
  std::string empty;
  EXPECT_EQ(empty, UTF16ToASCII(empty16));
  EXPECT_EQ(empty16, ASCIIToUTF16(empty));

  // Convert strings with an embedded NUL character.
  const char chars_with_nul[] = "test\0string";
  const int length_with_nul = std::size(chars_with_nul) - 1;
  std::string string_with_nul(chars_with_nul, length_with_nul);
  std::u16string string16_with_nul = ASCIIToUTF16(string_with_nul);
  EXPECT_EQ(static_cast<std::u16string::size_type>(length_with_nul),
            string16_with_nul.length());
  std::string narrow_with_nul = UTF16ToASCII(string16_with_nul);
  EXPECT_EQ(static_cast<std::string::size_type>(length_with_nul),
            narrow_with_nul.length());
  EXPECT_EQ(0, string_with_nul.compare(narrow_with_nul));
}

TEST(StringUtilTest, ToLowerASCII) {
  EXPECT_EQ('c', ToLowerASCII('C'));
  EXPECT_EQ('c', ToLowerASCII('c'));
  EXPECT_EQ('2', ToLowerASCII('2'));

  EXPECT_EQ(u'c', ToLowerASCII(u'C'));
  EXPECT_EQ(u'c', ToLowerASCII(u'c'));
  EXPECT_EQ(u'2', ToLowerASCII(u'2'));

  EXPECT_EQ("cc2", ToLowerASCII("Cc2"));
  EXPECT_EQ(u"cc2", ToLowerASCII(u"Cc2"));

  // Non-ASCII characters are unmodified. U+00C4 is LATIN CAPITAL LETTER A WITH
  // DIAERESIS.
  EXPECT_EQ('\xc4', ToLowerASCII('\xc4'));
  EXPECT_EQ(u'\x00c4', ToLowerASCII(u'\x00c4'));
}

TEST(StringUtilTest, ToUpperASCII) {
  EXPECT_EQ('C', ToUpperASCII('C'));
  EXPECT_EQ('C', ToUpperASCII('c'));
  EXPECT_EQ('2', ToUpperASCII('2'));

  EXPECT_EQ(u'C', ToUpperASCII(u'C'));
  EXPECT_EQ(u'C', ToUpperASCII(u'c'));
  EXPECT_EQ(u'2', ToUpperASCII(u'2'));

  EXPECT_EQ("CC2", ToUpperASCII("Cc2"));
  EXPECT_EQ(u"CC2", ToUpperASCII(u"Cc2"));

  // Non-ASCII characters are unmodified. U+00E4 is LATIN SMALL LETTER A WITH
  // DIAERESIS.
  EXPECT_EQ('\xe4', ToUpperASCII('\xe4'));
  EXPECT_EQ(u'\x00e4', ToUpperASCII(u'\x00e4'));
}

TEST(StringUtilTest, FormatBytesUnlocalized) {
  static const struct {
    int64_t bytes;
    const char* expected;
  } cases[] = {
      // Expected behavior: we show one post-decimal digit when we have
      // under two pre-decimal digits, except in cases where it makes no
      // sense (zero or bytes).
      // Since we switch units once we cross the 1000 mark, this keeps
      // the display of file sizes or bytes consistently around three
      // digits.
      {0, "0 B"},
      {512, "512 B"},
      {1024 * 1024, "1.0 MB"},
      {1024 * 1024 * 1024, "1.0 GB"},
      {10LL * 1024 * 1024 * 1024, "10.0 GB"},
      {99LL * 1024 * 1024 * 1024, "99.0 GB"},
      {105LL * 1024 * 1024 * 1024, "105 GB"},
      {105LL * 1024 * 1024 * 1024 + 500LL * 1024 * 1024, "105 GB"},
      {~(bits::LeftmostBit<int64_t>()), "8192 PB"},

      {99 * 1024 + 103, "99.1 kB"},
      {1024 * 1024 + 103, "1.0 MB"},
      {1024 * 1024 + 205 * 1024, "1.2 MB"},
      {1024 * 1024 * 1024 + (927 * 1024 * 1024), "1.9 GB"},
      {10LL * 1024 * 1024 * 1024, "10.0 GB"},
      {100LL * 1024 * 1024 * 1024, "100 GB"},
  };

  for (const auto& i : cases) {
    EXPECT_EQ(ASCIIToUTF16(i.expected), FormatBytesUnlocalized(i.bytes));
  }
}
TEST(StringUtilTest, ReplaceSubstringsAfterOffset) {
  static const struct {
    std::string_view str;
    size_t start_offset;
    std::string_view find_this;
    std::string_view replace_with;
    std::string_view expected;
  } cases[] = {
      {"aaa", 0, "", "b", "aaa"},
      {"aaa", 1, "", "b", "aaa"},
      {"aaa", 0, "a", "b", "bbb"},
      {"aaa", 0, "aa", "b", "ba"},
      {"aaa", 0, "aa", "bbb", "bbba"},
      {"aaaaa", 0, "aa", "b", "bba"},
      {"ababaaababa", 0, "aba", "", "baaba"},
      {"ababaaababa", 0, "aba", "_", "_baa_ba"},
      {"ababaaababa", 0, "aba", "__", "__baa__ba"},
      {"ababaaababa", 0, "aba", "___", "___baa___ba"},
      {"ababaaababa", 0, "aba", "____", "____baa____ba"},
      {"ababaaababa", 0, "aba", "_____", "_____baa_____ba"},
      {"abb", 0, "ab", "a", "ab"},
      {"Removing some substrings inging", 0, "ing", "", "Remov some substrs "},
      {"Not found", 0, "x", "0", "Not found"},
      {"Not found again", 5, "x", "0", "Not found again"},
      {" Making it much longer ", 0, " ", "Four score and seven years ago",
       "Four score and seven years agoMakingFour score and seven years agoit"
       "Four score and seven years agomuchFour score and seven years agolonger"
       "Four score and seven years ago"},
      {" Making it much much much much shorter ", 0,
       "Making it much much much much shorter", "", "  "},
      {"so much much much much much very much much much shorter", 0, "much ",
       "", "so very shorter"},
      {"Invalid offset", 9999, "t", "foobar", "Invalid offset"},
      {"Replace me only me once", 9, "me ", "", "Replace me only once"},
      {"abababab", 2, "ab", "c", "abccc"},
      {"abababab", 1, "ab", "c", "abccc"},
      {"abababab", 1, "aba", "c", "abcbab"},
  };

  // std::u16string variant
  for (const auto& scenario : cases) {
    std::u16string str = ASCIIToUTF16(scenario.str);
    ReplaceSubstringsAfterOffset(&str, scenario.start_offset,
                                 ASCIIToUTF16(scenario.find_this),
                                 ASCIIToUTF16(scenario.replace_with));
    EXPECT_EQ(ASCIIToUTF16(scenario.expected), str);
  }

  // std::string with insufficient capacity: expansion must realloc the buffer.
  for (const auto& scenario : cases) {
    std::string str(scenario.str);
    str.shrink_to_fit();  // This is nonbinding, but it's the best we've got.
    ReplaceSubstringsAfterOffset(&str, scenario.start_offset,
                                 scenario.find_this, scenario.replace_with);
    EXPECT_EQ(scenario.expected, str);
  }

  // std::string with ample capacity: should be possible to grow in-place.
  for (const auto& scenario : cases) {
    std::string str(scenario.str);
    str.reserve(std::max(scenario.str.length(), scenario.expected.length()) *
                2);

    ReplaceSubstringsAfterOffset(&str, scenario.start_offset,
                                 scenario.find_this, scenario.replace_with);
    EXPECT_EQ(scenario.expected, str);
  }
}

TEST(StringUtilTest, ReplaceFirstSubstringAfterOffset) {
  static const struct {
    const char* str;
    std::u16string::size_type start_offset;
    const char* find_this;
    const char* replace_with;
    const char* expected;
  } cases[] = {
    {"aaa", 0, "a", "b", "baa"},
    {"abb", 0, "ab", "a", "ab"},
    {"Removing some substrings inging", 0, "ing", "",
      "Remov some substrings inging"},
    {"Not found", 0, "x", "0", "Not found"},
    {"Not found again", 5, "x", "0", "Not found again"},
    {" Making it much longer ", 0, " ", "Four score and seven years ago",
     "Four score and seven years agoMaking it much longer "},
    {"Invalid offset", 9999, "t", "foobar", "Invalid offset"},
    {"Replace me only me once", 4, "me ", "", "Replace only me once"},
    {"abababab", 2, "ab", "c", "abcabab"},
  };

  for (const auto& i : cases) {
    std::u16string str = ASCIIToUTF16(i.str);
    ReplaceFirstSubstringAfterOffset(&str, i.start_offset,
                                     ASCIIToUTF16(i.find_this),
                                     ASCIIToUTF16(i.replace_with));
    EXPECT_EQ(ASCIIToUTF16(i.expected), str);
  }
}

TEST(StringUtilTest, HexDigitToInt) {
  EXPECT_EQ(0, HexDigitToInt('0'));
  EXPECT_EQ(1, HexDigitToInt('1'));
  EXPECT_EQ(2, HexDigitToInt('2'));
  EXPECT_EQ(3, HexDigitToInt('3'));
  EXPECT_EQ(4, HexDigitToInt('4'));
  EXPECT_EQ(5, HexDigitToInt('5'));
  EXPECT_EQ(6, HexDigitToInt('6'));
  EXPECT_EQ(7, HexDigitToInt('7'));
  EXPECT_EQ(8, HexDigitToInt('8'));
  EXPECT_EQ(9, HexDigitToInt('9'));
  EXPECT_EQ(10, HexDigitToInt('A'));
  EXPECT_EQ(11, HexDigitToInt('B'));
  EXPECT_EQ(12, HexDigitToInt('C'));
  EXPECT_EQ(13, HexDigitToInt('D'));
  EXPECT_EQ(14, HexDigitToInt('E'));
  EXPECT_EQ(15, HexDigitToInt('F'));

  // Verify the lower case as well.
  EXPECT_EQ(10, HexDigitToInt('a'));
  EXPECT_EQ(11, HexDigitToInt('b'));
  EXPECT_EQ(12, HexDigitToInt('c'));
  EXPECT_EQ(13, HexDigitToInt('d'));
  EXPECT_EQ(14, HexDigitToInt('e'));
  EXPECT_EQ(15, HexDigitToInt('f'));
}

TEST(StringUtilTest, JoinString) {
  std::string separator(", ");
  std::vector<std::string> parts;
  EXPECT_EQ(std::string(), JoinString(parts, separator));

  parts.push_back(std::string());
  EXPECT_EQ(std::string(), JoinString(parts, separator));
  parts.clear();

  parts.push_back("a");
  EXPECT_EQ("a", JoinString(parts, separator));

  parts.push_back("b");
  parts.push_back("c");
  EXPECT_EQ("a, b, c", JoinString(parts, separator));

  parts.push_back(std::string());
  EXPECT_EQ("a, b, c, ", JoinString(parts, separator));
  parts.push_back(" ");
  EXPECT_EQ("a|b|c|| ", JoinString(parts, "|"));
}

TEST(StringUtilTest, JoinString16) {
  std::u16string separator = u", ";
  std::vector<std::u16string> parts;
  EXPECT_EQ(std::u16string(), JoinString(parts, separator));

  parts.push_back(std::u16string());
  EXPECT_EQ(std::u16string(), JoinString(parts, separator));
  parts.clear();

  parts.push_back(u"a");
  EXPECT_EQ(u"a", JoinString(parts, separator));

  parts.push_back(u"b");
  parts.push_back(u"c");
  EXPECT_EQ(u"a, b, c", JoinString(parts, separator));

  parts.push_back(u"");
  EXPECT_EQ(u"a, b, c, ", JoinString(parts, separator));
  parts.push_back(u" ");
  EXPECT_EQ(u"a|b|c|| ", JoinString(parts, u"|"));
}

TEST(StringUtilTest, JoinStringPiece) {
  std::string separator(", ");
  std::vector<std::string_view> parts;
  EXPECT_EQ(std::string(), JoinString(parts, separator));

  // Test empty first part (https://crbug.com/698073).
  parts.push_back(std::string_view());
  EXPECT_EQ(std::string(), JoinString(parts, separator));
  parts.clear();

  parts.push_back("a");
  EXPECT_EQ("a", JoinString(parts, separator));

  parts.push_back("b");
  parts.push_back("c");
  EXPECT_EQ("a, b, c", JoinString(parts, separator));

  parts.push_back(std::string_view());
  EXPECT_EQ("a, b, c, ", JoinString(parts, separator));
  parts.push_back(" ");
  EXPECT_EQ("a|b|c|| ", JoinString(parts, "|"));
}

TEST(StringUtilTest, JoinStringPiece16) {
  std::u16string separator = u", ";
  std::vector<std::u16string_view> parts;
  EXPECT_EQ(std::u16string(), JoinString(parts, separator));

  // Test empty first part (https://crbug.com/698073).
  parts.push_back(std::u16string_view());
  EXPECT_EQ(std::u16string(), JoinString(parts, separator));
  parts.clear();

  const std::u16string kA = u"a";
  parts.push_back(kA);
  EXPECT_EQ(u"a", JoinString(parts, separator));

  const std::u16string kB = u"b";
  parts.push_back(kB);
  const std::u16string kC = u"c";
  parts.push_back(kC);
  EXPECT_EQ(u"a, b, c", JoinString(parts, separator));

  parts.push_back(std::u16string_view());
  EXPECT_EQ(u"a, b, c, ", JoinString(parts, separator));
  const std::u16string kSpace = u" ";
  parts.push_back(kSpace);
  EXPECT_EQ(u"a|b|c|| ", JoinString(parts, u"|"));
}

TEST(StringUtilTest, JoinStringInitializerList) {
  std::string separator(", ");
  EXPECT_EQ(std::string(), JoinString({}, separator));

  // Test empty first part (https://crbug.com/698073).
  EXPECT_EQ(std::string(), JoinString({std::string_view()}, separator));

  // With const char*s.
  EXPECT_EQ("a", JoinString({"a"}, separator));
  EXPECT_EQ("a, b, c", JoinString({"a", "b", "c"}, separator));
  EXPECT_EQ("a, b, c, ",
            JoinString({"a", "b", "c", std::string_view()}, separator));
  EXPECT_EQ("a|b|c|| ",
            JoinString({"a", "b", "c", std::string_view(), " "}, "|"));

  // With std::strings.
  const std::string kA = "a";
  const std::string kB = "b";
  EXPECT_EQ("a, b", JoinString({kA, kB}, separator));

  // With StringPieces.
  const std::string_view kPieceA = kA;
  const std::string_view kPieceB = kB;
  EXPECT_EQ("a, b", JoinString({kPieceA, kPieceB}, separator));
}

TEST(StringUtilTest, JoinStringInitializerList16) {
  std::u16string separator = u", ";
  EXPECT_EQ(std::u16string(), JoinString({}, separator));

  // Test empty first part (https://crbug.com/698073).
  EXPECT_EQ(std::u16string(), JoinString({std::u16string_view()}, separator));

  // With string16s.
  const std::u16string kA = u"a";
  EXPECT_EQ(u"a", JoinString({kA}, separator));

  const std::u16string kB = u"b";
  const std::u16string kC = u"c";
  EXPECT_EQ(u"a, b, c", JoinString({kA, kB, kC}, separator));

  EXPECT_EQ(u"a, b, c, ",
            JoinString({kA, kB, kC, std::u16string_view()}, separator));
  const std::u16string kSpace = u" ";
  EXPECT_EQ(u"a|b|c|| ",
            JoinString({kA, kB, kC, std::u16string_view(), kSpace}, u"|"));

  // With StringPiece16s.
  const std::u16string_view kPieceA = kA;
  const std::u16string_view kPieceB = kB;
  EXPECT_EQ(u"a, b", JoinString({kPieceA, kPieceB}, separator));
}

TEST(StringUtilTest, StartsWith) {
  EXPECT_TRUE(StartsWith("javascript:url", "javascript",
                         base::CompareCase::SENSITIVE));
  EXPECT_FALSE(StartsWith("JavaScript:url", "javascript",
                          base::CompareCase::SENSITIVE));
  EXPECT_TRUE(StartsWith("javascript:url", "javascript",
                         base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(StartsWith("JavaScript:url", "javascript",
                         base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(StartsWith("java", "javascript", base::CompareCase::SENSITIVE));
  EXPECT_FALSE(StartsWith("java", "javascript",
                          base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(StartsWith(std::string(), "javascript",
                          base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(StartsWith(std::string(), "javascript",
                          base::CompareCase::SENSITIVE));
  EXPECT_TRUE(StartsWith("java", std::string(),
                         base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(StartsWith("java", std::string(), base::CompareCase::SENSITIVE));

  EXPECT_TRUE(StartsWith(u"javascript:url", u"javascript",
                         base::CompareCase::SENSITIVE));
  EXPECT_FALSE(StartsWith(u"JavaScript:url", u"javascript",
                          base::CompareCase::SENSITIVE));
  EXPECT_TRUE(StartsWith(u"javascript:url", u"javascript",
                         base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(StartsWith(u"JavaScript:url", u"javascript",
                         base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(
      StartsWith(u"java", u"javascript", base::CompareCase::SENSITIVE));
  EXPECT_FALSE(
      StartsWith(u"java", u"javascript", base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(StartsWith(std::u16string(), u"javascript",
                          base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(StartsWith(std::u16string(), u"javascript",
                          base::CompareCase::SENSITIVE));
  EXPECT_TRUE(StartsWith(u"java", std::u16string(),
                         base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(
      StartsWith(u"java", std::u16string(), base::CompareCase::SENSITIVE));
}

TEST(StringUtilTest, EndsWith) {
  EXPECT_TRUE(
      EndsWith(u"Foo.plugin", u".plugin", base::CompareCase::SENSITIVE));
  EXPECT_FALSE(
      EndsWith(u"Foo.Plugin", u".plugin", base::CompareCase::SENSITIVE));
  EXPECT_TRUE(EndsWith(u"Foo.plugin", u".plugin",
                       base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(EndsWith(u"Foo.Plugin", u".plugin",
                       base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(EndsWith(u".plug", u".plugin", base::CompareCase::SENSITIVE));
  EXPECT_FALSE(
      EndsWith(u".plug", u".plugin", base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(
      EndsWith(u"Foo.plugin Bar", u".plugin", base::CompareCase::SENSITIVE));
  EXPECT_FALSE(EndsWith(u"Foo.plugin Bar", u".plugin",
                        base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(EndsWith(std::u16string(), u".plugin",
                        base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(
      EndsWith(std::u16string(), u".plugin", base::CompareCase::SENSITIVE));
  EXPECT_TRUE(EndsWith(u"Foo.plugin", std::u16string(),
                       base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(
      EndsWith(u"Foo.plugin", std::u16string(), base::CompareCase::SENSITIVE));
  EXPECT_TRUE(
      EndsWith(u".plugin", u".plugin", base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(EndsWith(u".plugin", u".plugin", base::CompareCase::SENSITIVE));
  EXPECT_TRUE(EndsWith(std::u16string(), std::u16string(),
                       base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(EndsWith(std::u16string(), std::u16string(),
                       base::CompareCase::SENSITIVE));
}

TEST(StringUtilTest, GetStringFWithOffsets) {
  std::vector<std::u16string> subst;
  subst.push_back(u"1");
  subst.push_back(u"2");
  std::vector<size_t> offsets;

  ReplaceStringPlaceholders(u"Hello, $1. Your number is $2.", subst, &offsets);
  EXPECT_EQ(2U, offsets.size());
  EXPECT_EQ(7U, offsets[0]);
  EXPECT_EQ(25U, offsets[1]);
  offsets.clear();

  ReplaceStringPlaceholders(u"Hello, $2. Your number is $1.", subst, &offsets);
  EXPECT_EQ(2U, offsets.size());
  EXPECT_EQ(25U, offsets[0]);
  EXPECT_EQ(7U, offsets[1]);
  offsets.clear();
}

TEST(StringUtilTest, ReplaceStringPlaceholdersTooFew) {
  // Test whether replacestringplaceholders works as expected when there
  // are fewer inputs than outputs.
  std::vector<std::u16string> subst;
  subst.push_back(u"9a");
  subst.push_back(u"8b");
  subst.push_back(u"7c");

  std::u16string formatted = ReplaceStringPlaceholders(
      u"$1a,$2b,$3c,$4d,$5e,$6f,$1g,$2h,$3i", subst, nullptr);

  EXPECT_EQ(u"9aa,8bb,7cc,d,e,f,9ag,8bh,7ci", formatted);
}

TEST(StringUtilTest, ReplaceStringPlaceholders) {
  std::vector<std::u16string> subst;
  subst.push_back(u"9a");
  subst.push_back(u"8b");
  subst.push_back(u"7c");
  subst.push_back(u"6d");
  subst.push_back(u"5e");
  subst.push_back(u"4f");
  subst.push_back(u"3g");
  subst.push_back(u"2h");
  subst.push_back(u"1i");

  std::u16string formatted = ReplaceStringPlaceholders(
      u"$1a,$2b,$3c,$4d,$5e,$6f,$7g,$8h,$9i", subst, nullptr);

  EXPECT_EQ(u"9aa,8bb,7cc,6dd,5ee,4ff,3gg,2hh,1ii", formatted);
}

TEST(StringUtilTest, ReplaceStringPlaceholdersNetExpansionWithContraction) {
  // In this test, some of the substitutions are shorter than the placeholders,
  // but overall the string gets longer.
  std::vector<std::u16string> subst;
  subst.push_back(u"9a____");
  subst.push_back(u"B");
  subst.push_back(u"7c___");
  subst.push_back(u"d");
  subst.push_back(u"5e____");
  subst.push_back(u"F");
  subst.push_back(u"3g___");
  subst.push_back(u"h");
  subst.push_back(u"1i_____");

  std::u16string original = u"$1a,$2b,$3c,$4d,$5e,$6f,$7g,$8h,$9i";
  std::u16string expected =
      u"9a____a,Bb,7c___c,dd,5e____e,Ff,3g___g,hh,1i_____i";

  EXPECT_EQ(expected, ReplaceStringPlaceholders(original, subst, nullptr));

  std::vector<size_t> offsets;
  EXPECT_EQ(expected, ReplaceStringPlaceholders(original, subst, &offsets));
  std::vector<size_t> expected_offsets = {0, 8, 11, 18, 21, 29, 32, 39, 42};
  EXPECT_EQ(offsets.size(), subst.size());
  EXPECT_EQ(expected_offsets, offsets);
  for (size_t i = 0; i < offsets.size(); i++) {
    EXPECT_EQ(expected.substr(expected_offsets[i], subst[i].length()),
              subst[i]);
  }
}

TEST(StringUtilTest, ReplaceStringPlaceholdersNetContractionWithExpansion) {
  // In this test, some of the substitutions are longer than the placeholders,
  // but overall the string gets smaller. Additionally, the placeholders appear
  // in a permuted order.
  std::vector<std::u16string> subst;
  subst.push_back(u"z");
  subst.push_back(u"y");
  subst.push_back(u"XYZW");
  subst.push_back(u"x");
  subst.push_back(u"w");

  std::u16string formatted =
      ReplaceStringPlaceholders(u"$3_$4$2$1$5", subst, nullptr);

  EXPECT_EQ(u"XYZW_xyzw", formatted);
}

TEST(StringUtilTest, ReplaceStringPlaceholdersOneDigit) {
  std::vector<std::u16string> subst;
  subst.push_back(u"1a");
  std::u16string formatted =
      ReplaceStringPlaceholders(u" $16 ", subst, nullptr);
  EXPECT_EQ(u" 1a6 ", formatted);
}

TEST(StringUtilTest, ReplaceStringPlaceholdersInvalidPlaceholder) {
  std::vector<std::u16string> subst;
  subst.push_back(u"1a");
  std::u16string formatted =
      ReplaceStringPlaceholders(u"+$-+$A+$1+", subst, nullptr);
  EXPECT_EQ(u"+++1a+", formatted);
}

TEST(StringUtilTest, StdStringReplaceStringPlaceholders) {
  std::vector<std::string> subst;
  subst.push_back("9a");
  subst.push_back("8b");
  subst.push_back("7c");
  subst.push_back("6d");
  subst.push_back("5e");
  subst.push_back("4f");
  subst.push_back("3g");
  subst.push_back("2h");
  subst.push_back("1i");

  std::string formatted =
      ReplaceStringPlaceholders(
          "$1a,$2b,$3c,$4d,$5e,$6f,$7g,$8h,$9i", subst, nullptr);

  EXPECT_EQ("9aa,8bb,7cc,6dd,5ee,4ff,3gg,2hh,1ii", formatted);
}

TEST(StringUtilTest, StdStringReplaceStringPlaceholdersMultipleMatches) {
  std::vector<std::string> subst;
  subst.push_back("4");   // Referenced twice.
  subst.push_back("?");   // Unreferenced.
  subst.push_back("!");   // Unreferenced.
  subst.push_back("16");  // Referenced once.

  std::string original = "$1 * $1 == $4";
  std::string expected = "4 * 4 == 16";
  EXPECT_EQ(expected, ReplaceStringPlaceholders(original, subst, nullptr));
  std::vector<size_t> offsets;
  EXPECT_EQ(expected, ReplaceStringPlaceholders(original, subst, &offsets));
  std::vector<size_t> expected_offsets = {0, 4, 9};
  EXPECT_EQ(expected_offsets, offsets);
}

TEST(StringUtilTest, ReplaceStringPlaceholdersConsecutiveDollarSigns) {
  std::vector<std::string> subst;
  subst.push_back("a");
  subst.push_back("b");
  subst.push_back("c");
  EXPECT_EQ(ReplaceStringPlaceholders("$$1 $$$2 $$$$3", subst, nullptr),
            "$1 $$2 $$$3");
}

TEST(StringUtilTest, LcpyTest) {
  // Test the normal case where we fit in our buffer.
  {
    char dst[10];
    char16_t u16dst[10];
    wchar_t wdst[10];
    EXPECT_EQ(7U, strlcpy(dst, "abcdefg", std::size(dst)));
    EXPECT_EQ(0, memcmp(dst, "abcdefg", sizeof(dst[0]) * 8));
    EXPECT_EQ(7U, u16cstrlcpy(u16dst, u"abcdefg", std::size(u16dst)));
    EXPECT_EQ(0, memcmp(u16dst, u"abcdefg", sizeof(u16dst[0]) * 8));
    EXPECT_EQ(7U, wcslcpy(wdst, L"abcdefg", std::size(wdst)));
    EXPECT_EQ(0, memcmp(wdst, L"abcdefg", sizeof(wdst[0]) * 8));

    EXPECT_EQ(7U, strlcpy(dst, "abcdefg"));
    EXPECT_EQ(base::span(dst).first(8u),
              base::span_with_nul_from_cstring("abcdefg"));
    EXPECT_EQ(7U, u16cstrlcpy(u16dst, u"abcdefg"));
    EXPECT_EQ(base::span(u16dst).first(8u),
              base::span_with_nul_from_cstring(u"abcdefg"));
    EXPECT_EQ(7U, wcslcpy(wdst, L"abcdefg"));
    EXPECT_EQ(base::span(wdst).first(8u),
              base::span_with_nul_from_cstring(L"abcdefg"));
  }

  // Test dst_size == 0, nothing should be written to |dst| and we should
  // have the equivalent of strlen(src).
  {
    char dst[2] = {1, 2};
    char16_t u16dst[2] = {1, 2};
    wchar_t wdst[2] = {1, 2};
    EXPECT_EQ(7U, strlcpy(dst, "abcdefg", 0));
    EXPECT_EQ(1, dst[0]);
    EXPECT_EQ(2, dst[1]);
    EXPECT_EQ(7U, u16cstrlcpy(u16dst, u"abcdefg", 0));
    EXPECT_EQ(char16_t{1}, u16dst[0]);
    EXPECT_EQ(char16_t{2}, u16dst[1]);
    EXPECT_EQ(7U, wcslcpy(wdst, L"abcdefg", 0));
    EXPECT_EQ(static_cast<wchar_t>(1), wdst[0]);
    EXPECT_EQ(static_cast<wchar_t>(2), wdst[1]);

    EXPECT_EQ(7U, strlcpy(base::span(dst).first(0u), "abcdefg"));
    EXPECT_EQ(1, dst[0]);
    EXPECT_EQ(2, dst[1]);
    EXPECT_EQ(7U, u16cstrlcpy(base::span(u16dst).first(0u), u"abcdefg"));
    EXPECT_EQ(char16_t{1}, u16dst[0]);
    EXPECT_EQ(char16_t{2}, u16dst[1]);
    EXPECT_EQ(7U, wcslcpy(base::span(wdst).first(0u), L"abcdefg"));
    EXPECT_EQ(static_cast<wchar_t>(1), wdst[0]);
    EXPECT_EQ(static_cast<wchar_t>(2), wdst[1]);
  }

  // Test the case were we _just_ competely fit including the null.
  {
    char dst[8];
    char16_t u16dst[8];
    wchar_t wdst[8];
    EXPECT_EQ(7U, strlcpy(dst, "abcdefg", std::size(dst)));
    EXPECT_EQ(0, memcmp(dst, "abcdefg", 8));
    EXPECT_EQ(7U, u16cstrlcpy(u16dst, u"abcdefg", std::size(u16dst)));
    EXPECT_EQ(0, memcmp(u16dst, u"abcdefg", sizeof(u16dst)));
    EXPECT_EQ(7U, wcslcpy(wdst, L"abcdefg", std::size(wdst)));
    EXPECT_EQ(0, memcmp(wdst, L"abcdefg", sizeof(wdst)));

    EXPECT_EQ(7U, strlcpy(dst, "abcdefg"));
    EXPECT_EQ(base::span(dst), base::span_with_nul_from_cstring("abcdefg"));
    EXPECT_EQ(7U, u16cstrlcpy(u16dst, u"abcdefg"));
    EXPECT_EQ(base::span(u16dst), base::span_with_nul_from_cstring(u"abcdefg"));
    EXPECT_EQ(7U, wcslcpy(wdst, L"abcdefg"));
    EXPECT_EQ(base::span(wdst), base::span_with_nul_from_cstring(L"abcdefg"));
  }

  // Test the case were we we are one smaller, so we can't fit the null.
  {
    char dst[7];
    char16_t u16dst[7];
    wchar_t wdst[7];
    EXPECT_EQ(7U, strlcpy(dst, "abcdefg", std::size(dst)));
    EXPECT_EQ(0, memcmp(dst, "abcdef", sizeof(dst[0]) * 7));
    EXPECT_EQ(7U, u16cstrlcpy(u16dst, u"abcdefg", std::size(u16dst)));
    EXPECT_EQ(0, memcmp(u16dst, u"abcdef", sizeof(u16dst[0]) * 7));
    EXPECT_EQ(7U, wcslcpy(wdst, L"abcdefg", std::size(wdst)));
    EXPECT_EQ(0, memcmp(wdst, L"abcdef", sizeof(wdst[0]) * 7));

    EXPECT_EQ(7U, strlcpy(dst, "abcdefg"));
    EXPECT_EQ(base::span(dst), base::span_with_nul_from_cstring("abcdef"));
    EXPECT_EQ(7U, u16cstrlcpy(u16dst, u"abcdefg"));
    EXPECT_EQ(base::span(u16dst), base::span_with_nul_from_cstring(u"abcdef"));
    EXPECT_EQ(7U, wcslcpy(wdst, L"abcdefg"));
    EXPECT_EQ(base::span(wdst), base::span_with_nul_from_cstring(L"abcdef"));
  }

  // Test the case were we are just too small.
  {
    char dst[3];
    char16_t u16dst[3];
    wchar_t wdst[3];
    EXPECT_EQ(7U, strlcpy(dst, "abcdefg", std::size(dst)));
    EXPECT_EQ(0, memcmp(dst, "ab", sizeof(dst)));
    EXPECT_EQ(7U, u16cstrlcpy(u16dst, u"abcdefg", std::size(u16dst)));
    EXPECT_EQ(0, memcmp(u16dst, u"ab", sizeof(u16dst)));
    EXPECT_EQ(7U, wcslcpy(wdst, L"abcdefg", std::size(wdst)));
    EXPECT_EQ(0, memcmp(wdst, L"ab", sizeof(wdst)));

    EXPECT_EQ(7U, strlcpy(dst, "abcdefg"));
    EXPECT_EQ(base::span(dst), base::span_with_nul_from_cstring("ab"));
    EXPECT_EQ(7U, u16cstrlcpy(u16dst, u"abcdefg"));
    EXPECT_EQ(base::span(u16dst), base::span_with_nul_from_cstring(u"ab"));
    EXPECT_EQ(7U, wcslcpy(wdst, L"abcdefg"));
    EXPECT_EQ(base::span(wdst), base::span_with_nul_from_cstring(L"ab"));
  }
}

TEST(StringUtilTest, WprintfFormatPortabilityTest) {
  static const struct {
    const wchar_t* input;
    bool portable;
  } cases[] = {
    { L"%ls", true },
    { L"%s", false },
    { L"%S", false },
    { L"%lS", false },
    { L"Hello, %s", false },
    { L"%lc", true },
    { L"%c", false },
    { L"%C", false },
    { L"%lC", false },
    { L"%ls %s", false },
    { L"%s %ls", false },
    { L"%s %ls %s", false },
    { L"%f", true },
    { L"%f %F", false },
    { L"%d %D", false },
    { L"%o %O", false },
    { L"%u %U", false },
    { L"%f %d %o %u", true },
    { L"%-8d (%02.1f%)", true },
    { L"% 10s", false },
    { L"% 10ls", true }
  };
  for (const auto& i : cases)
    EXPECT_EQ(i.portable, IsWprintfFormatPortable(i.input));
}

TEST(StringUtilTest, MakeBasicStringPieceTest) {
  constexpr char kFoo[] = "Foo";
  static_assert(MakeStringPiece(kFoo, kFoo + 3) == kFoo, "");
  static_assert(MakeStringPiece(kFoo, kFoo + 3).data() == kFoo, "");
  static_assert(MakeStringPiece(kFoo, kFoo + 3).size() == 3, "");
  static_assert(MakeStringPiece(kFoo + 3, kFoo + 3).empty(), "");
  static_assert(MakeStringPiece(kFoo + 4, kFoo + 4).empty(), "");

  std::string foo = kFoo;
  EXPECT_EQ(MakeStringPiece(foo.begin(), foo.end()), foo);
  EXPECT_EQ(MakeStringPiece(foo.begin(), foo.end()).data(), foo.data());
  EXPECT_EQ(MakeStringPiece(foo.begin(), foo.end()).size(), foo.size());
  EXPECT_TRUE(MakeStringPiece(foo.end(), foo.end()).empty());

  constexpr char16_t kBar[] = u"Bar";
  static_assert(MakeStringPiece16(kBar, kBar + 3) == kBar, "");
  static_assert(MakeStringPiece16(kBar, kBar + 3).data() == kBar, "");
  static_assert(MakeStringPiece16(kBar, kBar + 3).size() == 3, "");
  static_assert(MakeStringPiece16(kBar + 3, kBar + 3).empty(), "");
  static_assert(MakeStringPiece16(kBar + 4, kBar + 4).empty(), "");

  std::u16string bar = kBar;
  EXPECT_EQ(MakeStringPiece16(bar.begin(), bar.end()), bar);
  EXPECT_EQ(MakeStringPiece16(bar.begin(), bar.end()).data(), bar.data());
  EXPECT_EQ(MakeStringPiece16(bar.begin(), bar.end()).size(), bar.size());
  EXPECT_TRUE(MakeStringPiece16(bar.end(), bar.end()).empty());

  constexpr wchar_t kBaz[] = L"Baz";
  static_assert(MakeWStringView(kBaz, kBaz + 3) == kBaz, "");
  static_assert(MakeWStringView(kBaz, kBaz + 3).data() == kBaz, "");
  static_assert(MakeWStringView(kBaz, kBaz + 3).size() == 3, "");
  static_assert(MakeWStringView(kBaz + 3, kBaz + 3).empty(), "");
  static_assert(MakeWStringView(kBaz + 4, kBaz + 4).empty(), "");

  std::wstring baz = kBaz;
  EXPECT_EQ(MakeWStringView(baz.begin(), baz.end()), baz);
  EXPECT_EQ(MakeWStringView(baz.begin(), baz.end()).data(), baz.data());
  EXPECT_EQ(MakeWStringView(baz.begin(), baz.end()).size(), baz.size());
  EXPECT_TRUE(MakeWStringView(baz.end(), baz.end()).empty());
}

TEST(StringUtilTest, RemoveChars) {
  const char kRemoveChars[] = "-/+*";
  std::string input = "A-+bc/d!*";
  EXPECT_TRUE(RemoveChars(input, kRemoveChars, &input));
  EXPECT_EQ("Abcd!", input);

  // No characters match kRemoveChars.
  EXPECT_FALSE(RemoveChars(input, kRemoveChars, &input));
  EXPECT_EQ("Abcd!", input);

  // Empty string.
  input.clear();
  EXPECT_FALSE(RemoveChars(input, kRemoveChars, &input));
  EXPECT_EQ(std::string(), input);
}

TEST(StringUtilTest, ReplaceChars) {
  struct TestData {
    const char* input;
    const char* replace_chars;
    const char* replace_with;
    const char* output;
    bool result;
  } cases[] = {
      {"", "", "", "", false},
      {"t", "t", "t", "t", true},
      {"a", "b", "c", "a", false},
      {"b", "b", "c", "c", true},
      {"bob", "b", "p", "pop", true},
      {"bob", "o", "i", "bib", true},
      {"test", "", "", "test", false},
      {"test", "", "!", "test", false},
      {"test", "z", "!", "test", false},
      {"test", "e", "!", "t!st", true},
      {"test", "e", "!?", "t!?st", true},
      {"test", "ez", "!", "t!st", true},
      {"test", "zed", "!?", "t!?st", true},
      {"test", "t", "!?", "!?es!?", true},
      {"test", "et", "!>", "!>!>s!>", true},
      {"test", "zest", "!", "!!!!", true},
      {"test", "szt", "!", "!e!!", true},
      {"test", "t", "test", "testestest", true},
      {"tetst", "t", "test", "testeteststest", true},
      {"ttttttt", "t", "-", "-------", true},
      {"aAaAaAAaAAa", "A", "", "aaaaa", true},
      {"xxxxxxxxxx", "x", "", "", true},
      {"xxxxxxxxxx", "x", "x", "xxxxxxxxxx", true},
      {"xxxxxxxxxx", "x", "y-", "y-y-y-y-y-y-y-y-y-y-", true},
      {"xxxxxxxxxx", "x", "xy", "xyxyxyxyxyxyxyxyxyxy", true},
      {"xxxxxxxxxx", "x", "zyx", "zyxzyxzyxzyxzyxzyxzyxzyxzyxzyx", true},
      {"xaxxaxxxaxxxax", "x", "xy", "xyaxyxyaxyxyxyaxyxyxyaxy", true},
      {"-xaxxaxxxaxxxax-", "x", "xy", "-xyaxyxyaxyxyxyaxyxyxyaxy-", true},
  };

  for (const TestData& scenario : cases) {
    // Test with separate output and input vars.
    std::string output;
    bool result = ReplaceChars(scenario.input, scenario.replace_chars,
                               scenario.replace_with, &output);
    EXPECT_EQ(scenario.result, result) << scenario.input;
    EXPECT_EQ(scenario.output, output);
  }

  for (const TestData& scenario : cases) {
    // Test with an input/output var of limited capacity.
    std::string input_output = scenario.input;
    input_output.shrink_to_fit();
    bool result = ReplaceChars(input_output, scenario.replace_chars,
                               scenario.replace_with, &input_output);
    EXPECT_EQ(scenario.result, result) << scenario.input;
    EXPECT_EQ(scenario.output, input_output);
  }

  for (const TestData& scenario : cases) {
    // Test with an input/output var of ample capacity; should
    // not realloc.
    std::string input_output = scenario.input;
    input_output.reserve(strlen(scenario.output) * 2);
    const void* original_buffer = input_output.data();
    bool result = ReplaceChars(input_output, scenario.replace_chars,
                               scenario.replace_with, &input_output);
    EXPECT_EQ(scenario.result, result) << scenario.input;
    EXPECT_EQ(scenario.output, input_output);
    EXPECT_EQ(original_buffer, input_output.data());
  }
}

TEST(StringUtilTest, ContainsOnlyChars) {
  // Providing an empty list of characters should return false but for the empty
  // string.
  EXPECT_TRUE(ContainsOnlyChars(std::string(), std::string()));
  EXPECT_FALSE(ContainsOnlyChars("Hello", std::string()));

  EXPECT_TRUE(ContainsOnlyChars(std::string(), "1234"));
  EXPECT_TRUE(ContainsOnlyChars("1", "1234"));
  EXPECT_TRUE(ContainsOnlyChars("1", "4321"));
  EXPECT_TRUE(ContainsOnlyChars("123", "4321"));
  EXPECT_FALSE(ContainsOnlyChars("123a", "4321"));

  EXPECT_TRUE(ContainsOnlyChars(std::string(), kWhitespaceASCII));
  EXPECT_TRUE(ContainsOnlyChars(" ", kWhitespaceASCII));
  EXPECT_TRUE(ContainsOnlyChars("\t", kWhitespaceASCII));
  EXPECT_TRUE(ContainsOnlyChars("\t \r \n  ", kWhitespaceASCII));
  EXPECT_FALSE(ContainsOnlyChars("a", kWhitespaceASCII));
  EXPECT_FALSE(ContainsOnlyChars("\thello\r \n  ", kWhitespaceASCII));

  EXPECT_TRUE(ContainsOnlyChars(std::u16string(), kWhitespaceUTF16));
  EXPECT_TRUE(ContainsOnlyChars(u" ", kWhitespaceUTF16));
  EXPECT_TRUE(ContainsOnlyChars(u"\t", kWhitespaceUTF16));
  EXPECT_TRUE(ContainsOnlyChars(u"\t \r \n  ", kWhitespaceUTF16));
  EXPECT_FALSE(ContainsOnlyChars(u"a", kWhitespaceUTF16));
  EXPECT_FALSE(ContainsOnlyChars(u"\thello\r \n  ", kWhitespaceUTF16));
}

TEST(StringUtilTest, CompareCaseInsensitiveASCII) {
  EXPECT_EQ(0, CompareCaseInsensitiveASCII("", ""));
  EXPECT_EQ(0, CompareCaseInsensitiveASCII("Asdf", "aSDf"));

  // Differing lengths.
  EXPECT_EQ(-1, CompareCaseInsensitiveASCII("Asdf", "aSDfA"));
  EXPECT_EQ(1, CompareCaseInsensitiveASCII("AsdfA", "aSDf"));

  // Differing values.
  EXPECT_EQ(-1, CompareCaseInsensitiveASCII("AsdfA", "aSDfb"));
  EXPECT_EQ(1, CompareCaseInsensitiveASCII("Asdfb", "aSDfA"));

  // Non-ASCII bytes are permitted, but they will be compared case-sensitively.
  EXPECT_EQ(0, CompareCaseInsensitiveASCII("aaa \xc3\xa4", "AAA \xc3\xa4"));
  EXPECT_EQ(-1, CompareCaseInsensitiveASCII("AAA \xc3\x84", "aaa \xc3\xa4"));
  EXPECT_EQ(1, CompareCaseInsensitiveASCII("aaa \xc3\xa4", "AAA \xc3\x84"));

  // ASCII bytes should sort before non-ASCII ones.
  EXPECT_EQ(-1, CompareCaseInsensitiveASCII("a", "\xc3\xa4"));
  EXPECT_EQ(1, CompareCaseInsensitiveASCII("\xc3\xa4", "a"));

  // For constexpr.
  static_assert(CompareCaseInsensitiveASCII("", "") == 0);
  static_assert(CompareCaseInsensitiveASCII("Asdf", "aSDf") == 0);
  static_assert(CompareCaseInsensitiveASCII("Asdf", "aSDfA") == -1);
  static_assert(CompareCaseInsensitiveASCII("AsdfA", "aSDf") == 1);
  static_assert(CompareCaseInsensitiveASCII("AsdfA", "aSDfb") == -1);
  static_assert(CompareCaseInsensitiveASCII("Asdfb", "aSDfA") == 1);
  static_assert(CompareCaseInsensitiveASCII("aaa \xc3\xa4", "AAA \xc3\xa4") ==
                0);
  static_assert(CompareCaseInsensitiveASCII("AAA \xc3\x84", "aaa \xc3\xa4") ==
                -1);
  static_assert(CompareCaseInsensitiveASCII("aaa \xc3\xa4", "AAA \xc3\x84") ==
                1);
  static_assert(CompareCaseInsensitiveASCII("a", "\xc3\xa4") == -1);
  static_assert(CompareCaseInsensitiveASCII("\xc3\xa4", "a") == 1);
}

TEST(StringUtilTest, EqualsCaseInsensitiveASCII) {
  EXPECT_TRUE(EqualsCaseInsensitiveASCII("", ""));
  EXPECT_TRUE(EqualsCaseInsensitiveASCII("Asdf", "aSDF"));
  EXPECT_FALSE(EqualsCaseInsensitiveASCII("bsdf", "aSDF"));
  EXPECT_FALSE(EqualsCaseInsensitiveASCII("Asdf", "aSDFz"));

  EXPECT_TRUE(EqualsCaseInsensitiveASCII(u"", u""));
  EXPECT_TRUE(EqualsCaseInsensitiveASCII(u"Asdf", u"aSDF"));
  EXPECT_FALSE(EqualsCaseInsensitiveASCII(u"bsdf", u"aSDF"));
  EXPECT_FALSE(EqualsCaseInsensitiveASCII(u"Asdf", u"aSDFz"));

  EXPECT_TRUE(EqualsCaseInsensitiveASCII(u"", ""));
  EXPECT_TRUE(EqualsCaseInsensitiveASCII(u"Asdf", "aSDF"));
  EXPECT_FALSE(EqualsCaseInsensitiveASCII(u"bsdf", "aSDF"));
  EXPECT_FALSE(EqualsCaseInsensitiveASCII(u"Asdf", "aSDFz"));

  EXPECT_TRUE(EqualsCaseInsensitiveASCII("", u""));
  EXPECT_TRUE(EqualsCaseInsensitiveASCII("Asdf", u"aSDF"));
  EXPECT_FALSE(EqualsCaseInsensitiveASCII("bsdf", u"aSDF"));
  EXPECT_FALSE(EqualsCaseInsensitiveASCII("Asdf", u"aSDFz"));

  // Non-ASCII bytes are permitted, but they will be compared case-sensitively.
  EXPECT_TRUE(EqualsCaseInsensitiveASCII("aaa \xc3\xa4", "AAA \xc3\xa4"));
  EXPECT_FALSE(EqualsCaseInsensitiveASCII("aaa \xc3\x84", "AAA \xc3\xa4"));

  // The `std::wstring_view` overloads are only defined on Windows.
#if BUILDFLAG(IS_WIN)
  EXPECT_TRUE(EqualsCaseInsensitiveASCII(L"", L""));
  EXPECT_TRUE(EqualsCaseInsensitiveASCII(L"Asdf", L"aSDF"));
  EXPECT_FALSE(EqualsCaseInsensitiveASCII(L"bsdf", L"aSDF"));
  EXPECT_FALSE(EqualsCaseInsensitiveASCII(L"Asdf", L"aSDFz"));

  EXPECT_TRUE(EqualsCaseInsensitiveASCII(L"", ""));
  EXPECT_TRUE(EqualsCaseInsensitiveASCII(L"Asdf", "aSDF"));
  EXPECT_FALSE(EqualsCaseInsensitiveASCII(L"bsdf", "aSDF"));
  EXPECT_FALSE(EqualsCaseInsensitiveASCII(L"Asdf", "aSDFz"));

  EXPECT_TRUE(EqualsCaseInsensitiveASCII("", L""));
  EXPECT_TRUE(EqualsCaseInsensitiveASCII("Asdf", L"aSDF"));
  EXPECT_FALSE(EqualsCaseInsensitiveASCII("bsdf", L"aSDF"));
  EXPECT_FALSE(EqualsCaseInsensitiveASCII("Asdf", L"aSDFz"));
#endif
}

TEST(StringUtilTest, IsUnicodeWhitespace) {
  // NOT unicode white space.
  EXPECT_FALSE(IsUnicodeWhitespace(L'\0'));
  EXPECT_FALSE(IsUnicodeWhitespace(L'A'));
  EXPECT_FALSE(IsUnicodeWhitespace(L'0'));
  EXPECT_FALSE(IsUnicodeWhitespace(L'.'));
  EXPECT_FALSE(IsUnicodeWhitespace(L';'));
  EXPECT_FALSE(IsUnicodeWhitespace(L'\x4100'));

  // Actual unicode whitespace.
  EXPECT_TRUE(IsUnicodeWhitespace(L' '));
  EXPECT_TRUE(IsUnicodeWhitespace(L'\xa0'));
  EXPECT_TRUE(IsUnicodeWhitespace(L'\x3000'));
  EXPECT_TRUE(IsUnicodeWhitespace(L'\t'));
  EXPECT_TRUE(IsUnicodeWhitespace(L'\r'));
  EXPECT_TRUE(IsUnicodeWhitespace(L'\v'));
  EXPECT_TRUE(IsUnicodeWhitespace(L'\f'));
  EXPECT_TRUE(IsUnicodeWhitespace(L'\n'));
}

// Tests that MakeStringViewWithNulChars preserves internal NUL characters.
TEST(StringUtilTest, MakeStringViewWithNulChars) {
  {
    const char kTestString[] = "abd\0def";
    auto s = MakeStringViewWithNulChars(kTestString);
    EXPECT_EQ(s.size(), 7u);
    EXPECT_EQ(base::span(s), base::span_from_cstring(kTestString));
  }
  {
    const wchar_t kTestString[] = L"abd\0def";
    auto s = MakeStringViewWithNulChars(kTestString);
    EXPECT_EQ(s.size(), 7u);
    ASSERT_TRUE(base::span(s) == base::span_from_cstring(kTestString));
  }
  {
    const char16_t kTestString[] = u"abd\0def";
    auto s = MakeStringViewWithNulChars(kTestString);
    EXPECT_EQ(s.size(), 7u);
    EXPECT_TRUE(base::span(s) == base::span_from_cstring(kTestString));
  }
  {
    const char32_t kTestString[] = U"abd\0def";
    auto s = MakeStringViewWithNulChars(kTestString);
    EXPECT_EQ(s.size(), 7u);
    EXPECT_TRUE(base::span(s) == base::span_from_cstring(kTestString));
  }
}

class WriteIntoTest : public testing::Test {
 protected:
  static void WritesCorrectly(size_t num_chars) {
    std::string buffer;
    char kOriginal[] = "supercali";
    strncpy(WriteInto(&buffer, num_chars + 1), kOriginal, num_chars);
    // Using std::string(buffer.c_str()) instead of |buffer| truncates the
    // string at the first \0.
    EXPECT_EQ(
        std::string(kOriginal, std::min(num_chars, std::size(kOriginal) - 1)),
        std::string(buffer.c_str()));
    EXPECT_EQ(num_chars, buffer.size());
  }
};

TEST_F(WriteIntoTest, WriteInto) {
  // Validate that WriteInto reserves enough space and
  // sizes a string correctly.
  WritesCorrectly(1);
  WritesCorrectly(2);
  WritesCorrectly(5000);

  // Validate that WriteInto handles 0-length strings
  std::string empty;
  const char kOriginal[] = "original";
  strncpy(WriteInto(&empty, 1), kOriginal, 0);
  EXPECT_STREQ("", empty.c_str());
  EXPECT_EQ(0u, empty.size());

  // Validate that WriteInto doesn't modify other strings
  // when using a Copy-on-Write implementation.
  const char kLive[] = "live";
  const char kDead[] = "dead";
  const std::string live = kLive;
  std::string dead = live;
  strncpy(WriteInto(&dead, 5), kDead, 4);
  EXPECT_EQ(kDead, dead);
  EXPECT_EQ(4u, dead.size());
  EXPECT_EQ(kLive, live);
  EXPECT_EQ(4u, live.size());
}

}  // namespace

}  // namespace base
