// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <limits>

#include "base/i18n/number_formatting.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/usearch.h"

namespace base {
namespace {

TEST(NumberFormattingTest, FormatNumber) {
  static const struct {
    int64_t number;
    const char* expected_english;
    const char* expected_german;
  } cases[] = {
    {0, "0", "0"},
    {1024, "1,024", "1.024"},
    {std::numeric_limits<int64_t>::max(),
        "9,223,372,036,854,775,807", "9.223.372.036.854.775.807"},
    {std::numeric_limits<int64_t>::min(),
        "-9,223,372,036,854,775,808", "-9.223.372.036.854.775.808"},
    {-42, "-42", "-42"},
  };

  test::ScopedRestoreICUDefaultLocale restore_locale;

  for (const auto& i : cases) {
    i18n::SetICUDefaultLocale("en");
    ResetFormattersForTesting();
    EXPECT_EQ(i.expected_english, UTF16ToUTF8(FormatNumber(i.number)));
    i18n::SetICUDefaultLocale("de");
    ResetFormattersForTesting();
    EXPECT_EQ(i.expected_german, UTF16ToUTF8(FormatNumber(i.number)));
  }
}

TEST(NumberFormattingTest, FormatDouble) {
  static const struct {
    double number;
    int frac_digits;
    const char* expected_english;
    const char* expected_german;
  } cases[] = {
    {0.0, 0, "0", "0"},
#if !defined(OS_ANDROID)
    // Bionic can't printf negative zero correctly.
    {-0.0, 4, "-0.0000", "-0,0000"},
#endif
    {1024.2, 0, "1,024", "1.024"},
    {-1024.223, 2, "-1,024.22", "-1.024,22"},
    {std::numeric_limits<double>::max(), 6,
     "179,769,313,486,231,570,000,000,000,000,000,000,000,000,000,000,000,"
     "000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,"
     "000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,"
     "000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,"
     "000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,"
     "000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,"
     "000.000000",
     "179.769.313.486.231.570.000.000.000.000.000.000.000.000.000.000.000."
     "000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000."
     "000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000."
     "000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000."
     "000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000."
     "000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000."
     "000,000000"},
    {std::numeric_limits<double>::min(), 2, "0.00", "0,00"},
    {-42.7, 3, "-42.700", "-42,700"},
  };

  test::ScopedRestoreICUDefaultLocale restore_locale;
  for (const auto& i : cases) {
    i18n::SetICUDefaultLocale("en");
    ResetFormattersForTesting();
    EXPECT_EQ(i.expected_english,
              UTF16ToUTF8(FormatDouble(i.number, i.frac_digits)));
    i18n::SetICUDefaultLocale("de");
    ResetFormattersForTesting();
    EXPECT_EQ(i.expected_german,
              UTF16ToUTF8(FormatDouble(i.number, i.frac_digits)));
  }
}

TEST(NumberFormattingTest, FormatPercent) {
  static const struct {
    int64_t number;
    const char* expected_english;
    const char* expected_german;  // Note: Space before % isn't \x20.
    // Note: Eastern Arabic-Indic digits (U+06Fx) for Persian and
    // Arabic-Indic digits (U+066x) for Arabic in Egypt(ar-EG). In Arabic (ar),
    // uses European digits (Google-patch).
    // See https://unicode.org/cldr/trac/ticket/9040 for details.
    // See also https://unicode.org/cldr/trac/ticket/10176 .
    // For now, take what CLDR 32 has (percent sign to the right of
    // a number in Persian).
    const char* expected_persian;
    const char* expected_arabic;
    const char* expected_arabic_egypt;
  } cases[] = {
      {0, "0%", u8"0\u00a0%", u8"\u06f0\u066a", u8"0\u200e%\u200e",
       u8"\u0660\u066a\u061c"},
      {42, "42%", "42\u00a0%", u8"\u06f4\u06f2\u066a", u8"42\u200e%\u200e",
       "\u0664\u0662\u066a\u061c"},
      {1024, "1,024%", "1.024\u00a0%", u8"\u06f1\u066c\u06f0\u06f2\u06f4\u066a",
       "1,024\u200e%\u200e", "\u0661\u066c\u0660\u0662\u0664\u066a\u061c"},
  };

  test::ScopedRestoreICUDefaultLocale restore_locale;
  for (const auto& i : cases) {
    i18n::SetICUDefaultLocale("en");
    EXPECT_EQ(ASCIIToUTF16(i.expected_english), FormatPercent(i.number));
    i18n::SetICUDefaultLocale("de");
    EXPECT_EQ(UTF8ToUTF16(i.expected_german), FormatPercent(i.number));
    i18n::SetICUDefaultLocale("fa");
    EXPECT_EQ(UTF8ToUTF16(i.expected_persian), FormatPercent(i.number));
    i18n::SetICUDefaultLocale("ar");
    EXPECT_EQ(UTF8ToUTF16(i.expected_arabic), FormatPercent(i.number));
    i18n::SetICUDefaultLocale("ar-EG");
    EXPECT_EQ(UTF8ToUTF16(i.expected_arabic_egypt), FormatPercent(i.number));
  }
}

}  // namespace
}  // namespace base
