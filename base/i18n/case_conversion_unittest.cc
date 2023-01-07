// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/case_conversion.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/usearch.h"

namespace base {
namespace i18n {

namespace {

const char16_t kNonASCIIMixed[] =
    u"\xC4\xD6\xE4\xF6\x20\xCF\xEF\x20\xF7\x25"
    u"\xA4\x23\x2A\x5E\x60\x40\xA3\x24\x2030\x201A\x7E\x20\x1F07\x1F0F"
    u"\x20\x1E00\x1E01";
const char16_t kNonASCIILower[] =
    u"\xE4\xF6\xE4\xF6\x20\xEF\xEF"
    u"\x20\xF7\x25\xA4\x23\x2A\x5E\x60\x40\xA3\x24\x2030\x201A\x7E\x20\x1F07"
    u"\x1F07\x20\x1E01\x1E01";
const char16_t kNonASCIIUpper[] =
    u"\xC4\xD6\xC4\xD6\x20\xCF\xCF"
    u"\x20\xF7\x25\xA4\x23\x2A\x5E\x60\x40\xA3\x24\x2030\x201A\x7E\x20\x1F0F"
    u"\x1F0F\x20\x1E00\x1E00";

}  // namespace

// Test upper and lower case string conversion.
TEST(CaseConversionTest, UpperLower) {
  const std::u16string mixed(u"Text with UPPer & lowER casE.");
  const std::u16string expected_lower(u"text with upper & lower case.");
  const std::u16string expected_upper(u"TEXT WITH UPPER & LOWER CASE.");

  std::u16string result = ToLower(mixed);
  EXPECT_EQ(expected_lower, result);

  result = ToUpper(mixed);
  EXPECT_EQ(expected_upper, result);
}

TEST(CaseConversionTest, NonASCII) {
  const std::u16string mixed(kNonASCIIMixed);
  const std::u16string expected_lower(kNonASCIILower);
  const std::u16string expected_upper(kNonASCIIUpper);

  std::u16string result = ToLower(mixed);
  EXPECT_EQ(expected_lower, result);

  result = ToUpper(mixed);
  EXPECT_EQ(expected_upper, result);
}

TEST(CaseConversionTest, TurkishLocaleConversion) {
  const std::u16string mixed(u"\x49\x131");
  const std::u16string expected_lower(u"\x69\x131");
  const std::u16string expected_upper(u"\x49\x49");

  test::ScopedRestoreICUDefaultLocale restore_locale;
  i18n::SetICUDefaultLocale("en_US");

  std::u16string result = ToLower(mixed);
  EXPECT_EQ(expected_lower, result);

  result = ToUpper(mixed);
  EXPECT_EQ(expected_upper, result);

  i18n::SetICUDefaultLocale("tr");

  const std::u16string expected_lower_turkish(u"\x131\x131");
  const std::u16string expected_upper_turkish(u"\x49\x49");

  result = ToLower(mixed);
  EXPECT_EQ(expected_lower_turkish, result);

  result = ToUpper(mixed);
  EXPECT_EQ(expected_upper_turkish, result);
}

TEST(CaseConversionTest, FoldCase) {
  // Simple ASCII, should lower-case.
  EXPECT_EQ(u"hello, world", FoldCase(u"Hello, World"));

  // Non-ASCII cases from above. They should all fold to the same result.
  EXPECT_EQ(FoldCase(kNonASCIIMixed), FoldCase(kNonASCIILower));
  EXPECT_EQ(FoldCase(kNonASCIIMixed), FoldCase(kNonASCIIUpper));

  // Turkish cases from above. This is the lower-case expected result from the
  // US locale. It should be the same even when the current locale is Turkish.
  const std::u16string turkish(u"\x49\x131");
  const std::u16string turkish_expected(u"\x69\x131");

  test::ScopedRestoreICUDefaultLocale restore_locale;
  i18n::SetICUDefaultLocale("en_US");
  EXPECT_EQ(turkish_expected, FoldCase(turkish));

  i18n::SetICUDefaultLocale("tr");
  EXPECT_EQ(turkish_expected, FoldCase(turkish));

  // Test a case that gets bigger when processed.
  // U+130 = LATIN CAPITAL LETTER I WITH DOT ABOVE gets folded to a lower case
  // "i" followed by U+307 COMBINING DOT ABOVE.
  EXPECT_EQ(u"i\u0307j", FoldCase(u"\u0130j"));

  // U+00DF (SHARP S) and U+1E9E (CAPIRAL SHARP S) are both folded to "ss".
  EXPECT_EQ(u"ssss", FoldCase(u"\u00DF\u1E9E"));
}

}  // namespace i18n
}  // namespace base



