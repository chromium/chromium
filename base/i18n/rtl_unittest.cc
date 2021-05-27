// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/rtl.h"

#include <stddef.h>

#include <algorithm>

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/i18n/unicode/usearch.h"

namespace base {
namespace i18n {

class RTLTest : public PlatformTest {
};

TEST_F(RTLTest, GetFirstStrongCharacterDirection) {
  struct {
    const wchar_t* text;
    TextDirection direction;
  } cases[] = {
    // Test pure LTR string.
    { L"foo bar", LEFT_TO_RIGHT },
    // Test pure RTL string.
    { L"\x05d0\x05d1\x05d2 \x05d3\x0d4\x05d5", RIGHT_TO_LEFT},
    // Test bidi string in which the first character with strong directionality
    // is a character with type L.
    { L"foo \x05d0 bar", LEFT_TO_RIGHT },
    // Test bidi string in which the first character with strong directionality
    // is a character with type R.
    { L"\x05d0 foo bar", RIGHT_TO_LEFT },
    // Test bidi string which starts with a character with weak directionality
    // and in which the first character with strong directionality is a
    // character with type L.
    { L"!foo \x05d0 bar", LEFT_TO_RIGHT },
    // Test bidi string which starts with a character with weak directionality
    // and in which the first character with strong directionality is a
    // character with type R.
    { L",\x05d0 foo bar", RIGHT_TO_LEFT },
    // Test bidi string in which the first character with strong directionality
    // is a character with type LRE.
    { L"\x202a \x05d0 foo  bar", LEFT_TO_RIGHT },
    // Test bidi string in which the first character with strong directionality
    // is a character with type LRO.
    { L"\x202d \x05d0 foo  bar", LEFT_TO_RIGHT },
    // Test bidi string in which the first character with strong directionality
    // is a character with type RLE.
    { L"\x202b foo \x05d0 bar", RIGHT_TO_LEFT },
    // Test bidi string in which the first character with strong directionality
    // is a character with type RLO.
    { L"\x202e foo \x05d0 bar", RIGHT_TO_LEFT },
    // Test bidi string in which the first character with strong directionality
    // is a character with type AL.
    { L"\x0622 foo \x05d0 bar", RIGHT_TO_LEFT },
    // Test a string without strong directionality characters.
    { L",!.{}", LEFT_TO_RIGHT },
    // Test empty string.
    { L"", LEFT_TO_RIGHT },
    // Test characters in non-BMP (e.g. Phoenician letters. Please refer to
    // http://demo.icu-project.org/icu-bin/ubrowse?scr=151&b=10910 for more
    // information).
    {
#if defined(WCHAR_T_IS_UTF32)
      L" ! \x10910" L"abc 123",
#elif defined(WCHAR_T_IS_UTF16)
      L" ! \xd802\xdd10" L"abc 123",
#else
#error wchar_t should be either UTF-16 or UTF-32
#endif
      RIGHT_TO_LEFT },
    {
#if defined(WCHAR_T_IS_UTF32)
      L" ! \x10401" L"abc 123",
#elif defined(WCHAR_T_IS_UTF16)
      L" ! \xd801\xdc01" L"abc 123",
#else
#error wchar_t should be either UTF-16 or UTF-32
#endif
      LEFT_TO_RIGHT },
   };

  for (auto& i : cases)
    EXPECT_EQ(i.direction,
              GetFirstStrongCharacterDirection(WideToUTF16(i.text)));
}


// Note that the cases with LRE, LRO, RLE and RLO are invalid for
// GetLastStrongCharacterDirection because they should be followed by PDF
// character.
TEST_F(RTLTest, GetLastStrongCharacterDirection) {
  struct {
    const wchar_t* text;
    TextDirection direction;
  } cases[] = {
    // Test pure LTR string.
    { L"foo bar", LEFT_TO_RIGHT },
    // Test pure RTL string.
    { L"\x05d0\x05d1\x05d2 \x05d3\x0d4\x05d5", RIGHT_TO_LEFT},
    // Test bidi string in which the last character with strong directionality
    // is a character with type L.
    { L"foo \x05d0 bar", LEFT_TO_RIGHT },
    // Test bidi string in which the last character with strong directionality
    // is a character with type R.
    { L"\x05d0 foo bar \x05d3", RIGHT_TO_LEFT },
    // Test bidi string which ends with a character with weak directionality
    // and in which the last character with strong directionality is a
    // character with type L.
    { L"!foo \x05d0 bar!", LEFT_TO_RIGHT },
    // Test bidi string which ends with a character with weak directionality
    // and in which the last character with strong directionality is a
    // character with type R.
    { L",\x05d0 foo bar \x05d1,", RIGHT_TO_LEFT },
    // Test bidi string in which the last character with strong directionality
    // is a character with type AL.
    { L"\x0622 foo \x05d0 bar \x0622", RIGHT_TO_LEFT },
    // Test a string without strong directionality characters.
    { L",!.{}", LEFT_TO_RIGHT },
    // Test empty string.
    { L"", LEFT_TO_RIGHT },
    // Test characters in non-BMP (e.g. Phoenician letters. Please refer to
    // http://demo.icu-project.org/icu-bin/ubrowse?scr=151&b=10910 for more
    // information).
    {
#if defined(WCHAR_T_IS_UTF32)
       L"abc 123" L" ! \x10910 !",
#elif defined(WCHAR_T_IS_UTF16)
       L"abc 123" L" ! \xd802\xdd10 !",
#else
#error wchar_t should be either UTF-16 or UTF-32
#endif
      RIGHT_TO_LEFT },
    {
#if defined(WCHAR_T_IS_UTF32)
       L"abc 123" L" ! \x10401 !",
#elif defined(WCHAR_T_IS_UTF16)
       L"abc 123" L" ! \xd801\xdc01 !",
#else
#error wchar_t should be either UTF-16 or UTF-32
#endif
      LEFT_TO_RIGHT },
   };

  for (auto& i : cases)
    EXPECT_EQ(i.direction,
              GetLastStrongCharacterDirection(WideToUTF16(i.text)));
}

TEST_F(RTLTest, GetStringDirection) {
  struct {
    const wchar_t* text;
    TextDirection direction;
  } cases[] = {
    // Test pure LTR string.
    { L"foobar", LEFT_TO_RIGHT },
    { L".foobar", LEFT_TO_RIGHT },
    { L"foo, bar", LEFT_TO_RIGHT },
    // Test pure LTR with strong directionality characters of type LRE.
    { L"\x202a\x202a", LEFT_TO_RIGHT },
    { L".\x202a\x202a", LEFT_TO_RIGHT },
    { L"\x202a, \x202a", LEFT_TO_RIGHT },
    // Test pure LTR with strong directionality characters of type LRO.
    { L"\x202d\x202d", LEFT_TO_RIGHT },
    { L".\x202d\x202d", LEFT_TO_RIGHT },
    { L"\x202d, \x202d", LEFT_TO_RIGHT },
    // Test pure LTR with various types of strong directionality characters.
    { L"foo \x202a\x202d", LEFT_TO_RIGHT },
    { L".\x202d foo \x202a", LEFT_TO_RIGHT },
    { L"\x202a, \x202d foo", LEFT_TO_RIGHT },
    // Test pure RTL with strong directionality characters of type R.
    { L"\x05d0\x05d0", RIGHT_TO_LEFT },
    { L".\x05d0\x05d0", RIGHT_TO_LEFT },
    { L"\x05d0, \x05d0", RIGHT_TO_LEFT },
    // Test pure RTL with strong directionality characters of type RLE.
    { L"\x202b\x202b", RIGHT_TO_LEFT },
    { L".\x202b\x202b", RIGHT_TO_LEFT },
    { L"\x202b, \x202b", RIGHT_TO_LEFT },
    // Test pure RTL with strong directionality characters of type RLO.
    { L"\x202e\x202e", RIGHT_TO_LEFT },
    { L".\x202e\x202e", RIGHT_TO_LEFT },
    { L"\x202e, \x202e", RIGHT_TO_LEFT },
    // Test pure RTL with strong directionality characters of type AL.
    { L"\x0622\x0622", RIGHT_TO_LEFT },
    { L".\x0622\x0622", RIGHT_TO_LEFT },
    { L"\x0622, \x0622", RIGHT_TO_LEFT },
    // Test pure RTL with various types of strong directionality characters.
    { L"\x05d0\x202b\x202e\x0622", RIGHT_TO_LEFT },
    { L".\x202b\x202e\x0622\x05d0", RIGHT_TO_LEFT },
    { L"\x0622\x202e, \x202b\x05d0", RIGHT_TO_LEFT },
    // Test bidi strings.
    { L"foo \x05d0 bar", UNKNOWN_DIRECTION },
    { L"\x202b foo bar", UNKNOWN_DIRECTION },
    { L"!foo \x0622 bar", UNKNOWN_DIRECTION },
    { L"\x202a\x202b", UNKNOWN_DIRECTION },
    { L"\x202e\x202d", UNKNOWN_DIRECTION },
    { L"\x0622\x202a", UNKNOWN_DIRECTION },
    { L"\x202d\x05d0", UNKNOWN_DIRECTION },
    // Test a string without strong directionality characters.
    { L",!.{}", LEFT_TO_RIGHT },
    // Test empty string.
    { L"", LEFT_TO_RIGHT },
    {
#if defined(WCHAR_T_IS_UTF32)
      L" ! \x10910" L"abc 123",
#elif defined(WCHAR_T_IS_UTF16)
      L" ! \xd802\xdd10" L"abc 123",
#else
#error wchar_t should be either UTF-16 or UTF-32
#endif
      UNKNOWN_DIRECTION },
    {
#if defined(WCHAR_T_IS_UTF32)
      L" ! \x10401" L"abc 123",
#elif defined(WCHAR_T_IS_UTF16)
      L" ! \xd801\xdc01" L"abc 123",
#else
#error wchar_t should be either UTF-16 or UTF-32
#endif
      LEFT_TO_RIGHT },
   };

  for (auto& i : cases)
    EXPECT_EQ(i.direction, GetStringDirection(WideToUTF16(i.text)));
}

TEST_F(RTLTest, WrapPathWithLTRFormatting) {
  const wchar_t* cases[] = {
    // Test common path, such as "c:\foo\bar".
    L"c:/foo/bar",
    // Test path with file name, such as "c:\foo\bar\test.jpg".
    L"c:/foo/bar/test.jpg",
    // Test path ending with punctuation, such as "c:\(foo)\bar.".
    L"c:/(foo)/bar.",
    // Test path ending with separator, such as "c:\foo\bar\".
    L"c:/foo/bar/",
    // Test path with RTL character.
    L"c:/\x05d0",
    // Test path with 2 level RTL directory names.
    L"c:/\x05d0/\x0622",
    // Test path with mixed RTL/LTR directory names and ending with punctuation.
    L"c:/\x05d0/\x0622/(foo)/b.a.r.",
    // Test path without driver name, such as "/foo/bar/test/jpg".
    L"/foo/bar/test.jpg",
    // Test path start with current directory, such as "./foo".
    L"./foo",
    // Test path start with parent directory, such as "../foo/bar.jpg".
    L"../foo/bar.jpg",
    // Test absolute path, such as "//foo/bar.jpg".
    L"//foo/bar.jpg",
    // Test path with mixed RTL/LTR directory names.
    L"c:/foo/\x05d0/\x0622/\x05d1.jpg",
    // Test empty path.
    L""
  };

  for (auto*& i : cases) {
    FilePath path;
#if defined(OS_WIN)
    std::wstring win_path(i);
    std::replace(win_path.begin(), win_path.end(), '/', '\\');
    path = FilePath(win_path);
    std::wstring wrapped_expected =
        std::wstring(L"\x202a") + win_path + L"\x202c";
#else
    path = FilePath(base::SysWideToNativeMB(i));
    std::wstring wrapped_expected = std::wstring(L"\x202a") + i + L"\x202c";
#endif
    std::u16string localized_file_path_string;
    WrapPathWithLTRFormatting(path, &localized_file_path_string);

    std::wstring wrapped_actual = UTF16ToWide(localized_file_path_string);
    EXPECT_EQ(wrapped_expected, wrapped_actual);
  }
}

TEST_F(RTLTest, WrapString) {
  const wchar_t* cases[] = {
    L" . ",
    L"abc",
    L"a" L"\x5d0\x5d1",
    L"a" L"\x5d1" L"b",
    L"\x5d0\x5d1\x5d2",
    L"\x5d0\x5d1" L"a",
    L"\x5d0" L"a" L"\x5d1",
  };

  const bool was_rtl = IsRTL();

  test::ScopedRestoreICUDefaultLocale restore_locale;
  for (size_t i = 0; i < 2; ++i) {
    // Toggle the application default text direction (to try each direction).
    SetRTLForTesting(!IsRTL());

    std::u16string empty;
    WrapStringWithLTRFormatting(&empty);
    EXPECT_TRUE(empty.empty());
    WrapStringWithRTLFormatting(&empty);
    EXPECT_TRUE(empty.empty());

    for (auto*& test_case : cases) {
      std::u16string input = WideToUTF16(test_case);
      std::u16string ltr_wrap = input;
      WrapStringWithLTRFormatting(&ltr_wrap);
      EXPECT_EQ(ltr_wrap[0], kLeftToRightEmbeddingMark);
      EXPECT_EQ(ltr_wrap.substr(1, ltr_wrap.length() - 2), input);
      EXPECT_EQ(ltr_wrap[ltr_wrap.length() -1], kPopDirectionalFormatting);

      std::u16string rtl_wrap = input;
      WrapStringWithRTLFormatting(&rtl_wrap);
      EXPECT_EQ(rtl_wrap[0], kRightToLeftEmbeddingMark);
      EXPECT_EQ(rtl_wrap.substr(1, rtl_wrap.length() - 2), input);
      EXPECT_EQ(rtl_wrap[rtl_wrap.length() -1], kPopDirectionalFormatting);
    }
  }

  EXPECT_EQ(was_rtl, IsRTL());
}

TEST_F(RTLTest, GetDisplayStringInLTRDirectionality) {
  struct {
    const wchar_t* path;
    bool wrap_ltr;
    bool wrap_rtl;
  } cases[] = {
    { L"test",                   false, true },
    { L"test.html",              false, true },
    { L"\x05d0\x05d1\x05d2",     true,  true },
    { L"\x05d0\x05d1\x05d2.txt", true,  true },
    { L"\x05d0" L"abc",          true,  true },
    { L"\x05d0" L"abc.txt",      true,  true },
    { L"abc\x05d0\x05d1",        false, true },
    { L"abc\x05d0\x05d1.jpg",    false, true },
  };

  const bool was_rtl = IsRTL();

  test::ScopedRestoreICUDefaultLocale restore_locale;
  for (size_t i = 0; i < 2; ++i) {
    // Toggle the application default text direction (to try each direction).
    SetRTLForTesting(!IsRTL());
    for (auto& test_case : cases) {
      std::u16string input = WideToUTF16(test_case.path);
      std::u16string output = GetDisplayStringInLTRDirectionality(input);
      // Test the expected wrapping behavior for the current UI directionality.
      if (IsRTL() ? test_case.wrap_rtl : test_case.wrap_ltr)
        EXPECT_NE(output, input);
      else
        EXPECT_EQ(output, input);
    }
  }

  EXPECT_EQ(was_rtl, IsRTL());
}

TEST_F(RTLTest, GetTextDirection) {
  EXPECT_EQ(RIGHT_TO_LEFT, GetTextDirectionForLocale("ar"));
  EXPECT_EQ(RIGHT_TO_LEFT, GetTextDirectionForLocale("ar_EG"));
  EXPECT_EQ(RIGHT_TO_LEFT, GetTextDirectionForLocale("he"));
  EXPECT_EQ(RIGHT_TO_LEFT, GetTextDirectionForLocale("he_IL"));
  // iw is an obsolete code for Hebrew.
  EXPECT_EQ(RIGHT_TO_LEFT, GetTextDirectionForLocale("iw"));
  // Although we're not yet localized to Farsi and Urdu, we
  // do have the text layout direction information for them.
  EXPECT_EQ(RIGHT_TO_LEFT, GetTextDirectionForLocale("fa"));
  EXPECT_EQ(RIGHT_TO_LEFT, GetTextDirectionForLocale("ur"));
#if 0
  // Enable these when we include the minimal locale data for Azerbaijani
  // written in Arabic and Dhivehi. At the moment, our copy of
  // ICU data does not have entries for them.
  EXPECT_EQ(RIGHT_TO_LEFT, GetTextDirectionForLocale("az_Arab"));
  // Dhivehi that uses Thaana script.
  EXPECT_EQ(RIGHT_TO_LEFT, GetTextDirectionForLocale("dv"));
#endif
  EXPECT_EQ(LEFT_TO_RIGHT, GetTextDirectionForLocale("en"));
  // Chinese in China with '-'.
  EXPECT_EQ(LEFT_TO_RIGHT, GetTextDirectionForLocale("zh-CN"));
  // Filipino : 3-letter code
  EXPECT_EQ(LEFT_TO_RIGHT, GetTextDirectionForLocale("fil"));
  // Russian
  EXPECT_EQ(LEFT_TO_RIGHT, GetTextDirectionForLocale("ru"));
  // Japanese that uses multiple scripts
  EXPECT_EQ(LEFT_TO_RIGHT, GetTextDirectionForLocale("ja"));
}

TEST_F(RTLTest, GetTextDirectionForLocaleInStartUp) {
  EXPECT_EQ(RIGHT_TO_LEFT, GetTextDirectionForLocaleInStartUp("ar"));
  EXPECT_EQ(RIGHT_TO_LEFT, GetTextDirectionForLocaleInStartUp("ar_EG"));
  EXPECT_EQ(RIGHT_TO_LEFT, GetTextDirectionForLocaleInStartUp("he"));
  EXPECT_EQ(RIGHT_TO_LEFT, GetTextDirectionForLocaleInStartUp("he_IL"));
  // iw is an obsolete code for Hebrew.
  EXPECT_EQ(RIGHT_TO_LEFT, GetTextDirectionForLocaleInStartUp("iw"));
  // Although we're not yet localized to Farsi and Urdu, we
  // do have the text layout direction information for them.
  EXPECT_EQ(RIGHT_TO_LEFT, GetTextDirectionForLocaleInStartUp("fa"));
  EXPECT_EQ(RIGHT_TO_LEFT, GetTextDirectionForLocaleInStartUp("ur"));
  EXPECT_EQ(LEFT_TO_RIGHT, GetTextDirectionForLocaleInStartUp("en"));
  // Chinese in China with '-'.
  EXPECT_EQ(LEFT_TO_RIGHT, GetTextDirectionForLocaleInStartUp("zh-CN"));
  // Filipino : 3-letter code
  EXPECT_EQ(LEFT_TO_RIGHT, GetTextDirectionForLocaleInStartUp("fil"));
  // Russian
  EXPECT_EQ(LEFT_TO_RIGHT, GetTextDirectionForLocaleInStartUp("ru"));
  // Japanese that uses multiple scripts
  EXPECT_EQ(LEFT_TO_RIGHT, GetTextDirectionForLocaleInStartUp("ja"));
}

TEST_F(RTLTest, UnadjustStringForLocaleDirection) {
  // These test strings are borrowed from WrapPathWithLTRFormatting
  const wchar_t* cases[] = {
    L"foo bar",
    L"foo \x05d0 bar",
    L"\x05d0 foo bar",
    L"!foo \x05d0 bar",
    L",\x05d0 foo bar",
    L"\x202a \x05d0 foo  bar",
    L"\x202d \x05d0 foo  bar",
    L"\x202b foo \x05d0 bar",
    L"\x202e foo \x05d0 bar",
    L"\x0622 foo \x05d0 bar",
  };

  const bool was_rtl = IsRTL();

  test::ScopedRestoreICUDefaultLocale restore_locale;
  for (size_t i = 0; i < 2; ++i) {
    // Toggle the application default text direction (to try each direction).
    SetRTLForTesting(!IsRTL());

    for (auto*& test_case : cases) {
      std::u16string unadjusted_string = WideToUTF16(test_case);
      std::u16string adjusted_string = unadjusted_string;

      if (!AdjustStringForLocaleDirection(&adjusted_string))
        continue;

      EXPECT_NE(unadjusted_string, adjusted_string);
      EXPECT_TRUE(UnadjustStringForLocaleDirection(&adjusted_string));
      EXPECT_EQ(unadjusted_string, adjusted_string)
          << " for test case [" << unadjusted_string
          << "] with IsRTL() == " << IsRTL();
    }
  }

  EXPECT_EQ(was_rtl, IsRTL());
}

TEST_F(RTLTest, EnsureTerminatedDirectionalFormatting) {
  struct {
    const wchar_t* unformated_text;
    const wchar_t* formatted_text;
  } cases[] = {
      // Tests string without any dir-formatting characters.
      {L"google.com", L"google.com"},
      // Tests string with properly terminated dir-formatting character.
      {L"\x202egoogle.com\x202c", L"\x202egoogle.com\x202c"},
      // Tests string with over-terminated dir-formatting characters.
      {L"\x202egoogle\x202c.com\x202c", L"\x202egoogle\x202c.com\x202c"},
      // Tests string beginning with a dir-formatting character.
      {L"\x202emoc.elgoog", L"\x202emoc.elgoog\x202c"},
      // Tests string that over-terminates then re-opens.
      {L"\x202egoogle\x202c\x202c.\x202eom",
       L"\x202egoogle\x202c\x202c.\x202eom\x202c"},
      // Tests string containing a dir-formatting character in the middle.
      {L"google\x202e.com", L"google\x202e.com\x202c"},
      // Tests string with multiple dir-formatting characters.
      {L"\x202egoogle\x202e.com/\x202eguest",
       L"\x202egoogle\x202e.com/\x202eguest\x202c\x202c\x202c"},
      // Test the other dir-formatting characters (U+202A, U+202B, and U+202D).
      {L"\x202agoogle.com", L"\x202agoogle.com\x202c"},
      {L"\x202bgoogle.com", L"\x202bgoogle.com\x202c"},
      {L"\x202dgoogle.com", L"\x202dgoogle.com\x202c"},
  };

  const bool was_rtl = IsRTL();

  test::ScopedRestoreICUDefaultLocale restore_locale;
  for (size_t i = 0; i < 2; ++i) {
    // Toggle the application default text direction (to try each direction).
    SetRTLForTesting(!IsRTL());
    for (auto& test_case : cases) {
      std::u16string unsanitized_text = WideToUTF16(test_case.unformated_text);
      std::u16string sanitized_text = WideToUTF16(test_case.formatted_text);
      EnsureTerminatedDirectionalFormatting(&unsanitized_text);
      EXPECT_EQ(sanitized_text, unsanitized_text);
    }
  }
  EXPECT_EQ(was_rtl, IsRTL());
}

TEST_F(RTLTest, SanitizeUserSuppliedString) {
  struct {
    const wchar_t* unformatted_text;
    const wchar_t* formatted_text;
  } cases[] = {
      // Tests RTL string with properly terminated dir-formatting character.
      {L"\x202eكبير Google التطبيق\x202c", L"\x202eكبير Google التطبيق\x202c"},
      // Tests RTL string with over-terminated dir-formatting characters.
      {L"\x202eكبير Google\x202cالتطبيق\x202c",
       L"\x202eكبير Google\x202cالتطبيق\x202c"},
      // Tests RTL string that over-terminates then re-opens.
      {L"\x202eكبير Google\x202c\x202cالتطبيق\x202e",
       L"\x202eكبير Google\x202c\x202cالتطبيق\x202e\x202c"},
      // Tests RTL string with multiple dir-formatting characters.
      {L"\x202eك\x202eبير Google الت\x202eطبيق",
       L"\x202eك\x202eبير Google الت\x202eطبيق\x202c\x202c\x202c"},
      // Test the other dir-formatting characters (U+202A, U+202B, and U+202D).
      {L"\x202aكبير Google التطبيق", L"\x202aكبير Google التطبيق\x202c"},
      {L"\x202bكبير Google التطبيق", L"\x202bكبير Google التطبيق\x202c"},
      {L"\x202dكبير Google التطبيق", L"\x202dكبير Google التطبيق\x202c"},

  };

  for (auto& i : cases) {
    // On Windows for an LTR locale, no changes to the string are made.
    std::u16string prefix, suffix = u"";
#if !defined(OS_WIN)
    prefix = u"\x200e\x202b";
    suffix = u"\x202c\x200e";
#endif  // !OS_WIN
    std::u16string unsanitized_text = WideToUTF16(i.unformatted_text);
    std::u16string sanitized_text =
        prefix + WideToUTF16(i.formatted_text) + suffix;
    SanitizeUserSuppliedString(&unsanitized_text);
    EXPECT_EQ(sanitized_text, unsanitized_text);
  }
}

class SetICULocaleTest : public PlatformTest {};

TEST_F(SetICULocaleTest, OverlongLocaleId) {
  test::ScopedRestoreICUDefaultLocale restore_locale;
  std::string id("fr-ca-x-foo");
  while (id.length() < 152)
    id.append("-x-foo");
  SetICUDefaultLocale(id);
  EXPECT_STRNE("en_US", icu::Locale::getDefault().getName());
  id.append("zzz");
  SetICUDefaultLocale(id);
  EXPECT_STREQ("en_US", icu::Locale::getDefault().getName());
}

}  // namespace i18n
}  // namespace base
