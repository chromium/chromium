// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/strings/escape.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

struct UnescapeURLCase {
  const char* input;
  UnescapeRule::Type rules;
  const char* output;
};

struct UnescapeAndDecodeCase {
  const char* input;

  // The expected output when run through UnescapeURL.
  const char* url_unescaped;

  // The expected output when run through UnescapeQuery.
  const char* query_unescaped;

  // The expected output when run through UnescapeAndDecodeURLComponent.
  const wchar_t* decoded;
};

struct AdjustOffsetCase {
  const char* input;
  size_t input_offset;
  size_t output_offset;
};

TEST(EscapeTest, DataURLWithAccentedCharacters) {
  const std::string url =
      "text/html;charset=utf-8,%3Chtml%3E%3Cbody%3ETonton,%20ton%20th%C3"
      "%A9%20t'a-t-il%20%C3%B4t%C3%A9%20ta%20toux%20";

  OffsetAdjuster::Adjustments adjustments;
  UnescapeAndDecodeUTF8URLComponentWithAdjustments(url, UnescapeRule::SPACES,
                                                   &adjustments);
}

TEST(EscapeTest, UnescapeURLComponent) {
  const UnescapeURLCase kUnescapeCases[] = {
      {"", UnescapeRule::NORMAL, ""},
      {"%2", UnescapeRule::NORMAL, "%2"},
      {"%%%%%%", UnescapeRule::NORMAL, "%%%%%%"},
      {"Don't escape anything", UnescapeRule::NORMAL, "Don't escape anything"},
      {"Invalid %escape %2", UnescapeRule::NORMAL, "Invalid %escape %2"},
      {"Some%20random text %25%2dOK", UnescapeRule::NONE,
       "Some%20random text %25%2dOK"},
      {"Some%20random text %25%2dOK", UnescapeRule::NORMAL,
       "Some%20random text %25-OK"},
      {"Some%20random text %25%E1%A6", UnescapeRule::NORMAL,
       "Some%20random text %25\xE1\xA6"},
      {"Some%20random text %25%E1%A6OK", UnescapeRule::NORMAL,
       "Some%20random text %25\xE1\xA6OK"},
      {"Some%20random text %25%E1%A6%99OK", UnescapeRule::NORMAL,
       "Some%20random text %25\xE1\xA6\x99OK"},

      // BiDi Control characters should not be unescaped.
      {"Some%20random text %25%D8%9COK", UnescapeRule::NORMAL,
       "Some%20random text %25%D8%9COK"},
      {"Some%20random text %25%E2%80%8EOK", UnescapeRule::NORMAL,
       "Some%20random text %25%E2%80%8EOK"},
      {"Some%20random text %25%E2%80%8FOK", UnescapeRule::NORMAL,
       "Some%20random text %25%E2%80%8FOK"},
      {"Some%20random text %25%E2%80%AAOK", UnescapeRule::NORMAL,
       "Some%20random text %25%E2%80%AAOK"},
      {"Some%20random text %25%E2%80%ABOK", UnescapeRule::NORMAL,
       "Some%20random text %25%E2%80%ABOK"},
      {"Some%20random text %25%E2%80%AEOK", UnescapeRule::NORMAL,
       "Some%20random text %25%E2%80%AEOK"},
      {"Some%20random text %25%E2%81%A6OK", UnescapeRule::NORMAL,
       "Some%20random text %25%E2%81%A6OK"},
      {"Some%20random text %25%E2%81%A9OK", UnescapeRule::NORMAL,
       "Some%20random text %25%E2%81%A9OK"},

      // Certain banned characters should not be unescaped.
      // U+1F50F LOCK WITH INK PEN
      {"Some%20random text %25%F0%9F%94%8FOK", UnescapeRule::NORMAL,
       "Some%20random text %25%F0%9F%94%8FOK"},
      // U+1F510 CLOSED LOCK WITH KEY
      {"Some%20random text %25%F0%9F%94%90OK", UnescapeRule::NORMAL,
       "Some%20random text %25%F0%9F%94%90OK"},
      // U+1F512 LOCK
      {"Some%20random text %25%F0%9F%94%92OK", UnescapeRule::NORMAL,
       "Some%20random text %25%F0%9F%94%92OK"},
      // U+1F513 OPEN LOCK
      {"Some%20random text %25%F0%9F%94%93OK", UnescapeRule::NORMAL,
       "Some%20random text %25%F0%9F%94%93OK"},

      // Spaces
      {"(%C2%85)(%C2%A0)(%E1%9A%80)(%E2%80%80)", UnescapeRule::NORMAL,
       "(%C2%85)(%C2%A0)(%E1%9A%80)(%E2%80%80)"},
      {"(%E2%80%81)(%E2%80%82)(%E2%80%83)(%E2%80%84)", UnescapeRule::NORMAL,
       "(%E2%80%81)(%E2%80%82)(%E2%80%83)(%E2%80%84)"},
      {"(%E2%80%85)(%E2%80%86)(%E2%80%87)(%E2%80%88)", UnescapeRule::NORMAL,
       "(%E2%80%85)(%E2%80%86)(%E2%80%87)(%E2%80%88)"},
      {"(%E2%80%89)(%E2%80%8A)(%E2%80%A8)(%E2%80%A9)", UnescapeRule::NORMAL,
       "(%E2%80%89)(%E2%80%8A)(%E2%80%A8)(%E2%80%A9)"},
      {"(%E2%80%AF)(%E2%81%9F)(%E3%80%80)", UnescapeRule::NORMAL,
       "(%E2%80%AF)(%E2%81%9F)(%E3%80%80)"},
      {"(%E2%A0%80)", UnescapeRule::NORMAL, "(%E2%A0%80)"},

      // Default Ignorable and Formatting characters should not be unescaped.
      {"(%E2%81%A5)(%EF%BF%B0)(%EF%BF%B8)", UnescapeRule::NORMAL,
       "(%E2%81%A5)(%EF%BF%B0)(%EF%BF%B8)"},
      {"(%F3%A0%82%80)(%F3%A0%83%BF)(%F3%A0%87%B0)", UnescapeRule::NORMAL,
       "(%F3%A0%82%80)(%F3%A0%83%BF)(%F3%A0%87%B0)"},
      {"(%F3%A0%BF%BF)(%C2%AD)(%CD%8F)", UnescapeRule::NORMAL,
       "(%F3%A0%BF%BF)(%C2%AD)(%CD%8F)"},
      {"(%D8%80%20)(%D8%85)(%DB%9D)(%DC%8F)(%E0%A3%A2)", UnescapeRule::NORMAL,
       "(%D8%80%20)(%D8%85)(%DB%9D)(%DC%8F)(%E0%A3%A2)"},
      {"(%E1%85%9F)(%E1%85%A0)(%E1%9E%B4)(%E1%9E%B5)", UnescapeRule::NORMAL,
       "(%E1%85%9F)(%E1%85%A0)(%E1%9E%B4)(%E1%9E%B5)"},
      {"(%E1%A0%8B)(%E1%A0%8C)(%E1%A0%8D)(%E1%A0%8E)", UnescapeRule::NORMAL,
       "(%E1%A0%8B)(%E1%A0%8C)(%E1%A0%8D)(%E1%A0%8E)"},
      {"(%E2%80%8B)(%E2%80%8C)(%E2%80%8D)(%E2%81%A0)", UnescapeRule::NORMAL,
       "(%E2%80%8B)(%E2%80%8C)(%E2%80%8D)(%E2%81%A0)"},
      {"(%E2%81%A1)(%E2%81%A2)(%E2%81%A3)(%E2%81%A4)", UnescapeRule::NORMAL,
       "(%E2%81%A1)(%E2%81%A2)(%E2%81%A3)(%E2%81%A4)"},
      {"(%E3%85%A4)(%EF%BB%BF)(%EF%BE%A0)(%EF%BF%B9)", UnescapeRule::NORMAL,
       "(%E3%85%A4)(%EF%BB%BF)(%EF%BE%A0)(%EF%BF%B9)"},
      {"(%EF%BF%BB)(%F0%91%82%BD)(%F0%91%83%8D)", UnescapeRule::NORMAL,
       "(%EF%BF%BB)(%F0%91%82%BD)(%F0%91%83%8D)"},
      {"(%F0%93%90%B0)(%F0%93%90%B8)", UnescapeRule::NORMAL,
       "(%F0%93%90%B0)(%F0%93%90%B8)"},
      // General Punctuation - Deprecated (U+206A--206F)
      {"(%E2%81%AA)(%E2%81%AD)(%E2%81%AF)", UnescapeRule::NORMAL,
       "(%E2%81%AA)(%E2%81%AD)(%E2%81%AF)"},
      // Variation selectors (U+FE00--FE0F)
      {"(%EF%B8%80)(%EF%B8%8C)(%EF%B8%8D)", UnescapeRule::NORMAL,
       "(%EF%B8%80)(%EF%B8%8C)(%EF%B8%8D)"},
      // Shorthand format controls (U+1BCA0--1BCA3)
      {"(%F0%9B%B2%A0)(%F0%9B%B2%A1)(%F0%9B%B2%A3)", UnescapeRule::NORMAL,
       "(%F0%9B%B2%A0)(%F0%9B%B2%A1)(%F0%9B%B2%A3)"},
      // Musical symbols beams and slurs (U+1D173--1D17A)
      {"(%F0%9D%85%B3)(%F0%9D%85%B9)(%F0%9D%85%BA)", UnescapeRule::NORMAL,
       "(%F0%9D%85%B3)(%F0%9D%85%B9)(%F0%9D%85%BA)"},
      // Tags block (U+E0000--E007F), includes unassigned points
      {"(%F3%A0%80%80)(%F3%A0%80%81)(%F3%A0%81%8F)", UnescapeRule::NORMAL,
       "(%F3%A0%80%80)(%F3%A0%80%81)(%F3%A0%81%8F)"},
      // Ideographic-specific variation selectors (U+E0100--E01EF)
      {"(%F3%A0%84%80)(%F3%A0%84%90)(%F3%A0%87%AF)", UnescapeRule::NORMAL,
       "(%F3%A0%84%80)(%F3%A0%84%90)(%F3%A0%87%AF)"},

      // Two spoofing characters in a row should not be unescaped.
      {"%D8%9C%D8%9C", UnescapeRule::NORMAL, "%D8%9C%D8%9C"},
      // Non-spoofing characters surrounded by spoofing characters should be
      // unescaped.
      {"%D8%9C%C2%A1%D8%9C%C2%A1", UnescapeRule::NORMAL,
       "%D8%9C\xC2\xA1%D8%9C\xC2\xA1"},
      // Invalid UTF-8 characters surrounded by spoofing characters should be
      // unescaped.
      {"%D8%9C%85%D8%9C%85", UnescapeRule::NORMAL, "%D8%9C\x85%D8%9C\x85"},
      // Test with enough trail bytes to overflow the CBU8_MAX_LENGTH-byte
      // buffer. The first two bytes are a spoofing character as well.
      {"%D8%9C%9C%9C%9C%9C%9C%9C%9C%9C%9C", UnescapeRule::NORMAL,
       "%D8%9C\x9C\x9C\x9C\x9C\x9C\x9C\x9C\x9C\x9C"},

      {"Some%20random text %25%2dOK", UnescapeRule::SPACES,
       "Some random text %25-OK"},
      {"Some%20random text %25%2dOK", UnescapeRule::PATH_SEPARATORS,
       "Some%20random text %25-OK"},
      {"Some%20random text %25%2dOK",
       UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS,
       "Some%20random text %-OK"},
      {"Some%20random text %25%2dOK",
       UnescapeRule::SPACES |
           UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS,
       "Some random text %-OK"},
      {"%A0%B1%C2%D3%E4%F5", UnescapeRule::NORMAL, "\xA0\xB1\xC2\xD3\xE4\xF5"},
      {"%Aa%Bb%Cc%Dd%Ee%Ff", UnescapeRule::NORMAL, "\xAa\xBb\xCc\xDd\xEe\xFf"},
      // Certain URL-sensitive characters should not be unescaped unless asked.
      {"Hello%20%13%10world %23# %3F? %3D= %26& %25% %2B+",
       UnescapeRule::SPACES, "Hello %13%10world %23# %3F? %3D= %26& %25% %2B+"},
      {"Hello%20%13%10world %23# %3F? %3D= %26& %25% %2B+",
       UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS,
       "Hello%20%13%10world ## ?? == && %% ++"},
      // We can neither escape nor unescape '@' since some websites expect it to
      // be preserved as either '@' or "%40".
      // See http://b/996720 and http://crbug.com/23933 .
      {"me@my%40example", UnescapeRule::NORMAL, "me@my%40example"},
      // Control characters.
      {"%01%02%03%04%05%06%07%08%09 %25",
       UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS,
       "%01%02%03%04%05%06%07%08%09 %"},
      {"Hello%20%13%10%02", UnescapeRule::SPACES, "Hello %13%10%02"},

      // '/' and '\\' should only be unescaped by PATH_SEPARATORS.
      {"%2F%5C", UnescapeRule::PATH_SEPARATORS, "/\\"},
  };

  for (const auto unescape_case : kUnescapeCases) {
    EXPECT_EQ(unescape_case.output,
              UnescapeURLComponent(unescape_case.input, unescape_case.rules));
  }

  // Test NULL character unescaping, which can't be tested above since those are
  // just char pointers.
  std::string input("Null");
  input.push_back(0);  // Also have a NULL in the input.
  input.append("%00%39Test");

  std::string expected = "Null";
  expected.push_back(0);
  expected.append("%009Test");
  EXPECT_EQ(expected, UnescapeURLComponent(input, UnescapeRule::NORMAL));
}

TEST(EscapeTest, UnescapeAndDecodeUTF8URLComponentWithAdjustments) {
  const UnescapeAndDecodeCase unescape_cases[] = {
      {"%", "%", "%", L"%"},
      {"+", "+", " ", L"+"},
      {"%2+", "%2+", "%2 ", L"%2+"},
      {"+%%%+%%%", "+%%%+%%%", " %%% %%%", L"+%%%+%%%"},
      {"Don't escape anything", "Don't escape anything",
       "Don't escape anything", L"Don't escape anything"},
      {"+Invalid %escape %2+", "+Invalid %escape %2+", " Invalid %escape %2 ",
       L"+Invalid %escape %2+"},
      {"Some random text %25%2dOK", "Some random text %25-OK",
       "Some random text %25-OK", L"Some random text %25-OK"},
      {"%01%02%03%04%05%06%07%08%09", "%01%02%03%04%05%06%07%08%09",
       "%01%02%03%04%05%06%07%08%09", L"%01%02%03%04%05%06%07%08%09"},
      {"%E4%BD%A0+%E5%A5%BD", "\xE4\xBD\xA0+\xE5\xA5\xBD",
       "\xE4\xBD\xA0 \xE5\xA5\xBD", L"\x4f60+\x597d"},
      {"%ED%ED",                            // Invalid UTF-8.
       "\xED\xED", "\xED\xED", L"%ED%ED"},  // Invalid UTF-8 -> kept unescaped.
  };

  for (const auto& unescape_case : unescape_cases) {
    std::string unescaped =
        UnescapeURLComponent(unescape_case.input, UnescapeRule::NORMAL);
    EXPECT_EQ(std::string(unescape_case.url_unescaped), unescaped);

    unescaped = UnescapeURLComponent(unescape_case.input,
                                     UnescapeRule::REPLACE_PLUS_WITH_SPACE);
    EXPECT_EQ(std::string(unescape_case.query_unescaped), unescaped);

    // The adjustments argument is covered by the next test.
    //
    // TODO: Need to test unescape_spaces and unescape_percent.
    std::u16string decoded = UnescapeAndDecodeUTF8URLComponentWithAdjustments(
        unescape_case.input, UnescapeRule::NORMAL, nullptr);
    EXPECT_EQ(WideToUTF16(unescape_case.decoded), decoded);
  }
}

TEST(EscapeTest, AdjustOffset) {
  const AdjustOffsetCase adjust_cases[] = {
      {"", 0, 0},
      {"test", 0, 0},
      {"test", 2, 2},
      {"test", 4, 4},
      {"test", std::string::npos, std::string::npos},
      {"%2dtest", 6, 4},
      {"%2dtest", 3, 1},
      {"%2dtest", 2, std::string::npos},
      {"%2dtest", 1, std::string::npos},
      {"%2dtest", 0, 0},
      {"test%2d", 2, 2},
      {"test%2e", 2, 2},
      {"%E4%BD%A0+%E5%A5%BD", 9, 1},
      {"%E4%BD%A0+%E5%A5%BD", 6, std::string::npos},
      {"%E4%BD%A0+%E5%A5%BD", 0, 0},
      {"%E4%BD%A0+%E5%A5%BD", 10, 2},
      {"%E4%BD%A0+%E5%A5%BD", 19, 3},

      {"hi%41test%E4%BD%A0+%E5%A5%BD", 18, 8},
      {"hi%41test%E4%BD%A0+%E5%A5%BD", 15, std::string::npos},
      {"hi%41test%E4%BD%A0+%E5%A5%BD", 9, 7},
      {"hi%41test%E4%BD%A0+%E5%A5%BD", 19, 9},
      {"hi%41test%E4%BD%A0+%E5%A5%BD", 28, 10},
      {"hi%41test%E4%BD%A0+%E5%A5%BD", 0, 0},
      {"hi%41test%E4%BD%A0+%E5%A5%BD", 2, 2},
      {"hi%41test%E4%BD%A0+%E5%A5%BD", 3, std::string::npos},
      {"hi%41test%E4%BD%A0+%E5%A5%BD", 5, 3},

      {"%E4%BD%A0+%E5%A5%BDhi%41test", 9, 1},
      {"%E4%BD%A0+%E5%A5%BDhi%41test", 6, std::string::npos},
      {"%E4%BD%A0+%E5%A5%BDhi%41test", 0, 0},
      {"%E4%BD%A0+%E5%A5%BDhi%41test", 10, 2},
      {"%E4%BD%A0+%E5%A5%BDhi%41test", 19, 3},
      {"%E4%BD%A0+%E5%A5%BDhi%41test", 21, 5},
      {"%E4%BD%A0+%E5%A5%BDhi%41test", 22, std::string::npos},
      {"%E4%BD%A0+%E5%A5%BDhi%41test", 24, 6},
      {"%E4%BD%A0+%E5%A5%BDhi%41test", 28, 10},

      {"%ED%B0%80+%E5%A5%BD", 6, 6},  // not convertible to UTF-8
  };

  for (const auto& adjust_case : adjust_cases) {
    size_t offset = adjust_case.input_offset;
    OffsetAdjuster::Adjustments adjustments;
    UnescapeAndDecodeUTF8URLComponentWithAdjustments(
        adjust_case.input, UnescapeRule::NORMAL, &adjustments);
    OffsetAdjuster::AdjustOffset(adjustments, &offset);
    EXPECT_EQ(adjust_case.output_offset, offset)
        << "input=" << adjust_case.input
        << " offset=" << adjust_case.input_offset;
  }
}

TEST(EscapeTest, UnescapeBinaryURLComponent) {
  const UnescapeURLCase kTestCases[] = {
      // Check that ASCII characters with special handling in
      // UnescapeURLComponent() are still unescaped.
      {"%09%20%25foo%2F", UnescapeRule::NORMAL, "\x09 %foo/"},

      // UTF-8 Characters banned by UnescapeURLComponent() should also be
      // unescaped.
      {"Some random text %D8%9COK", UnescapeRule::NORMAL,
       "Some random text \xD8\x9COK"},
      {"Some random text %F0%9F%94%8FOK", UnescapeRule::NORMAL,
       "Some random text \xF0\x9F\x94\x8FOK"},

      // As should invalid UTF-8 characters.
      {"%A0%A0%E9%E9%A0%A0%A0%A0", UnescapeRule::NORMAL,
       "\xA0\xA0\xE9\xE9\xA0\xA0\xA0\xA0"},

      // And valid UTF-8 characters that are not banned by
      // UnescapeURLComponent() should be unescaped, too!
      {"%C2%A1%C2%A1", UnescapeRule::NORMAL, "\xC2\xA1\xC2\xA1"},

      // '+' should be left alone by default
      {"++%2B++", UnescapeRule::NORMAL, "+++++"},
      // But should magically be turned into a space if requested.
      {"++%2B++", UnescapeRule::REPLACE_PLUS_WITH_SPACE, "  +  "},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(test_case.output,
              UnescapeBinaryURLComponent(test_case.input, test_case.rules));
  }

  // Test NULL character unescaping, which can't be tested above since those are
  // just char pointers.
  std::string input("Null");
  input.push_back(0);  // Also have a NULL in the input.
  input.append("%00%39Test");

  std::string expected("Null");
  expected.push_back(0);
  expected.push_back(0);
  expected.append("9Test");
  EXPECT_EQ(expected, UnescapeBinaryURLComponent(input));
}

TEST(EscapeTest, UnescapeBinaryURLComponentSafe) {
  const struct TestCase {
    const char* input;
    // Expected output. Null if call is expected to fail when
    // |fail_on_path_separators| is false.
    const char* expected_output;
    // Whether |input| has any escaped path separators.
    bool has_path_separators;
  } kTestCases[] = {
      // Spaces, percents, and invalid UTF-8 characters are all successfully
      // unescaped.
      {"%20%25foo%81", " %foo\x81", false},

      // Characters disallowed unconditionally.
      {"foo%00", nullptr, false},
      {"foo%01", nullptr, false},
      {"foo%0A", nullptr, false},
      {"foo%0D", nullptr, false},

      // Path separators.
      {"foo%2F", "foo/", true},
      {"foo%5C", "foo\\", true},

      // Characters that are considered invalid to escape are ignored if passed
      // in unescaped.
      {"foo\x01\r/\\", "foo\x01\r/\\", false},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.input);

    std::string output = "foo";
    if (!test_case.expected_output) {
      EXPECT_FALSE(UnescapeBinaryURLComponentSafe(
          test_case.input, false /* fail_on_path_separators */, &output));
      EXPECT_TRUE(output.empty());
      EXPECT_FALSE(UnescapeBinaryURLComponentSafe(
          test_case.input, true /* fail_on_path_separators */, &output));
      EXPECT_TRUE(output.empty());
      continue;
    }
    EXPECT_TRUE(UnescapeBinaryURLComponentSafe(
        test_case.input, false /* fail_on_path_separators */, &output));
    EXPECT_EQ(test_case.expected_output, output);
    if (test_case.has_path_separators) {
      EXPECT_FALSE(UnescapeBinaryURLComponentSafe(
          test_case.input, true /* fail_on_path_separators */, &output));
      EXPECT_TRUE(output.empty());
    } else {
      output = "foo";
      EXPECT_TRUE(UnescapeBinaryURLComponentSafe(
          test_case.input, true /* fail_on_path_separators */, &output));
      EXPECT_EQ(test_case.expected_output, output);
    }
  }
}

TEST(EscapeTest, ContainsEncodedBytes) {
  EXPECT_FALSE(ContainsEncodedBytes("abc/def", {'/', '\\'}));
  EXPECT_FALSE(ContainsEncodedBytes("abc%2Fdef", {'%'}));
  EXPECT_TRUE(ContainsEncodedBytes("abc%252Fdef", {'%'}));
  EXPECT_TRUE(ContainsEncodedBytes("abc%2Fdef", {'/', '\\'}));
  EXPECT_TRUE(ContainsEncodedBytes("abc%5Cdef", {'/', '\\'}));
  EXPECT_TRUE(ContainsEncodedBytes("abc%2fdef", {'/', '\\'}));

  // Should be looking for byte values, not UTF-8 character values.
  EXPECT_TRUE(
      ContainsEncodedBytes("caf%C3%A9", {static_cast<uint8_t>('\xc3')}));
  EXPECT_FALSE(
      ContainsEncodedBytes("caf%C3%A9", {static_cast<uint8_t>('\xe9')}));
}

}  // namespace base
