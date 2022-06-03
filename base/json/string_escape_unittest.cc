// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/string_escape.h"

#include <stddef.h>

#include "base/cxx17_backports.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(JSONStringEscapeTest, EscapeUTF8) {
  const struct {
    const char* to_escape;
    const char* escaped;
  } cases[] = {
      {"\b\001aZ\"\\wee", "\\b\\u0001aZ\\\"\\\\wee"},
      {"a\b\f\n\r\t\v\1\\.\"z", "a\\b\\f\\n\\r\\t\\u000B\\u0001\\\\.\\\"z"},
      {"b\x0f\x7f\xf0\xff!",  // \xf0\xff is not a valid UTF-8 unit.
       "b\\u000F\x7F\xEF\xBF\xBD\xEF\xBF\xBD!"},
      {"c<>d", "c\\u003C>d"},
      {"Hello\xE2\x80\xA8world", "Hello\\u2028world"},  // U+2028
      {"\xE2\x80\xA9purple", "\\u2029purple"},          // U+2029
      // Unicode non-characters.
      {"\xEF\xB7\x90", "\xEF\xB7\x90"},          // U+FDD0
      {"\xEF\xB7\x9F", "\xEF\xB7\x9F"},          // U+FDDF
      {"\xEF\xB7\xAF", "\xEF\xB7\xAF"},          // U+FDEF
      {"\xEF\xBF\xBE", "\xEF\xBF\xBE"},          // U+FFFE
      {"\xEF\xBF\xBF", "\xEF\xBF\xBF"},          // U+FFFF
      {"\xF0\x9F\xBF\xBE", "\xF0\x9F\xBF\xBE"},  // U+01FFFE
      {"\xF0\x9F\xBF\xBF", "\xF0\x9F\xBF\xBF"},  // U+01FFFF
      {"\xF0\xAF\xBF\xBE", "\xF0\xAF\xBF\xBE"},  // U+02FFFE
      {"\xF0\xAF\xBF\xBF", "\xF0\xAF\xBF\xBF"},  // U+02FFFF
      {"\xF0\xBF\xBF\xBE", "\xF0\xBF\xBF\xBE"},  // U+03FFFE
      {"\xF0\xBF\xBF\xBF", "\xF0\xBF\xBF\xBF"},  // U+03FFFF
      {"\xF1\x8F\xBF\xBE", "\xF1\x8F\xBF\xBE"},  // U+04FFFE
      {"\xF1\x8F\xBF\xBF", "\xF1\x8F\xBF\xBF"},  // U+04FFFF
      {"\xF1\x9F\xBF\xBE", "\xF1\x9F\xBF\xBE"},  // U+05FFFE
      {"\xF1\x9F\xBF\xBF", "\xF1\x9F\xBF\xBF"},  // U+05FFFF
      {"\xF1\xAF\xBF\xBE", "\xF1\xAF\xBF\xBE"},  // U+06FFFE
      {"\xF1\xAF\xBF\xBF", "\xF1\xAF\xBF\xBF"},  // U+06FFFF
      {"\xF1\xBF\xBF\xBE", "\xF1\xBF\xBF\xBE"},  // U+07FFFE
      {"\xF1\xBF\xBF\xBF", "\xF1\xBF\xBF\xBF"},  // U+07FFFF
      {"\xF2\x8F\xBF\xBE", "\xF2\x8F\xBF\xBE"},  // U+08FFFE
      {"\xF2\x8F\xBF\xBF", "\xF2\x8F\xBF\xBF"},  // U+08FFFF
      {"\xF2\x9F\xBF\xBE", "\xF2\x9F\xBF\xBE"},  // U+09FFFE
      {"\xF2\x9F\xBF\xBF", "\xF2\x9F\xBF\xBF"},  // U+09FFFF
      {"\xF2\xAF\xBF\xBE", "\xF2\xAF\xBF\xBE"},  // U+0AFFFE
      {"\xF2\xAF\xBF\xBF", "\xF2\xAF\xBF\xBF"},  // U+0AFFFF
      {"\xF2\xBF\xBF\xBE", "\xF2\xBF\xBF\xBE"},  // U+0BFFFE
      {"\xF2\xBF\xBF\xBF", "\xF2\xBF\xBF\xBF"},  // U+0BFFFF
      {"\xF3\x8F\xBF\xBE", "\xF3\x8F\xBF\xBE"},  // U+0CFFFE
      {"\xF3\x8F\xBF\xBF", "\xF3\x8F\xBF\xBF"},  // U+0CFFFF
      {"\xF3\x9F\xBF\xBE", "\xF3\x9F\xBF\xBE"},  // U+0DFFFE
      {"\xF3\x9F\xBF\xBF", "\xF3\x9F\xBF\xBF"},  // U+0DFFFF
      {"\xF3\xAF\xBF\xBE", "\xF3\xAF\xBF\xBE"},  // U+0EFFFE
      {"\xF3\xAF\xBF\xBF", "\xF3\xAF\xBF\xBF"},  // U+0EFFFF
      {"\xF3\xBF\xBF\xBE", "\xF3\xBF\xBF\xBE"},  // U+0FFFFE
      {"\xF3\xBF\xBF\xBF", "\xF3\xBF\xBF\xBF"},  // U+0FFFFF
      {"\xF4\x8F\xBF\xBE", "\xF4\x8F\xBF\xBE"},  // U+10FFFE
      {"\xF4\x8F\xBF\xBF", "\xF4\x8F\xBF\xBF"},  // U+10FFFF
  };

  for (const auto& i : cases) {
    const char* in_ptr = i.to_escape;
    std::string in_str = in_ptr;

    std::string out;
    EscapeJSONString(in_ptr, false, &out);
    EXPECT_EQ(std::string(i.escaped), out);
    EXPECT_TRUE(IsStringUTF8AllowingNoncharacters(out));

    out.erase();
    EscapeJSONString(in_str, false, &out);
    EXPECT_EQ(std::string(i.escaped), out);
    EXPECT_TRUE(IsStringUTF8AllowingNoncharacters(out));

    std::string fooout = GetQuotedJSONString(in_str);
    EXPECT_EQ("\"" + std::string(i.escaped) + "\"", fooout);
    EXPECT_TRUE(IsStringUTF8AllowingNoncharacters(out));
  }

  std::string in = cases[0].to_escape;
  std::string out;
  EscapeJSONString(in, false, &out);
  EXPECT_TRUE(IsStringUTF8AllowingNoncharacters(out));

  // test quoting
  std::string out_quoted;
  EscapeJSONString(in, true, &out_quoted);
  EXPECT_EQ(out.length() + 2, out_quoted.length());
  EXPECT_EQ(out_quoted.find(out), 1U);
  EXPECT_TRUE(IsStringUTF8AllowingNoncharacters(out_quoted));

  // now try with a NULL in the string
  std::string null_prepend = "test";
  null_prepend.push_back(0);
  in = null_prepend + in;
  std::string expected = "test\\u0000";
  expected += cases[0].escaped;
  out.clear();
  EscapeJSONString(in, false, &out);
  EXPECT_EQ(expected, out);
  EXPECT_TRUE(IsStringUTF8AllowingNoncharacters(out));
}

TEST(JSONStringEscapeTest, EscapeUTF16) {
  const struct {
    const wchar_t* to_escape;
    const char* escaped;
  } cases[] = {
      {L"b\uffb1\u00ff", "b\xEF\xBE\xB1\xC3\xBF"},
      {L"\b\001aZ\"\\wee", "\\b\\u0001aZ\\\"\\\\wee"},
      {L"a\b\f\n\r\t\v\1\\.\"z", "a\\b\\f\\n\\r\\t\\u000B\\u0001\\\\.\\\"z"},
      {L"b\x0F\x7F\xF0\xFF!", "b\\u000F\x7F\xC3\xB0\xC3\xBF!"},
      {L"c<>d", "c\\u003C>d"},
      {L"Hello\u2028world", "Hello\\u2028world"},
      {L"\u2029purple", "\\u2029purple"},
      // Unicode non-characters.
      {L"\uFDD0", "\xEF\xB7\x90"},          // U+FDD0
      {L"\uFDDF", "\xEF\xB7\x9F"},          // U+FDDF
      {L"\uFDEF", "\xEF\xB7\xAF"},          // U+FDEF
      {L"\uFFFE", "\xEF\xBF\xBE"},          // U+FFFE
      {L"\uFFFF", "\xEF\xBF\xBF"},          // U+FFFF
      {L"\U0001FFFE", "\xF0\x9F\xBF\xBE"},  // U+01FFFE
      {L"\U0001FFFF", "\xF0\x9F\xBF\xBF"},  // U+01FFFF
      {L"\U0002FFFE", "\xF0\xAF\xBF\xBE"},  // U+02FFFE
      {L"\U0002FFFF", "\xF0\xAF\xBF\xBF"},  // U+02FFFF
      {L"\U0003FFFE", "\xF0\xBF\xBF\xBE"},  // U+03FFFE
      {L"\U0003FFFF", "\xF0\xBF\xBF\xBF"},  // U+03FFFF
      {L"\U0004FFFE", "\xF1\x8F\xBF\xBE"},  // U+04FFFE
      {L"\U0004FFFF", "\xF1\x8F\xBF\xBF"},  // U+04FFFF
      {L"\U0005FFFE", "\xF1\x9F\xBF\xBE"},  // U+05FFFE
      {L"\U0005FFFF", "\xF1\x9F\xBF\xBF"},  // U+05FFFF
      {L"\U0006FFFE", "\xF1\xAF\xBF\xBE"},  // U+06FFFE
      {L"\U0006FFFF", "\xF1\xAF\xBF\xBF"},  // U+06FFFF
      {L"\U0007FFFE", "\xF1\xBF\xBF\xBE"},  // U+07FFFE
      {L"\U0007FFFF", "\xF1\xBF\xBF\xBF"},  // U+07FFFF
      {L"\U0008FFFE", "\xF2\x8F\xBF\xBE"},  // U+08FFFE
      {L"\U0008FFFF", "\xF2\x8F\xBF\xBF"},  // U+08FFFF
      {L"\U0009FFFE", "\xF2\x9F\xBF\xBE"},  // U+09FFFE
      {L"\U0009FFFF", "\xF2\x9F\xBF\xBF"},  // U+09FFFF
      {L"\U000AFFFE", "\xF2\xAF\xBF\xBE"},  // U+0AFFFE
      {L"\U000AFFFF", "\xF2\xAF\xBF\xBF"},  // U+0AFFFF
      {L"\U000BFFFE", "\xF2\xBF\xBF\xBE"},  // U+0BFFFE
      {L"\U000BFFFF", "\xF2\xBF\xBF\xBF"},  // U+0BFFFF
      {L"\U000CFFFE", "\xF3\x8F\xBF\xBE"},  // U+0CFFFE
      {L"\U000CFFFF", "\xF3\x8F\xBF\xBF"},  // U+0CFFFF
      {L"\U000DFFFE", "\xF3\x9F\xBF\xBE"},  // U+0DFFFE
      {L"\U000DFFFF", "\xF3\x9F\xBF\xBF"},  // U+0DFFFF
      {L"\U000EFFFE", "\xF3\xAF\xBF\xBE"},  // U+0EFFFE
      {L"\U000EFFFF", "\xF3\xAF\xBF\xBF"},  // U+0EFFFF
      {L"\U000FFFFE", "\xF3\xBF\xBF\xBE"},  // U+0FFFFE
      {L"\U000FFFFF", "\xF3\xBF\xBF\xBF"},  // U+0FFFFF
      {L"\U0010FFFE", "\xF4\x8F\xBF\xBE"},  // U+10FFFE
      {L"\U0010FFFF", "\xF4\x8F\xBF\xBF"},  // U+10FFFF
  };

  for (const auto& i : cases) {
    std::u16string in = WideToUTF16(i.to_escape);

    std::string out;
    EscapeJSONString(in, false, &out);
    EXPECT_EQ(std::string(i.escaped), out);
    EXPECT_TRUE(IsStringUTF8AllowingNoncharacters(out));

    out = GetQuotedJSONString(in);
    EXPECT_EQ("\"" + std::string(i.escaped) + "\"", out);
    EXPECT_TRUE(IsStringUTF8AllowingNoncharacters(out));
  }

  std::u16string in = WideToUTF16(cases[0].to_escape);
  std::string out;
  EscapeJSONString(in, false, &out);
  EXPECT_TRUE(IsStringUTF8AllowingNoncharacters(out));

  // test quoting
  std::string out_quoted;
  EscapeJSONString(in, true, &out_quoted);
  EXPECT_EQ(out.length() + 2, out_quoted.length());
  EXPECT_EQ(out_quoted.find(out), 1U);
  EXPECT_TRUE(IsStringUTF8AllowingNoncharacters(out));

  // now try with a NULL in the string
  std::u16string null_prepend = u"test";
  null_prepend.push_back(0);
  in = null_prepend + in;
  std::string expected = "test\\u0000";
  expected += cases[0].escaped;
  out.clear();
  EscapeJSONString(in, false, &out);
  EXPECT_EQ(expected, out);
  EXPECT_TRUE(IsStringUTF8AllowingNoncharacters(out));
}

TEST(JSONStringEscapeTest, EscapeUTF16OutsideBMP) {
  {
    // {a, U+10300, !}, SMP.
    std::u16string test;
    test.push_back('a');
    test.push_back(0xD800);
    test.push_back(0xDF00);
    test.push_back('!');
    std::string actual;
    EXPECT_TRUE(EscapeJSONString(test, false, &actual));
    EXPECT_EQ("a\xF0\x90\x8C\x80!", actual);
  }
  {
    // {U+20021, U+2002B}, SIP.
    std::u16string test;
    test.push_back(0xD840);
    test.push_back(0xDC21);
    test.push_back(0xD840);
    test.push_back(0xDC2B);
    std::string actual;
    EXPECT_TRUE(EscapeJSONString(test, false, &actual));
    EXPECT_EQ("\xF0\xA0\x80\xA1\xF0\xA0\x80\xAB", actual);
  }
  {
    // {?, U+D800, @}, lone surrogate.
    std::u16string test;
    test.push_back('?');
    test.push_back(0xD800);
    test.push_back('@');
    std::string actual;
    EXPECT_FALSE(EscapeJSONString(test, false, &actual));
    EXPECT_EQ("?\xEF\xBF\xBD@", actual);
  }
}

TEST(JSONStringEscapeTest, EscapeBytes) {
  const struct {
    const char* to_escape;
    const char* escaped;
  } cases[] = {
    {"b\x0f\x7f\xf0\xff!", "b\\u000F\\u007F\\u00F0\\u00FF!"},
    {"\xe5\xc4\x4f\x05\xb6\xfd", "\\u00E5\\u00C4O\\u0005\\u00B6\\u00FD"},
  };

  for (const auto& i : cases) {
    std::string in = std::string(i.to_escape);
    EXPECT_FALSE(IsStringUTF8AllowingNoncharacters(in));

    EXPECT_EQ(std::string(i.escaped),
              EscapeBytesAsInvalidJSONString(in, false));
    EXPECT_EQ("\"" + std::string(i.escaped) + "\"",
              EscapeBytesAsInvalidJSONString(in, true));
  }

  const char kEmbedNull[] = { '\xab', '\x39', '\0', '\x9f', '\xab' };
  std::string in(kEmbedNull, base::size(kEmbedNull));
  EXPECT_FALSE(IsStringUTF8AllowingNoncharacters(in));
  EXPECT_EQ(std::string("\\u00AB9\\u0000\\u009F\\u00AB"),
            EscapeBytesAsInvalidJSONString(in, false));
}

}  // namespace base
