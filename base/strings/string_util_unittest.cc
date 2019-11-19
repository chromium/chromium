// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"

#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <type_traits>

#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace base {

static const struct trim_case {
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

static const struct trim_case_ascii {
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

namespace {

// Helper used to test TruncateUTF8ToByteSize.
bool Truncated(const std::string& input,
               const size_t byte_size,
               std::string* output) {
    size_t prev = input.length();
    TruncateUTF8ToByteSize(input, byte_size, output);
    return prev != output->length();
}

}  // namespace

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
    const std::string array_string(array, base::size(array));
    EXPECT_TRUE(Truncated(array_string, 4, &output));
    EXPECT_EQ(output.compare(std::string("\x00\x00\xc2\x81", 4)), 0);
  }

  {
    const char array[] = "\x00\xc2\x81\xc2\x81";
    const std::string array_string(array, base::size(array));
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
    const std::string array_string(array, base::size(array));
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
    const std::string array_string(array, base::size(array));
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

#if defined(WCHAR_T_IS_UTF16)
TEST(StringUtilTest, as_wcstr) {
  char16 rw_buffer[10] = {};
  static_assert(
      std::is_same<wchar_t*, decltype(as_writable_wcstr(rw_buffer))>::value,
      "");
  EXPECT_EQ(static_cast<void*>(rw_buffer), as_writable_wcstr(rw_buffer));

  string16 rw_str(10, '\0');
  static_assert(
      std::is_same<wchar_t*, decltype(as_writable_wcstr(rw_str))>::value, "");
  EXPECT_EQ(static_cast<const void*>(rw_str.data()), as_writable_wcstr(rw_str));

  const char16 ro_buffer[10] = {};
  static_assert(
      std::is_same<const wchar_t*, decltype(as_wcstr(ro_buffer))>::value, "");
  EXPECT_EQ(static_cast<const void*>(ro_buffer), as_wcstr(ro_buffer));

  const string16 ro_str(10, '\0');
  static_assert(std::is_same<const wchar_t*, decltype(as_wcstr(ro_str))>::value,
                "");
  EXPECT_EQ(static_cast<const void*>(ro_str.data()), as_wcstr(ro_str));

  StringPiece16 piece = ro_buffer;
  static_assert(std::is_same<const wchar_t*, decltype(as_wcstr(piece))>::value,
                "");
  EXPECT_EQ(static_cast<const void*>(piece.data()), as_wcstr(piece));
}

TEST(StringUtilTest, as_u16cstr) {
  wchar_t rw_buffer[10] = {};
  static_assert(
      std::is_same<char16*, decltype(as_writable_u16cstr(rw_buffer))>::value,
      "");
  EXPECT_EQ(static_cast<void*>(rw_buffer), as_writable_u16cstr(rw_buffer));

  std::wstring rw_str(10, '\0');
  static_assert(
      std::is_same<char16*, decltype(as_writable_u16cstr(rw_str))>::value, "");
  EXPECT_EQ(static_cast<const void*>(rw_str.data()),
            as_writable_u16cstr(rw_str));

  const wchar_t ro_buffer[10] = {};
  static_assert(
      std::is_same<const char16*, decltype(as_u16cstr(ro_buffer))>::value, "");
  EXPECT_EQ(static_cast<const void*>(ro_buffer), as_u16cstr(ro_buffer));

  const std::wstring ro_str(10, '\0');
  static_assert(
      std::is_same<const char16*, decltype(as_u16cstr(ro_str))>::value, "");
  EXPECT_EQ(static_cast<const void*>(ro_str.data()), as_u16cstr(ro_str));

  WStringPiece piece = ro_buffer;
  static_assert(std::is_same<const char16*, decltype(as_u16cstr(piece))>::value,
                "");
  EXPECT_EQ(static_cast<const void*>(piece.data()), as_u16cstr(piece));
}
#endif  // defined(WCHAR_T_IS_UTF16)

TEST(StringUtilTest, TrimWhitespace) {
  string16 output;  // Allow contents to carry over to next testcase
  for (const auto& value : trim_cases) {
    EXPECT_EQ(value.return_value,
              TrimWhitespace(WideToUTF16(value.input), value.positions,
                             &output));
    EXPECT_EQ(WideToUTF16(value.output), output);
  }

  // Test that TrimWhitespace() can take the same string for input and output
  output = ASCIIToUTF16("  This is a test \r\n");
  EXPECT_EQ(TRIM_ALL, TrimWhitespace(output, TRIM_ALL, &output));
  EXPECT_EQ(ASCIIToUTF16("This is a test"), output);

  // Once more, but with a string of whitespace
  output = ASCIIToUTF16("  \r\n");
  EXPECT_EQ(TRIM_ALL, TrimWhitespace(output, TRIM_ALL, &output));
  EXPECT_EQ(string16(), output);

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
  {"  \tFoo  bar  \n", true, "Foo bar"},
  {" a \r b\n c \r\n d \t\re \t f \n ", true, "abcde f"},
};

TEST(StringUtilTest, CollapseWhitespaceASCII) {
  for (const auto& value : collapse_cases_ascii) {
    EXPECT_EQ(value.output, CollapseWhitespaceASCII(value.input, value.trim));
  }
}

TEST(StringUtilTest, IsStringUTF8) {
  EXPECT_TRUE(IsStringUTF8("abc"));
  EXPECT_TRUE(IsStringUTF8("\xc2\x81"));
  EXPECT_TRUE(IsStringUTF8("\xe1\x80\xbf"));
  EXPECT_TRUE(IsStringUTF8("\xf1\x80\xa0\xbf"));
  EXPECT_TRUE(IsStringUTF8("a\xc2\x81\xe1\x80\xbf\xf1\x80\xa0\xbf"));
  EXPECT_TRUE(IsStringUTF8("\xef\xbb\xbf" "abc"));  // UTF-8 BOM

  // surrogate code points
  EXPECT_FALSE(IsStringUTF8("\xed\xa0\x80\xed\xbf\xbf"));
  EXPECT_FALSE(IsStringUTF8("\xed\xa0\x8f"));
  EXPECT_FALSE(IsStringUTF8("\xed\xbf\xbf"));

  // overlong sequences
  EXPECT_FALSE(IsStringUTF8("\xc0\x80"));  // U+0000
  EXPECT_FALSE(IsStringUTF8("\xc1\x80\xc1\x81"));  // "AB"
  EXPECT_FALSE(IsStringUTF8("\xe0\x80\x80"));  // U+0000
  EXPECT_FALSE(IsStringUTF8("\xe0\x82\x80"));  // U+0080
  EXPECT_FALSE(IsStringUTF8("\xe0\x9f\xbf"));  // U+07ff
  EXPECT_FALSE(IsStringUTF8("\xf0\x80\x80\x8D"));  // U+000D
  EXPECT_FALSE(IsStringUTF8("\xf0\x80\x82\x91"));  // U+0091
  EXPECT_FALSE(IsStringUTF8("\xf0\x80\xa0\x80"));  // U+0800
  EXPECT_FALSE(IsStringUTF8("\xf0\x8f\xbb\xbf"));  // U+FEFF (BOM)
  EXPECT_FALSE(IsStringUTF8("\xf8\x80\x80\x80\xbf"));  // U+003F
  EXPECT_FALSE(IsStringUTF8("\xfc\x80\x80\x80\xa0\xa5"));  // U+00A5

  // Beyond U+10FFFF (the upper limit of Unicode codespace)
  EXPECT_FALSE(IsStringUTF8("\xf4\x90\x80\x80"));  // U+110000
  EXPECT_FALSE(IsStringUTF8("\xf8\xa0\xbf\x80\xbf"));  // 5 bytes
  EXPECT_FALSE(IsStringUTF8("\xfc\x9c\xbf\x80\xbf\x80"));  // 6 bytes

  // BOMs in UTF-16(BE|LE) and UTF-32(BE|LE)
  EXPECT_FALSE(IsStringUTF8("\xfe\xff"));
  EXPECT_FALSE(IsStringUTF8("\xff\xfe"));
  EXPECT_FALSE(IsStringUTF8(std::string("\x00\x00\xfe\xff", 4)));
  EXPECT_FALSE(IsStringUTF8("\xff\xfe\x00\x00"));

  // Non-characters : U+xxFFF[EF] where xx is 0x00 through 0x10 and <FDD0,FDEF>
  EXPECT_FALSE(IsStringUTF8("\xef\xbf\xbe"));  // U+FFFE)
  EXPECT_FALSE(IsStringUTF8("\xf0\x8f\xbf\xbe"));  // U+1FFFE
  EXPECT_FALSE(IsStringUTF8("\xf3\xbf\xbf\xbf"));  // U+10FFFF
  EXPECT_FALSE(IsStringUTF8("\xef\xb7\x90"));  // U+FDD0
  EXPECT_FALSE(IsStringUTF8("\xef\xb7\xaf"));  // U+FDEF
  // Strings in legacy encodings. We can certainly make up strings
  // in a legacy encoding that are valid in UTF-8, but in real data,
  // most of them are invalid as UTF-8.
  EXPECT_FALSE(IsStringUTF8("caf\xe9"));  // cafe with U+00E9 in ISO-8859-1
  EXPECT_FALSE(IsStringUTF8("\xb0\xa1\xb0\xa2"));  // U+AC00, U+AC001 in EUC-KR
  EXPECT_FALSE(IsStringUTF8("\xa7\x41\xa6\x6e"));  // U+4F60 U+597D in Big5
  // "abc" with U+201[CD] in windows-125[0-8]
  EXPECT_FALSE(IsStringUTF8("\x93" "abc\x94"));
  // U+0639 U+064E U+0644 U+064E in ISO-8859-6
  EXPECT_FALSE(IsStringUTF8("\xd9\xee\xe4\xee"));
  // U+03B3 U+03B5 U+03B9 U+03AC in ISO-8859-7
  EXPECT_FALSE(IsStringUTF8("\xe3\xe5\xe9\xdC"));

  // Check that we support Embedded Nulls. The first uses the canonical UTF-8
  // representation, and the second uses a 2-byte sequence. The second version
  // is invalid UTF-8 since UTF-8 states that the shortest encoding for a
  // given codepoint must be used.
  static const char kEmbeddedNull[] = "embedded\0null";
  EXPECT_TRUE(IsStringUTF8(
      std::string(kEmbeddedNull, sizeof(kEmbeddedNull))));
  EXPECT_FALSE(IsStringUTF8("embedded\xc0\x80U+0000"));
}

TEST(StringUtilTest, IsStringASCII) {
  static char char_ascii[] =
      "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF";
  static char16 char16_ascii[] = {
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', 'A',
      'B', 'C', 'D', 'E', 'F', '0', '1', '2', '3', '4', '5', '6',
      '7', '8', '9', '0', 'A', 'B', 'C', 'D', 'E', 'F', 0 };
  static std::wstring wchar_ascii(
      L"0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF");

  // Test a variety of the fragment start positions and lengths in order to make
  // sure that bit masking in IsStringASCII works correctly.
  // Also, test that a non-ASCII character will be detected regardless of its
  // position inside the string.
  {
    const size_t string_length = base::size(char_ascii) - 1;
    for (size_t offset = 0; offset < 8; ++offset) {
      for (size_t len = 0, max_len = string_length - offset; len < max_len;
           ++len) {
        EXPECT_TRUE(IsStringASCII(StringPiece(char_ascii + offset, len)));
        for (size_t char_pos = offset; char_pos < len; ++char_pos) {
          char_ascii[char_pos] |= '\x80';
          EXPECT_FALSE(IsStringASCII(StringPiece(char_ascii + offset, len)));
          char_ascii[char_pos] &= ~'\x80';
        }
      }
    }
  }

  {
    const size_t string_length = base::size(char16_ascii) - 1;
    for (size_t offset = 0; offset < 4; ++offset) {
      for (size_t len = 0, max_len = string_length - offset; len < max_len;
           ++len) {
        EXPECT_TRUE(IsStringASCII(StringPiece16(char16_ascii + offset, len)));
        for (size_t char_pos = offset; char_pos < len; ++char_pos) {
          char16_ascii[char_pos] |= 0x80;
          EXPECT_FALSE(
              IsStringASCII(StringPiece16(char16_ascii + offset, len)));
          char16_ascii[char_pos] &= ~0x80;
          // Also test when the upper half is non-zero.
          char16_ascii[char_pos] |= 0x100;
          EXPECT_FALSE(
              IsStringASCII(StringPiece16(char16_ascii + offset, len)));
          char16_ascii[char_pos] &= ~0x100;
        }
      }
    }
  }

#if defined(WCHAR_T_IS_UTF32)
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
#endif  // WCHAR_T_IS_UTF32
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

  for (size_t i = 0; i < base::size(char_cases); ++i) {
    EXPECT_TRUE(IsStringASCII(char_cases[i]));
    string16 utf16 = ASCIIToUTF16(char_cases[i]);
    EXPECT_EQ(WideToUTF16(wchar_cases[i]), utf16);

    std::string ascii = UTF16ToASCII(WideToUTF16(wchar_cases[i]));
    EXPECT_EQ(char_cases[i], ascii);
  }

  EXPECT_FALSE(IsStringASCII("Google \x80Video"));

  // Convert empty strings.
  string16 empty16;
  std::string empty;
  EXPECT_EQ(empty, UTF16ToASCII(empty16));
  EXPECT_EQ(empty16, ASCIIToUTF16(empty));

  // Convert strings with an embedded NUL character.
  const char chars_with_nul[] = "test\0string";
  const int length_with_nul = base::size(chars_with_nul) - 1;
  std::string string_with_nul(chars_with_nul, length_with_nul);
  string16 string16_with_nul = ASCIIToUTF16(string_with_nul);
  EXPECT_EQ(static_cast<string16::size_type>(length_with_nul),
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

  EXPECT_EQ(static_cast<char16>('c'), ToLowerASCII(static_cast<char16>('C')));
  EXPECT_EQ(static_cast<char16>('c'), ToLowerASCII(static_cast<char16>('c')));
  EXPECT_EQ(static_cast<char16>('2'), ToLowerASCII(static_cast<char16>('2')));

  EXPECT_EQ("cc2", ToLowerASCII("Cc2"));
  EXPECT_EQ(ASCIIToUTF16("cc2"), ToLowerASCII(ASCIIToUTF16("Cc2")));
}

TEST(StringUtilTest, ToUpperASCII) {
  EXPECT_EQ('C', ToUpperASCII('C'));
  EXPECT_EQ('C', ToUpperASCII('c'));
  EXPECT_EQ('2', ToUpperASCII('2'));

  EXPECT_EQ(static_cast<char16>('C'), ToUpperASCII(static_cast<char16>('C')));
  EXPECT_EQ(static_cast<char16>('C'), ToUpperASCII(static_cast<char16>('c')));
  EXPECT_EQ(static_cast<char16>('2'), ToUpperASCII(static_cast<char16>('2')));

  EXPECT_EQ("CC2", ToUpperASCII("Cc2"));
  EXPECT_EQ(ASCIIToUTF16("CC2"), ToUpperASCII(ASCIIToUTF16("Cc2")));
}

TEST(StringUtilTest, LowerCaseEqualsASCII) {
  static const struct {
    const char*    src_a;
    const char*    dst;
  } lowercase_cases[] = {
    { "FoO", "foo" },
    { "foo", "foo" },
    { "FOO", "foo" },
  };

  for (const auto& i : lowercase_cases) {
    EXPECT_TRUE(LowerCaseEqualsASCII(ASCIIToUTF16(i.src_a), i.dst));
    EXPECT_TRUE(LowerCaseEqualsASCII(i.src_a, i.dst));
  }
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
    {1024*1024, "1.0 MB"},
    {1024*1024*1024, "1.0 GB"},
    {10LL*1024*1024*1024, "10.0 GB"},
    {99LL*1024*1024*1024, "99.0 GB"},
    {105LL*1024*1024*1024, "105 GB"},
    {105LL*1024*1024*1024 + 500LL*1024*1024, "105 GB"},
    {~(1LL << 63), "8192 PB"},

    {99*1024 + 103, "99.1 kB"},
    {1024*1024 + 103, "1.0 MB"},
    {1024*1024 + 205 * 1024, "1.2 MB"},
    {1024*1024*1024 + (927 * 1024*1024), "1.9 GB"},
    {10LL*1024*1024*1024, "10.0 GB"},
    {100LL*1024*1024*1024, "100 GB"},
  };

  for (const auto& i : cases) {
    EXPECT_EQ(ASCIIToUTF16(i.expected), FormatBytesUnlocalized(i.bytes));
  }
}
TEST(StringUtilTest, ReplaceSubstringsAfterOffset) {
  static const struct {
    StringPiece str;
    size_t start_offset;
    StringPiece find_this;
    StringPiece replace_with;
    StringPiece expected;
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

  // base::string16 variant
  for (const auto& scenario : cases) {
    string16 str = ASCIIToUTF16(scenario.str);
    ReplaceSubstringsAfterOffset(&str, scenario.start_offset,
                                 ASCIIToUTF16(scenario.find_this),
                                 ASCIIToUTF16(scenario.replace_with));
    EXPECT_EQ(ASCIIToUTF16(scenario.expected), str);
  }

  // std::string with insufficient capacity: expansion must realloc the buffer.
  for (const auto& scenario : cases) {
    std::string str = scenario.str.as_string();
    str.shrink_to_fit();  // This is nonbinding, but it's the best we've got.
    ReplaceSubstringsAfterOffset(&str, scenario.start_offset,
                                 scenario.find_this, scenario.replace_with);
    EXPECT_EQ(scenario.expected, str);
  }

  // std::string with ample capacity: should be possible to grow in-place.
  for (const auto& scenario : cases) {
    std::string str = scenario.str.as_string();
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
    string16::size_type start_offset;
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
    string16 str = ASCIIToUTF16(i.str);
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
  string16 separator = ASCIIToUTF16(", ");
  std::vector<string16> parts;
  EXPECT_EQ(string16(), JoinString(parts, separator));

  parts.push_back(string16());
  EXPECT_EQ(string16(), JoinString(parts, separator));
  parts.clear();

  parts.push_back(ASCIIToUTF16("a"));
  EXPECT_EQ(ASCIIToUTF16("a"), JoinString(parts, separator));

  parts.push_back(ASCIIToUTF16("b"));
  parts.push_back(ASCIIToUTF16("c"));
  EXPECT_EQ(ASCIIToUTF16("a, b, c"), JoinString(parts, separator));

  parts.push_back(ASCIIToUTF16(""));
  EXPECT_EQ(ASCIIToUTF16("a, b, c, "), JoinString(parts, separator));
  parts.push_back(ASCIIToUTF16(" "));
  EXPECT_EQ(ASCIIToUTF16("a|b|c|| "), JoinString(parts, ASCIIToUTF16("|")));
}

TEST(StringUtilTest, JoinStringPiece) {
  std::string separator(", ");
  std::vector<StringPiece> parts;
  EXPECT_EQ(std::string(), JoinString(parts, separator));

  // Test empty first part (https://crbug.com/698073).
  parts.push_back(StringPiece());
  EXPECT_EQ(std::string(), JoinString(parts, separator));
  parts.clear();

  parts.push_back("a");
  EXPECT_EQ("a", JoinString(parts, separator));

  parts.push_back("b");
  parts.push_back("c");
  EXPECT_EQ("a, b, c", JoinString(parts, separator));

  parts.push_back(StringPiece());
  EXPECT_EQ("a, b, c, ", JoinString(parts, separator));
  parts.push_back(" ");
  EXPECT_EQ("a|b|c|| ", JoinString(parts, "|"));
}

TEST(StringUtilTest, JoinStringPiece16) {
  string16 separator = ASCIIToUTF16(", ");
  std::vector<StringPiece16> parts;
  EXPECT_EQ(string16(), JoinString(parts, separator));

  // Test empty first part (https://crbug.com/698073).
  parts.push_back(StringPiece16());
  EXPECT_EQ(string16(), JoinString(parts, separator));
  parts.clear();

  const string16 kA = ASCIIToUTF16("a");
  parts.push_back(kA);
  EXPECT_EQ(ASCIIToUTF16("a"), JoinString(parts, separator));

  const string16 kB = ASCIIToUTF16("b");
  parts.push_back(kB);
  const string16 kC = ASCIIToUTF16("c");
  parts.push_back(kC);
  EXPECT_EQ(ASCIIToUTF16("a, b, c"), JoinString(parts, separator));

  parts.push_back(StringPiece16());
  EXPECT_EQ(ASCIIToUTF16("a, b, c, "), JoinString(parts, separator));
  const string16 kSpace = ASCIIToUTF16(" ");
  parts.push_back(kSpace);
  EXPECT_EQ(ASCIIToUTF16("a|b|c|| "), JoinString(parts, ASCIIToUTF16("|")));
}

TEST(StringUtilTest, JoinStringInitializerList) {
  std::string separator(", ");
  EXPECT_EQ(std::string(), JoinString({}, separator));

  // Test empty first part (https://crbug.com/698073).
  EXPECT_EQ(std::string(), JoinString({StringPiece()}, separator));

  // With const char*s.
  EXPECT_EQ("a", JoinString({"a"}, separator));
  EXPECT_EQ("a, b, c", JoinString({"a", "b", "c"}, separator));
  EXPECT_EQ("a, b, c, ", JoinString({"a", "b", "c", StringPiece()}, separator));
  EXPECT_EQ("a|b|c|| ", JoinString({"a", "b", "c", StringPiece(), " "}, "|"));

  // With std::strings.
  const std::string kA = "a";
  const std::string kB = "b";
  EXPECT_EQ("a, b", JoinString({kA, kB}, separator));

  // With StringPieces.
  const StringPiece kPieceA = kA;
  const StringPiece kPieceB = kB;
  EXPECT_EQ("a, b", JoinString({kPieceA, kPieceB}, separator));
}

TEST(StringUtilTest, JoinStringInitializerList16) {
  string16 separator = ASCIIToUTF16(", ");
  EXPECT_EQ(string16(), JoinString({}, separator));

  // Test empty first part (https://crbug.com/698073).
  EXPECT_EQ(string16(), JoinString({StringPiece16()}, separator));

  // With string16s.
  const string16 kA = ASCIIToUTF16("a");
  EXPECT_EQ(ASCIIToUTF16("a"), JoinString({kA}, separator));

  const string16 kB = ASCIIToUTF16("b");
  const string16 kC = ASCIIToUTF16("c");
  EXPECT_EQ(ASCIIToUTF16("a, b, c"), JoinString({kA, kB, kC}, separator));

  EXPECT_EQ(ASCIIToUTF16("a, b, c, "),
            JoinString({kA, kB, kC, StringPiece16()}, separator));
  const string16 kSpace = ASCIIToUTF16(" ");
  EXPECT_EQ(
      ASCIIToUTF16("a|b|c|| "),
      JoinString({kA, kB, kC, StringPiece16(), kSpace}, ASCIIToUTF16("|")));

  // With StringPiece16s.
  const StringPiece16 kPieceA = kA;
  const StringPiece16 kPieceB = kB;
  EXPECT_EQ(ASCIIToUTF16("a, b"), JoinString({kPieceA, kPieceB}, separator));
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

  EXPECT_TRUE(StartsWith(ASCIIToUTF16("javascript:url"),
                         ASCIIToUTF16("javascript"),
                         base::CompareCase::SENSITIVE));
  EXPECT_FALSE(StartsWith(ASCIIToUTF16("JavaScript:url"),
                          ASCIIToUTF16("javascript"),
                          base::CompareCase::SENSITIVE));
  EXPECT_TRUE(StartsWith(ASCIIToUTF16("javascript:url"),
                         ASCIIToUTF16("javascript"),
                         base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(StartsWith(ASCIIToUTF16("JavaScript:url"),
                         ASCIIToUTF16("javascript"),
                         base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(StartsWith(ASCIIToUTF16("java"), ASCIIToUTF16("javascript"),
                          base::CompareCase::SENSITIVE));
  EXPECT_FALSE(StartsWith(ASCIIToUTF16("java"), ASCIIToUTF16("javascript"),
                          base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(StartsWith(string16(), ASCIIToUTF16("javascript"),
                          base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(StartsWith(string16(), ASCIIToUTF16("javascript"),
                          base::CompareCase::SENSITIVE));
  EXPECT_TRUE(StartsWith(ASCIIToUTF16("java"), string16(),
                         base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(StartsWith(ASCIIToUTF16("java"), string16(),
                         base::CompareCase::SENSITIVE));
}

TEST(StringUtilTest, EndsWith) {
  EXPECT_TRUE(EndsWith(ASCIIToUTF16("Foo.plugin"), ASCIIToUTF16(".plugin"),
                       base::CompareCase::SENSITIVE));
  EXPECT_FALSE(EndsWith(ASCIIToUTF16("Foo.Plugin"), ASCIIToUTF16(".plugin"),
                        base::CompareCase::SENSITIVE));
  EXPECT_TRUE(EndsWith(ASCIIToUTF16("Foo.plugin"), ASCIIToUTF16(".plugin"),
                       base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(EndsWith(ASCIIToUTF16("Foo.Plugin"), ASCIIToUTF16(".plugin"),
                       base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(EndsWith(ASCIIToUTF16(".plug"), ASCIIToUTF16(".plugin"),
                        base::CompareCase::SENSITIVE));
  EXPECT_FALSE(EndsWith(ASCIIToUTF16(".plug"), ASCIIToUTF16(".plugin"),
                        base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(EndsWith(ASCIIToUTF16("Foo.plugin Bar"), ASCIIToUTF16(".plugin"),
                        base::CompareCase::SENSITIVE));
  EXPECT_FALSE(EndsWith(ASCIIToUTF16("Foo.plugin Bar"), ASCIIToUTF16(".plugin"),
                        base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(EndsWith(string16(), ASCIIToUTF16(".plugin"),
                        base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_FALSE(EndsWith(string16(), ASCIIToUTF16(".plugin"),
                        base::CompareCase::SENSITIVE));
  EXPECT_TRUE(EndsWith(ASCIIToUTF16("Foo.plugin"), string16(),
                       base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(EndsWith(ASCIIToUTF16("Foo.plugin"), string16(),
                       base::CompareCase::SENSITIVE));
  EXPECT_TRUE(EndsWith(ASCIIToUTF16(".plugin"), ASCIIToUTF16(".plugin"),
                       base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(EndsWith(ASCIIToUTF16(".plugin"), ASCIIToUTF16(".plugin"),
                       base::CompareCase::SENSITIVE));
  EXPECT_TRUE(
      EndsWith(string16(), string16(), base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_TRUE(EndsWith(string16(), string16(), base::CompareCase::SENSITIVE));
}

TEST(StringUtilTest, GetStringFWithOffsets) {
  std::vector<string16> subst;
  subst.push_back(ASCIIToUTF16("1"));
  subst.push_back(ASCIIToUTF16("2"));
  std::vector<size_t> offsets;

  ReplaceStringPlaceholders(ASCIIToUTF16("Hello, $1. Your number is $2."),
                            subst,
                            &offsets);
  EXPECT_EQ(2U, offsets.size());
  EXPECT_EQ(7U, offsets[0]);
  EXPECT_EQ(25U, offsets[1]);
  offsets.clear();

  ReplaceStringPlaceholders(ASCIIToUTF16("Hello, $2. Your number is $1."),
                            subst,
                            &offsets);
  EXPECT_EQ(2U, offsets.size());
  EXPECT_EQ(25U, offsets[0]);
  EXPECT_EQ(7U, offsets[1]);
  offsets.clear();
}

TEST(StringUtilTest, ReplaceStringPlaceholdersTooFew) {
  // Test whether replacestringplaceholders works as expected when there
  // are fewer inputs than outputs.
  std::vector<string16> subst;
  subst.push_back(ASCIIToUTF16("9a"));
  subst.push_back(ASCIIToUTF16("8b"));
  subst.push_back(ASCIIToUTF16("7c"));

  string16 formatted =
      ReplaceStringPlaceholders(
          ASCIIToUTF16("$1a,$2b,$3c,$4d,$5e,$6f,$1g,$2h,$3i"), subst, nullptr);

  EXPECT_EQ(ASCIIToUTF16("9aa,8bb,7cc,d,e,f,9ag,8bh,7ci"), formatted);
}

TEST(StringUtilTest, ReplaceStringPlaceholders) {
  std::vector<string16> subst;
  subst.push_back(ASCIIToUTF16("9a"));
  subst.push_back(ASCIIToUTF16("8b"));
  subst.push_back(ASCIIToUTF16("7c"));
  subst.push_back(ASCIIToUTF16("6d"));
  subst.push_back(ASCIIToUTF16("5e"));
  subst.push_back(ASCIIToUTF16("4f"));
  subst.push_back(ASCIIToUTF16("3g"));
  subst.push_back(ASCIIToUTF16("2h"));
  subst.push_back(ASCIIToUTF16("1i"));

  string16 formatted =
      ReplaceStringPlaceholders(
          ASCIIToUTF16("$1a,$2b,$3c,$4d,$5e,$6f,$7g,$8h,$9i"), subst, nullptr);

  EXPECT_EQ(ASCIIToUTF16("9aa,8bb,7cc,6dd,5ee,4ff,3gg,2hh,1ii"), formatted);
}

TEST(StringUtilTest, ReplaceStringPlaceholdersNetExpansionWithContraction) {
  // In this test, some of the substitutions are shorter than the placeholders,
  // but overall the string gets longer.
  std::vector<string16> subst;
  subst.push_back(ASCIIToUTF16("9a____"));
  subst.push_back(ASCIIToUTF16("B"));
  subst.push_back(ASCIIToUTF16("7c___"));
  subst.push_back(ASCIIToUTF16("d"));
  subst.push_back(ASCIIToUTF16("5e____"));
  subst.push_back(ASCIIToUTF16("F"));
  subst.push_back(ASCIIToUTF16("3g___"));
  subst.push_back(ASCIIToUTF16("h"));
  subst.push_back(ASCIIToUTF16("1i_____"));

  string16 original = ASCIIToUTF16("$1a,$2b,$3c,$4d,$5e,$6f,$7g,$8h,$9i");
  string16 expected =
      ASCIIToUTF16("9a____a,Bb,7c___c,dd,5e____e,Ff,3g___g,hh,1i_____i");

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
  std::vector<string16> subst;
  subst.push_back(ASCIIToUTF16("z"));
  subst.push_back(ASCIIToUTF16("y"));
  subst.push_back(ASCIIToUTF16("XYZW"));
  subst.push_back(ASCIIToUTF16("x"));
  subst.push_back(ASCIIToUTF16("w"));

  string16 formatted =
      ReplaceStringPlaceholders(ASCIIToUTF16("$3_$4$2$1$5"), subst, nullptr);

  EXPECT_EQ(ASCIIToUTF16("XYZW_xyzw"), formatted);
}

TEST(StringUtilTest, ReplaceStringPlaceholdersOneDigit) {
  std::vector<string16> subst;
  subst.push_back(ASCIIToUTF16("1a"));
  string16 formatted =
      ReplaceStringPlaceholders(ASCIIToUTF16(" $16 "), subst, nullptr);
  EXPECT_EQ(ASCIIToUTF16(" 1a6 "), formatted);
}

TEST(StringUtilTest, ReplaceStringPlaceholdersInvalidPlaceholder) {
  std::vector<string16> subst;
  subst.push_back(ASCIIToUTF16("1a"));
  string16 formatted =
      ReplaceStringPlaceholders(ASCIIToUTF16("+$-+$A+$1+"), subst, nullptr);
  EXPECT_EQ(ASCIIToUTF16("+++1a+"), formatted);
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
    wchar_t wdst[10];
    EXPECT_EQ(7U, strlcpy(dst, "abcdefg", base::size(dst)));
    EXPECT_EQ(0, memcmp(dst, "abcdefg", 8));
    EXPECT_EQ(7U, wcslcpy(wdst, L"abcdefg", base::size(wdst)));
    EXPECT_EQ(0, memcmp(wdst, L"abcdefg", sizeof(wchar_t) * 8));
  }

  // Test dst_size == 0, nothing should be written to |dst| and we should
  // have the equivalent of strlen(src).
  {
    char dst[2] = {1, 2};
    wchar_t wdst[2] = {1, 2};
    EXPECT_EQ(7U, strlcpy(dst, "abcdefg", 0));
    EXPECT_EQ(1, dst[0]);
    EXPECT_EQ(2, dst[1]);
    EXPECT_EQ(7U, wcslcpy(wdst, L"abcdefg", 0));
    EXPECT_EQ(static_cast<wchar_t>(1), wdst[0]);
    EXPECT_EQ(static_cast<wchar_t>(2), wdst[1]);
  }

  // Test the case were we _just_ competely fit including the null.
  {
    char dst[8];
    wchar_t wdst[8];
    EXPECT_EQ(7U, strlcpy(dst, "abcdefg", base::size(dst)));
    EXPECT_EQ(0, memcmp(dst, "abcdefg", 8));
    EXPECT_EQ(7U, wcslcpy(wdst, L"abcdefg", base::size(wdst)));
    EXPECT_EQ(0, memcmp(wdst, L"abcdefg", sizeof(wchar_t) * 8));
  }

  // Test the case were we we are one smaller, so we can't fit the null.
  {
    char dst[7];
    wchar_t wdst[7];
    EXPECT_EQ(7U, strlcpy(dst, "abcdefg", base::size(dst)));
    EXPECT_EQ(0, memcmp(dst, "abcdef", 7));
    EXPECT_EQ(7U, wcslcpy(wdst, L"abcdefg", base::size(wdst)));
    EXPECT_EQ(0, memcmp(wdst, L"abcdef", sizeof(wchar_t) * 7));
  }

  // Test the case were we are just too small.
  {
    char dst[3];
    wchar_t wdst[3];
    EXPECT_EQ(7U, strlcpy(dst, "abcdefg", base::size(dst)));
    EXPECT_EQ(0, memcmp(dst, "ab", 3));
    EXPECT_EQ(7U, wcslcpy(wdst, L"abcdefg", base::size(wdst)));
    EXPECT_EQ(0, memcmp(wdst, L"ab", sizeof(wchar_t) * 3));
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

  EXPECT_TRUE(ContainsOnlyChars(string16(), kWhitespaceUTF16));
  EXPECT_TRUE(ContainsOnlyChars(ASCIIToUTF16(" "), kWhitespaceUTF16));
  EXPECT_TRUE(ContainsOnlyChars(ASCIIToUTF16("\t"), kWhitespaceUTF16));
  EXPECT_TRUE(ContainsOnlyChars(ASCIIToUTF16("\t \r \n  "), kWhitespaceUTF16));
  EXPECT_FALSE(ContainsOnlyChars(ASCIIToUTF16("a"), kWhitespaceUTF16));
  EXPECT_FALSE(ContainsOnlyChars(ASCIIToUTF16("\thello\r \n  "),
                                  kWhitespaceUTF16));
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
}

TEST(StringUtilTest, EqualsCaseInsensitiveASCII) {
  EXPECT_TRUE(EqualsCaseInsensitiveASCII("", ""));
  EXPECT_TRUE(EqualsCaseInsensitiveASCII("Asdf", "aSDF"));
  EXPECT_FALSE(EqualsCaseInsensitiveASCII("bsdf", "aSDF"));
  EXPECT_FALSE(EqualsCaseInsensitiveASCII("Asdf", "aSDFz"));
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

class WriteIntoTest : public testing::Test {
 protected:
  static void WritesCorrectly(size_t num_chars) {
    std::string buffer;
    char kOriginal[] = "supercali";
    strncpy(WriteInto(&buffer, num_chars + 1), kOriginal, num_chars);
    // Using std::string(buffer.c_str()) instead of |buffer| truncates the
    // string at the first \0.
    EXPECT_EQ(
        std::string(kOriginal, std::min(num_chars, base::size(kOriginal) - 1)),
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

}  // namespace base
