// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/number_formatting.h"

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <limits>
#include <vector>

#include "base/compiler_specific.h"
#include "base/features.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/rtl.h"
#include "base/i18n/test/scoped_icu_locale.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequence_manager/thread_controller_power_monitor.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/simple_thread.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/ures.h"
#include "third_party/icu/source/i18n/unicode/curramt.h"
#include "third_party/icu/source/i18n/unicode/currunit.h"
#include "third_party/icu/source/i18n/unicode/dcfmtsym.h"
#include "third_party/icu/source/i18n/unicode/decimfmt.h"
#include "third_party/icu/source/i18n/unicode/numfmt.h"
#include "third_party/icu/source/i18n/unicode/numsys.h"
#include "third_party/icu/source/i18n/unicode/unum.h"
#include "third_party/icu/source/i18n/unicode/unumberformatter.h"
#include "third_party/icu/source/i18n/unicode/usearch.h"

namespace base {
namespace {

class ScopedI18nOptimizationsFeature {
 public:
  explicit ScopedI18nOptimizationsFeature(bool enable) {
    features_.InitWithFeatureState(features::kI18nOptimizations, enable);
    features::Init();
    sequence_manager::internal::ThreadControllerPowerMonitor::ResetForTesting();
  }
  ~ScopedI18nOptimizationsFeature() {
    features_.Reset();
    features::Init();
    sequence_manager::internal::ThreadControllerPowerMonitor::ResetForTesting();
  }

 private:
  test::ScopedFeatureList features_;
};

class NumberFormatWorkerThread : public base::SimpleThread {
 public:
  NumberFormatWorkerThread(base::WaitableEvent* event,
                           int digits,
                           int iterations)
      : SimpleThread("NumberFormatWorkerThread"),
        event_(event),
        digits_(digits),
        iterations_(iterations) {}

  void Run() override {
    event_->Wait();
    for (int i = 0; i < iterations_; ++i) {
      FormatDouble(1.2345678, digits_);
    }
  }

 private:
  const raw_ptr<base::WaitableEvent> event_;
  const int digits_;
  const int iterations_;
};

TEST(NumberFormattingTest, FormatNumber) {
  static const struct {
    int64_t number;
    const char* expected_english;
    const char* expected_german;
  } cases[] = {
      {0, "0", "0"},
      {1024, "1,024", "1.024"},
      {std::numeric_limits<int64_t>::max(), "9,223,372,036,854,775,807",
       "9.223.372.036.854.775.807"},
      {std::numeric_limits<int64_t>::min(), "-9,223,372,036,854,775,808",
       "-9.223.372.036.854.775.808"},
      {-42, "-42", "-42"},
  };

  for (const auto& i : cases) {
    {
      i18n::ScopedDefaultIcuLocale scoped_locale(
          i18n::GetKnownLanguageTag("en"));
      ResetFormattersForTesting();
      EXPECT_EQ(i.expected_english, UTF16ToUTF8(FormatNumber(i.number)));
    }
    {
      i18n::ScopedDefaultIcuLocale scoped_locale(
          i18n::GetKnownLanguageTag("de"));
      ResetFormattersForTesting();
      EXPECT_EQ(i.expected_german, UTF16ToUTF8(FormatNumber(i.number)));
    }
  }
}

TEST(NumberFormattingTest, FormatDoubleWithFixedFractionalDigits) {
  static const struct {
    double number;
    int frac_digits;
    const char* expected_english;
    const char* expected_german;
  } cases[] = {
      {0.0, 0, "0", "0"},
#if !BUILDFLAG(IS_ANDROID)
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

  for (const auto& i : cases) {
    {
      i18n::ScopedDefaultIcuLocale scoped_locale(
          i18n::GetKnownLanguageTag("en"));
      ResetFormattersForTesting();
      EXPECT_EQ(i.expected_english,
                UTF16ToUTF8(FormatDouble(i.number, i.frac_digits)));
    }
    {
      i18n::ScopedDefaultIcuLocale scoped_locale(
          i18n::GetKnownLanguageTag("de"));
      ResetFormattersForTesting();
      EXPECT_EQ(i.expected_german,
                UTF16ToUTF8(FormatDouble(i.number, i.frac_digits)));
    }
  }
}

TEST(NumberFormattingTest, FormatDoubleWithFractionalDigitRange) {
  static const struct {
    double number;
    int min_frac_digits;
    int max_frac_digits;
    const char* expected_english;
    const char* expected_german;
  } cases[] = {
      {0.0, 0, 0, "0", "0"},
#if !BUILDFLAG(IS_ANDROID)
      // Bionic can't printf negative zero correctly.
      {-0.0, 0, 4, "-0", "-0"},
#endif
      {1024.2, 0, 0, "1,024", "1.024"},
      {-1024.223, 0, 2, "-1,024.22", "-1.024,22"},
      {std::numeric_limits<double>::max(), 0, 6,
       "179,769,313,486,231,570,000,000,000,000,000,000,000,000,000,000,000,"
       "000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,"
       "000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,"
       "000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,"
       "000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,"
       "000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,"
       "000",
       "179.769.313.486.231.570.000.000.000.000.000.000.000.000.000.000.000."
       "000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000."
       "000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000."
       "000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000."
       "000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000."
       "000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000.000."
       "000"},
      {std::numeric_limits<double>::min(), 2, 2, "0.00", "0,00"},
      {-42.7, 0, 3, "-42.7", "-42,7"},
  };

  for (const auto& i : cases) {
    {
      i18n::ScopedDefaultIcuLocale scoped_locale(
          i18n::GetKnownLanguageTag("en"));
      ResetFormattersForTesting();
      EXPECT_EQ(i.expected_english,
                UTF16ToUTF8(FormatDouble(i.number, i.min_frac_digits,
                                         i.max_frac_digits)));
    }
    {
      i18n::ScopedDefaultIcuLocale scoped_locale(
          i18n::GetKnownLanguageTag("de"));
      ResetFormattersForTesting();
      EXPECT_EQ(i.expected_german,
                UTF16ToUTF8(FormatDouble(i.number, i.min_frac_digits,
                                         i.max_frac_digits)));
    }
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
      {0, "0%", "0\u00a0%", "\u06f0\u066a", "0\u200e%\u200e",
       "\u0660\u066a\u061c"},
      {42, "42%", "42\u00a0%", "\u06f4\u06f2\u066a", "42\u200e%\u200e",
       "\u0664\u0662\u066a\u061c"},
      {1024, "1,024%", "1.024\u00a0%", "\u06f1\u066c\u06f0\u06f2\u06f4\u066a",
       "1,024\u200e%\u200e", "\u0661\u066c\u0660\u0662\u0664\u066a\u061c"},
  };

  for (bool enable_feature : {false, true}) {
    ScopedI18nOptimizationsFeature scoped_feature(enable_feature);
    for (const auto& i : cases) {
      {
        i18n::ScopedDefaultIcuLocale scoped_locale(
            i18n::GetKnownLanguageTag("en"));
        EXPECT_EQ(ASCIIToUTF16(i.expected_english), FormatPercent(i.number));
        EXPECT_EQ(ASCIIToUTF16(i.expected_english), FormatPercent(i.number));
      }
      {
        i18n::ScopedDefaultIcuLocale scoped_locale(
            i18n::GetKnownLanguageTag("de"));
        EXPECT_EQ(UTF8ToUTF16(i.expected_german), FormatPercent(i.number));
        EXPECT_EQ(UTF8ToUTF16(i.expected_german), FormatPercent(i.number));
      }
      {
        i18n::ScopedDefaultIcuLocale scoped_locale(
            i18n::GetKnownLanguageTag("fa"));
        EXPECT_EQ(UTF8ToUTF16(i.expected_persian), FormatPercent(i.number));
        EXPECT_EQ(UTF8ToUTF16(i.expected_persian), FormatPercent(i.number));
      }
      {
        i18n::ScopedDefaultIcuLocale scoped_locale(
            i18n::GetKnownLanguageTag("ar"));
        EXPECT_EQ(UTF8ToUTF16(i.expected_arabic), FormatPercent(i.number));
        EXPECT_EQ(UTF8ToUTF16(i.expected_arabic), FormatPercent(i.number));
      }
      {
        i18n::ScopedDefaultIcuLocale scoped_locale(
            i18n::GetKnownLanguageTag("ar-EG"));
        EXPECT_EQ(UTF8ToUTF16(i.expected_arabic_egypt),
                  FormatPercent(i.number));
        EXPECT_EQ(UTF8ToUTF16(i.expected_arabic_egypt),
                  FormatPercent(i.number));
      }
    }
  }
}

// Regression test for crbug.com/506477192. Ensure that concurrent calls to
// FormatDouble with different fractional digits don't cause a data race in
// the shared ICU number formatter.
TEST(NumberFormattingTest, FormatDoubleRace) {
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  std::vector<std::unique_ptr<NumberFormatWorkerThread>> threads;

  for (int i = 0; i < 20; ++i) {
    threads
        .emplace_back(
            std::make_unique<NumberFormatWorkerThread>(&event, i % 5, 1000))
        ->Start();
  }

  event.Signal();

  for (auto& t : threads) {
    t->Join();
  }
}

std::string IcuToUtf8(const icu::UnicodeString& str) {
  std::string out;
  str.toUTF8String(out);
  return out;
}

// Validates that NumberFormat, DecimalFormat, DecimalFormatSymbols,
// NumberingSystem, and UNumberFormatter produce the exact expected formatted
// outputs across default and explicit numbering systems.
TEST(NumberFormattingTest, IcuNumberFormatNumberingSystems) {
  struct TestCase {
    const char* locale_name;
    const char* expected_ns_name;
    const char* expected_zero_digit;
    const char* expected_percent_symbol;
    const char* expected_grouping_separator;
    const char* expected_number;
    const char* expected_percent;
    const char* expected_currency;
    const char* expected_scientific;
    const char* expected_with_de_symbols;
  };
  static constexpr auto kCases = std::to_array<TestCase>({
      {"en-US", "latn", "0", "%", ",", "12,345.67", "42%", "$1,234.56",
       "1.234567E4", "1.234,5"},
      {"de-DE", "latn", "0", "%", ".", "12.345,67", "42\xc2\xa0%",
       "1.234,56\xc2\xa0\xe2\x82\xac", "1,234567E4", "1.234,5"},
      {"fa", "arabext", "\xdb\xb0", "\xd9\xaa", "\xd9\xac",
       "\xdb\xb1\xdb\xb2\xd9\xac\xdb\xb3\xdb\xb4\xdb\xb5\xd9\xab\xdb\xb6\xdb"
       "\xb7",
       "\xdb\xb4\xdb\xb2\xd9\xaa",
       "\xe2\x80\x8eXXX\xc2\xa0\xdb\xb1\xd9\xac\xdb\xb2\xdb\xb3\xdb\xb4\xd9\xab"
       "\xdb\xb5\xdb\xb6",
       "\xdb\xb1\xd9\xab\xdb\xb2\xdb\xb3\xdb\xb4\xdb\xb5\xdb\xb6\xdb\xb7\xc3"
       "\x97\xdb\xb1\xdb\xb0^\xdb\xb4",
       "1.234,5"},
      {"ar-EG", "arab", "\xd9\xa0", "\xd9\xaa\xd8\x9c", "\xd9\xac",
       "\xd9\xa1\xd9\xa2\xd9\xac\xd9\xa3\xd9\xa4\xd9\xa5\xd9\xab\xd9\xa6\xd9"
       "\xa7",
       "\xd9\xa4\xd9\xa2\xd9\xaa\xd8\x9c",
       "\xe2\x80\x8f\xd9\xa1\xd9\xac\xd9\xa2\xd9\xa3\xd9\xa4\xd9\xab\xd9\xa5"
       "\xd9\xa6\xc2\xa0\xd8\xac.\xd9\x85.\xe2\x80\x8f",
       "\xd9\xa1\xd9\xab\xd9\xa2\xd9\xa3\xd9\xa4\xd9\xa5\xd9\xa6\xd9\xa7\xd8"
       "\xa3\xd8\xb3\xd9\xa4",
       "1.234,5"},
#if BUILDFLAG(IS_IOS) && !BUILDFLAG(USE_BLINK)
      // On non-Blink iOS, `third_party/icu/filters/ios.json` abridges non-UI
      // locale data for "bn" and "mr" (stripping `NumberElements` and excluding
      // them from `curr_tree`), so they fall back to "root" ("latn").
      // TODO(crbug.com/568824848): Check whether this fallback is intended.
      {"bn", "latn", "0", "%", ",", "12,345.67", "42%",
       "XXX\xc2\xa0"
       "1,234.56",
       "1.234567E4", "1.234,5"},
      {"mr", "latn", "0", "%", ",", "12,345.67", "42%",
       "XXX\xc2\xa0"
       "1,234.56",
       "1.234567E4", "1.234,5"},
#else
      {"bn", "beng", "\xe0\xa7\xa6", "%", ",",
       "\xe0\xa7\xa7\xe0\xa7\xa8,\xe0\xa7\xa9\xe0\xa7\xaa\xe0\xa7\xab.\xe0\xa7"
       "\xac\xe0\xa7\xad",
       "\xe0\xa7\xaa\xe0\xa7\xa8%",
       "\xe0\xa7\xa7,\xe0\xa7\xa8\xe0\xa7\xa9\xe0\xa7\xaa.\xe0\xa7\xab\xe0\xa7"
       "\xac\xc2\xa0XXX",
       "\xe0\xa7\xa7.\xe0\xa7\xa8\xe0\xa7\xa9\xe0\xa7\xaa\xe0\xa7\xab\xe0\xa7"
       "\xac\xe0\xa7\xad"
       "E\xe0\xa7\xaa",
       "1.234,5"},
      {"mr", "deva", "\xe0\xa5\xa6", "%", ",",
       "\xe0\xa5\xa7\xe0\xa5\xa8,\xe0\xa5\xa9\xe0\xa5\xaa\xe0\xa5\xab.\xe0\xa5"
       "\xac\xe0\xa5\xad",
       "\xe0\xa5\xaa\xe0\xa5\xa8%",
       "XXX\xc2\xa0\xe0\xa5\xa7,\xe0\xa5\xa8\xe0\xa5\xa9\xe0\xa5\xaa.\xe0\xa5"
       "\xab\xe0\xa5\xac",
       "[\xe0\xa5\xa7.\xe0\xa5\xa8\xe0\xa5\xa9\xe0\xa5\xaa\xe0\xa5\xab\xe0\xa5"
       "\xac\xe0\xa5\xad"
       "E\xe0\xa5\xaa]",
       "1.234,5"},
#endif
      {"en-US-u-nu-arab", "arab", "\xd9\xa0", "\xd9\xaa\xd8\x9c", "\xd9\xac",
       "\xd9\xa1\xd9\xa2\xd9\xac\xd9\xa3\xd9\xa4\xd9\xa5\xd9\xab\xd9\xa6\xd9"
       "\xa7",
       "\xd9\xa4\xd9\xa2\xd9\xaa\xd8\x9c",
       "\xd9\xa1\xd9\xac\xd9\xa2\xd9\xa3\xd9\xa4\xd9\xab\xd9\xa5\xd9\xa6\xc2"
       "\xa0$",
       "\xd9\xa1\xd9\xab\xd9\xa2\xd9\xa3\xd9\xa4\xd9\xa5\xd9\xa6\xd9\xa7\xd8"
       "\xa7\xd8\xb3\xd9\xa4",
       "1.234,5"},
      {"en-US-u-nu-deva", "deva", "\xe0\xa5\xa6", "%", ",",
       "\xe0\xa5\xa7\xe0\xa5\xa8,\xe0\xa5\xa9\xe0\xa5\xaa\xe0\xa5\xab.\xe0\xa5"
       "\xac\xe0\xa5\xad",
       "\xe0\xa5\xaa\xe0\xa5\xa8%",
       "$\xe0\xa5\xa7,\xe0\xa5\xa8\xe0\xa5\xa9\xe0\xa5\xaa.\xe0\xa5\xab\xe0\xa5"
       "\xac",
       "\xe0\xa5\xa7.\xe0\xa5\xa8\xe0\xa5\xa9\xe0\xa5\xaa\xe0\xa5\xab\xe0\xa5"
       "\xac\xe0\xa5\xad"
       "E\xe0\xa5\xaa",
       "1.234,5"},
      {"ar-EG-u-nu-latn", "latn", "0", "\xe2\x80\x8e%\xe2\x80\x8e", ",",
       "12,345.67", "42\xe2\x80\x8e%\xe2\x80\x8e",
       "\xe2\x80\x8f"
       "1,234.56\xc2\xa0\xd8\xac.\xd9\x85.\xe2\x80\x8f",
       "1.234567E4", "1.234,5"},
  });

  for (const auto& tc : kCases) {
    SCOPED_TRACE(tc.locale_name);
    UErrorCode status = U_ZERO_ERROR;
    icu::Locale locale = icu::Locale::forLanguageTag(tc.locale_name, status);
    ASSERT_TRUE(U_SUCCESS(status));

    std::unique_ptr<icu::NumberingSystem> expected_ns(
        icu::NumberingSystem::createInstance(locale, status));
    ASSERT_TRUE(U_SUCCESS(status));
    ASSERT_NE(expected_ns, nullptr);
    EXPECT_STREQ(expected_ns->getName(), tc.expected_ns_name);

    icu::DecimalFormatSymbols explicit_symbols(locale, *expected_ns, status);
    ASSERT_TRUE(U_SUCCESS(status));
    EXPECT_EQ(IcuToUtf8(explicit_symbols.getSymbol(
                  icu::DecimalFormatSymbols::kZeroDigitSymbol)),
              tc.expected_zero_digit);
    EXPECT_EQ(IcuToUtf8(explicit_symbols.getSymbol(
                  icu::DecimalFormatSymbols::kPercentSymbol)),
              tc.expected_percent_symbol);
    EXPECT_EQ(IcuToUtf8(explicit_symbols.getSymbol(
                  icu::DecimalFormatSymbols::kGroupingSeparatorSymbol)),
              tc.expected_grouping_separator);

    // Verify NumberFormat::createInstance, createPercentInstance,
    // createCurrencyInstance, and createScientificInstance against hardcoded
    // expectations.
    std::unique_ptr<icu::NumberFormat> nf_number(
        icu::NumberFormat::createInstance(locale, status));
    std::unique_ptr<icu::NumberFormat> nf_percent(
        icu::NumberFormat::createPercentInstance(locale, status));
    std::unique_ptr<icu::NumberFormat> nf_currency(
        icu::NumberFormat::createCurrencyInstance(locale, status));
    std::unique_ptr<icu::NumberFormat> nf_scientific(
        icu::NumberFormat::createScientificInstance(locale, status));
    ASSERT_TRUE(U_SUCCESS(status));

    auto* df_number = static_cast<icu::DecimalFormat*>(nf_number.get());
    ASSERT_NE(df_number, nullptr);
    const icu::DecimalFormatSymbols* df_syms =
        df_number->getDecimalFormatSymbols();
    ASSERT_NE(df_syms, nullptr);
    EXPECT_EQ(IcuToUtf8(df_syms->getSymbol(
                  icu::DecimalFormatSymbols::kZeroDigitSymbol)),
              tc.expected_zero_digit);
    EXPECT_EQ(IcuToUtf8(df_syms->getSymbol(
                  icu::DecimalFormatSymbols::kPercentSymbol)),
              tc.expected_percent_symbol);
    EXPECT_EQ(IcuToUtf8(df_syms->getSymbol(
                  icu::DecimalFormatSymbols::kGroupingSeparatorSymbol)),
              tc.expected_grouping_separator);

    icu::UnicodeString nf_formatted_number;
    nf_number->format(12345.67, nf_formatted_number);
    EXPECT_EQ(IcuToUtf8(nf_formatted_number), tc.expected_number);

    icu::UnicodeString nf_formatted_percent;
    nf_percent->format(0.42, nf_formatted_percent);
    EXPECT_EQ(IcuToUtf8(nf_formatted_percent), tc.expected_percent);

    icu::UnicodeString nf_formatted_currency;
    nf_currency->format(1234.56, nf_formatted_currency);
    EXPECT_EQ(IcuToUtf8(nf_formatted_currency), tc.expected_currency);

    icu::UnicodeString nf_formatted_scientific;
    nf_scientific->format(12345.67, nf_formatted_scientific);
    EXPECT_EQ(IcuToUtf8(nf_formatted_scientific), tc.expected_scientific);

    // Verify UNumberFormatter (where no DecimalFormatSymbols are pre-supplied)
    // produces the exact expected output.
    icu::LocalUNumberFormatterPointer uformatter(
        unumf_openForSkeletonAndLocale(u"", 0, locale.getName(), &status));
    icu::LocalUFormattedNumberPointer uresult(unumf_openResult(&status));
    ASSERT_TRUE(U_SUCCESS(status));
    unumf_formatDouble(uformatter.getAlias(), 12345.67, uresult.getAlias(),
                       &status);
    ASSERT_TRUE(U_SUCCESS(status));
    int32_t buf_len =
        unumf_resultToString(uresult.getAlias(), nullptr, 0, &status);
    status = U_ZERO_ERROR;
    std::u16string uformatter_str(static_cast<size_t>(buf_len + 1), u'\0');
    unumf_resultToString(uresult.getAlias(), uformatter_str.data(), buf_len + 1,
                         &status);
    ASSERT_TRUE(U_SUCCESS(status));
    uformatter_str.resize(static_cast<size_t>(buf_len));
    EXPECT_EQ(UTF16ToUTF8(uformatter_str), tc.expected_number);

    // Verify DecimalFormat with explicit same-locale DecimalFormatSymbols.
    icu::UnicodeString pattern;
    df_number->toPattern(pattern);
    icu::DecimalFormat df_explicit_syms(pattern, explicit_symbols, status);
    ASSERT_TRUE(U_SUCCESS(status));
    icu::UnicodeString formatted_explicit_syms;
    df_explicit_syms.format(12345.67, formatted_explicit_syms);
    EXPECT_EQ(IcuToUtf8(formatted_explicit_syms), tc.expected_number);

    // Verify DecimalFormat when explicit symbols come from a different locale
    // ("de-DE") than the formatter's pattern.
    icu::DecimalFormatSymbols de_symbols(icu::Locale("de", "DE"), status);
    ASSERT_TRUE(U_SUCCESS(status));
    icu::DecimalFormat df_cross_locale(pattern, de_symbols, status);
    ASSERT_TRUE(U_SUCCESS(status));
    icu::UnicodeString cross_locale_formatted;
    df_cross_locale.format(1234.5, cross_locale_formatted);
    EXPECT_EQ(IcuToUtf8(cross_locale_formatted), tc.expected_with_de_symbols);
  }
}

// Validates edge cases for numbering systems: algorithmic numbering systems
// (which instantiate RuleBasedNumberFormat or fall back to "latn" symbols while
// preserving algorithmic formatting), explicit non-default NumberingSystem
// passed to DecimalFormatSymbols, and compact number formatting.
TEST(NumberFormattingTest, IcuNumberFormatNumberingSystemEdgeCases) {
  // 1. Algorithmic numbering systems via -u-nu- tags (using ja/zh RBNF rules
  // included in Chromium's ICU data table).
  struct AlgorithmicCase {
    const char* locale_name;
    const char* expected_ns_name;
    const char* expected_number;
    const char* expected_percent;
    const char* expected_currency;
  };
  static constexpr auto kAlgorithmicCases = std::to_array<AlgorithmicCase>({
      {"zh-CN-u-nu-hans", "hans",
       "\xe4\xb8\x80\xe5\x8d\x83\xe4\xba\x8c\xe7\x99\xbe\xe4\xb8\x89\xe5\x8d"
       "\x81\xe5\x9b\x9b",
       "\xe5\x9b\x9b\xe5\x8d\x81\xe4\xba\x8c",
       "\xe4\xb8\x80\xe5\x8d\x83\xe4\xba\x8c\xe7\x99\xbe\xe4\xb8\x89\xe5\x8d"
       "\x81\xe5\x9b\x9b\xe7\x82\xb9\xe4\xba\x94\xe5\x85\xad"},
      {"zh-CN-u-nu-hansfin", "hansfin",
       "\xe5\xa3\xb9\xe4\xbb\x9f\xe8\xb4\xb0\xe4\xbd\xb0\xe5\x8f\x81\xe6\x8b"
       "\xbe\xe8\x82\x86",
       "\xe8\x82\x86\xe6\x8b\xbe\xe8\xb4\xb0",
       "\xe5\xa3\xb9\xe4\xbb\x9f\xe8\xb4\xb0\xe4\xbd\xb0\xe5\x8f\x81\xe6\x8b"
       "\xbe\xe8\x82\x86\xe7\x82\xb9\xe4\xbc\x8d\xe9\x99\x86"},
      {"ja-JP-u-nu-jpan", "jpan",
       "\xe5\x8d\x83\xe4\xba\x8c\xe7\x99\xbe\xe4\xb8\x89\xe5\x8d\x81\xe5\x9b"
       "\x9b",
       "\xe5\x9b\x9b\xe5\x8d\x81\xe4\xba\x8c",
       "\xe5\x8d\x83\xe4\xba\x8c\xe7\x99\xbe\xe4\xb8\x89\xe5\x8d\x81\xe5\x9b"
       "\x9b\xe3\x83\xbb\xe4\xba\x94\xe5\x85\xad"},
      {"ja-JP-u-nu-jpanfin", "jpanfin",
       "\xe5\xa3\xb1\xe5\x8d\x83\xe5\xbc\x90\xe7\x99\xbe\xe5\x8f\x82\xe6\x8b"
       "\xbe\xe5\x9b\x9b",
       "\xe5\x9b\x9b\xe6\x8b\xbe\xe5\xbc\x90",
       "\xe5\xa3\xb1\xe5\x8d\x83\xe5\xbc\x90\xe7\x99\xbe\xe5\x8f\x82\xe6\x8b"
       "\xbe\xe5\x9b\x9b\xe7\x82\xb9\xe4\xbc\x8d\xe5\x85\xad"},
  });

  for (const auto& tc : kAlgorithmicCases) {
    SCOPED_TRACE(tc.locale_name);
    UErrorCode status = U_ZERO_ERROR;
    icu::Locale locale = icu::Locale::forLanguageTag(tc.locale_name, status);
    ASSERT_TRUE(U_SUCCESS(status));

    std::unique_ptr<icu::NumberingSystem> ns(
        icu::NumberingSystem::createInstance(locale, status));
    ASSERT_TRUE(U_SUCCESS(status));
    EXPECT_TRUE(ns->isAlgorithmic());
    EXPECT_STREQ(ns->getName(), tc.expected_ns_name);

    // DecimalFormatSymbols falls back to "latn" digits for algorithmic
    // numbering systems.
    icu::DecimalFormatSymbols syms(locale, status);
    ASSERT_TRUE(U_SUCCESS(status));
    EXPECT_EQ(
        IcuToUtf8(syms.getSymbol(icu::DecimalFormatSymbols::kZeroDigitSymbol)),
        "0");

    std::unique_ptr<icu::NumberFormat> nf_number(
        icu::NumberFormat::createInstance(locale, status));
    std::unique_ptr<icu::NumberFormat> nf_percent(
        icu::NumberFormat::createPercentInstance(locale, status));
    std::unique_ptr<icu::NumberFormat> nf_currency(
        icu::NumberFormat::createCurrencyInstance(locale, status));
#if BUILDFLAG(IS_IOS) && !BUILDFLAG(USE_BLINK)
    // On non-Blink iOS, `third_party/icu/filters/ios.json` excludes `rbnf_tree`
    // and `rbnf_index`, so `NumberFormat` creation for algorithmic numbering
    // systems fails with `U_MISSING_RESOURCE_ERROR`.
    // TODO(crbug.com/568824848): Check whether this is intended on iOS.
    EXPECT_EQ(status, U_MISSING_RESOURCE_ERROR);
    EXPECT_EQ(nf_number, nullptr);
    EXPECT_EQ(nf_percent, nullptr);
    EXPECT_EQ(nf_currency, nullptr);
    status = U_ZERO_ERROR;
#else
    ASSERT_TRUE(U_SUCCESS(status));

    icu::UnicodeString s_num, s_pct, s_cur;
    nf_number->format(1234, s_num);
    nf_percent->format(42, s_pct);
    nf_currency->format(1234.56, s_cur);
    EXPECT_EQ(IcuToUtf8(s_num), tc.expected_number);
    EXPECT_EQ(IcuToUtf8(s_pct), tc.expected_percent);
    EXPECT_EQ(IcuToUtf8(s_cur), tc.expected_currency);
#endif

    // UNumberFormatter uses "latn" fallback for algorithmic numbering systems
    // (UNumberFormatter does not instantiate RuleBasedNumberFormat).
    icu::LocalUNumberFormatterPointer uformatter(
        unumf_openForSkeletonAndLocale(u"", 0, locale.getName(), &status));
    icu::LocalUFormattedNumberPointer uresult(unumf_openResult(&status));
    ASSERT_TRUE(U_SUCCESS(status));
    unumf_formatInt(uformatter.getAlias(), 1234, uresult.getAlias(), &status);
    ASSERT_TRUE(U_SUCCESS(status));
    int32_t buf_len =
        unumf_resultToString(uresult.getAlias(), nullptr, 0, &status);
    status = U_ZERO_ERROR;
    std::u16string uformatter_str(static_cast<size_t>(buf_len + 1), u'\0');
    unumf_resultToString(uresult.getAlias(), uformatter_str.data(), buf_len + 1,
                         &status);
    ASSERT_TRUE(U_SUCCESS(status));
    uformatter_str.resize(static_cast<size_t>(buf_len));
    EXPECT_EQ(UTF16ToUTF8(uformatter_str), "1,234");
  }

  // 2. Explicit non-default NumberingSystem ("arab" or "deva") passed to
  // DecimalFormatSymbols with a "latn" locale ("en-US").
  {
    UErrorCode status = U_ZERO_ERROR;
    icu::Locale en_us("en", "US");
    std::unique_ptr<icu::NumberingSystem> arab_ns(
        icu::NumberingSystem::createInstanceByName("arab", status));
    ASSERT_TRUE(U_SUCCESS(status));
    icu::DecimalFormatSymbols en_with_arab(en_us, *arab_ns, status);
    ASSERT_TRUE(U_SUCCESS(status));
    icu::DecimalFormat df(u"#,##0.00", en_with_arab, status);
    ASSERT_TRUE(U_SUCCESS(status));
    icu::UnicodeString formatted;
    df.format(12345.67, formatted);
    // Uses Arabic-Indic digits and Arabic symbols loaded from
    // NumberElements/arab/symbols.
    EXPECT_EQ(IcuToUtf8(formatted),
              "\xd9\xa1\xd9\xa2\xd9\xac\xd9\xa3\xd9\xa4\xd9\xa5\xd9\xab\xd9\xa6"
              "\xd9\xa7");
  }

  // 3. Compact number formatting (UNUM_DECIMAL_COMPACT_SHORT and LONG).
  {
    struct CompactCase {
      const char* locale_name;
      const char* expected_short;
      const char* expected_long;
    };
    static constexpr auto kCompactCases = std::to_array<CompactCase>({
        {"en-US", "1.2M", "1.2 million"},
        {"de-DE", "1,2\xc2\xa0Mio.", "1,2 Millionen"},
        {"fa",
         "\xdb\xb1\xd9\xab\xdb\xb2\xc2\xa0\xd9\x85\xdb\x8c\xd9\x84\xdb\x8c\xd9"
         "\x88\xd9\x86",
         "\xdb\xb1\xd9\xab\xdb\xb2 \xd9\x85\xdb\x8c\xd9\x84\xdb\x8c\xd9\x88\xd9"
         "\x86"},
        {"ar-EG",
         "\xd9\xa1\xd9\xab\xd9\xa2\xc2\xa0\xd9\x85\xd9\x84\xd9\x8a\xd9\x88\xd9"
         "\x86",
         "\xd9\xa1\xd9\xab\xd9\xa2 \xd9\x85\xd9\x84\xd9\x8a\xd9\x88\xd9\x86"},
    });
    for (const auto& tc : kCompactCases) {
      SCOPED_TRACE(tc.locale_name);
      UErrorCode status = U_ZERO_ERROR;
      icu::LocalUNumberFormatPointer fmt_short(
          unum_open(UNUM_DECIMAL_COMPACT_SHORT, nullptr, 0, tc.locale_name,
                    nullptr, &status));
      icu::LocalUNumberFormatPointer fmt_long(
          unum_open(UNUM_DECIMAL_COMPACT_LONG, nullptr, 0, tc.locale_name,
                    nullptr, &status));
      ASSERT_TRUE(U_SUCCESS(status));

      char16_t buf[64];
      int32_t len_short = unum_formatDouble(fmt_short.getAlias(), 1234567.0,
                                            buf, 64, nullptr, &status);
      ASSERT_TRUE(U_SUCCESS(status));
      EXPECT_EQ(UTF16ToUTF8(std::u16string_view(buf, len_short)),
                tc.expected_short);

      int32_t len_long = unum_formatDouble(fmt_long.getAlias(), 1234567.0, buf,
                                           64, nullptr, &status);
      ASSERT_TRUE(U_SUCCESS(status));
      EXPECT_EQ(UTF16ToUTF8(std::u16string_view(buf, len_long)),
                tc.expected_long);
    }
  }
}

// Validates currency formatting and ensures that a non-currency DecimalFormat
// reconfigured to format currency lazily loads currency symbols and spacing
// data to produce the exact expected output.
TEST(NumberFormattingTest, IcuDecimalFormatCurrencyAndLazyCurrencyLoading) {
  struct TestCase {
    const char* locale_name;
    const char* expected_currency_symbol;
    const char* expected_intl_currency_symbol;
    const char* expected_monetary_decimal_separator;
    const char* expected_monetary_grouping_separator;
    const char* expected_currency_spacing_match;
    const char* expected_currency_spacing_insert;
    const char* expected_default_currency;
    const char* expected_eur_currency;
    const char* expected_intl_currency;
  };
  static constexpr auto kCases = std::to_array<TestCase>({
      {"en-US", "$", "USD", ".", ",", "[[:^S:]&[:^Z:]]", "\xc2\xa0",
       "$1,234.56",
       "\xe2\x82\xac"
       "1,234.56",
       "USD 1,234.56"},
      {"en-GB", "\xc2\xa3", "GBP", ".", ",", "[[:^S:]&[:^Z:]]", "\xc2\xa0",
       "\xc2\xa3"
       "1,234.56",
       "\xe2\x82\xac"
       "1,234.56",
       "GBP 1,234.56"},
      {"de-DE", "\xe2\x82\xac", "EUR", ",", ".", "[[:^S:]&[:^Z:]]", "\xc2\xa0",
       "1.234,56\xc2\xa0\xe2\x82\xac", "1.234,56\xc2\xa0\xe2\x82\xac",
       "EUR 1.234,56"},
      {"fr-FR", "\xe2\x82\xac", "EUR", ",", "\xe2\x80\xaf", "[[:^S:]&[:^Z:]]",
       "\xc2\xa0",
       "1\xe2\x80\xaf"
       "234,56\xc2\xa0\xe2\x82\xac",
       "1\xe2\x80\xaf"
       "234,56\xc2\xa0\xe2\x82\xac",
       "EUR 1\xe2\x80\xaf"
       "234,56"},
      {"ja-JP", "\xef\xbf\xa5", "JPY", ".", ",", "[[:^S:]&[:^Z:]]", "\xc2\xa0",
       "\xef\xbf\xa5"
       "1,235",
       "\xe2\x82\xac"
       "1,235",
       "JPY 1,234.56"},
      {"ar-EG", "\xd8\xac.\xd9\x85.\xe2\x80\x8f", "EGP", "\xd9\xab", "\xd9\xac",
       "[[:^S:]&[:^Z:]]", "\xc2\xa0",
       "\xe2\x80\x8f\xd9\xa1\xd9\xac\xd9\xa2\xd9\xa3\xd9\xa4\xd9\xab\xd9\xa5"
       "\xd9\xa6\xc2\xa0\xd8\xac.\xd9\x85.\xe2\x80\x8f",
       "\xe2\x80\x8f\xd9\xa1\xd9\xac\xd9\xa2\xd9\xa3\xd9\xa4\xd9\xab\xd9\xa5"
       "\xd9\xa6\xc2\xa0\xe2\x82\xac",
       "EGP \xd9\xa1\xd9\xac\xd9\xa2\xd9\xa3\xd9\xa4\xd9\xab\xd9\xa5\xd9\xa6"},
      {"fa-IR", "\xd8\xb1\xdb\x8c\xd8\xa7\xd9\x84", "IRR", "\xd9\xab",
       "\xd9\xac", "[[:^S:]&[:^Z:]]", "\xc2\xa0",
       "\xe2\x80\x8e\xd8\xb1\xdb\x8c\xd8\xa7\xd9\x84\xc2\xa0\xdb\xb1\xd9\xac"
       "\xdb\xb2\xdb\xb3\xdb\xb5",
       "\xe2\x80\x8e\xe2\x82\xac\xdb\xb1\xd9\xac\xdb\xb2\xdb\xb3\xdb\xb5",
       "IRR \xdb\xb1\xd9\xac\xdb\xb2\xdb\xb3\xdb\xb4\xd9\xab\xdb\xb5\xdb\xb6"},
  });

  for (const auto& tc : kCases) {
    SCOPED_TRACE(tc.locale_name);
    UErrorCode status = U_ZERO_ERROR;
    icu::Locale locale = icu::Locale::forLanguageTag(tc.locale_name, status);
    ASSERT_TRUE(U_SUCCESS(status));

    // 1. Both public DecimalFormatSymbols constructor and DecimalFormatSymbols
    // retrieved from a non-currency DecimalFormat must return the exact
    // expected currency symbols, monetary separators, and spacing patterns.
    icu::DecimalFormatSymbols public_syms(locale, status);
    ASSERT_TRUE(U_SUCCESS(status));
    EXPECT_EQ(IcuToUtf8(public_syms.getSymbol(
                  icu::DecimalFormatSymbols::kCurrencySymbol)),
              tc.expected_currency_symbol);
    EXPECT_EQ(IcuToUtf8(public_syms.getSymbol(
                  icu::DecimalFormatSymbols::kIntlCurrencySymbol)),
              tc.expected_intl_currency_symbol);
    EXPECT_EQ(IcuToUtf8(public_syms.getSymbol(
                  icu::DecimalFormatSymbols::kMonetarySeparatorSymbol)),
              tc.expected_monetary_decimal_separator);
    EXPECT_EQ(IcuToUtf8(public_syms.getSymbol(
                  icu::DecimalFormatSymbols::kMonetaryGroupingSeparatorSymbol)),
              tc.expected_monetary_grouping_separator);
    EXPECT_EQ(IcuToUtf8(public_syms.getPatternForCurrencySpacing(
                  UNUM_CURRENCY_MATCH, /*beforeCurrency=*/false, status)),
              tc.expected_currency_spacing_match);
    EXPECT_EQ(IcuToUtf8(public_syms.getPatternForCurrencySpacing(
                  UNUM_CURRENCY_INSERT, /*beforeCurrency=*/false, status)),
              tc.expected_currency_spacing_insert);
    ASSERT_TRUE(U_SUCCESS(status));

    std::unique_ptr<icu::NumberFormat> inspect_decimal_nf(
        icu::NumberFormat::createInstance(locale, status));
    ASSERT_TRUE(U_SUCCESS(status));
    const icu::DecimalFormatSymbols* lazy_syms =
        static_cast<icu::DecimalFormat*>(inspect_decimal_nf.get())
            ->getDecimalFormatSymbols();
    ASSERT_NE(lazy_syms, nullptr);
    EXPECT_EQ(IcuToUtf8(lazy_syms->getSymbol(
                  icu::DecimalFormatSymbols::kCurrencySymbol)),
              tc.expected_currency_symbol);
    EXPECT_EQ(IcuToUtf8(lazy_syms->getSymbol(
                  icu::DecimalFormatSymbols::kIntlCurrencySymbol)),
              tc.expected_intl_currency_symbol);
    EXPECT_EQ(IcuToUtf8(lazy_syms->getSymbol(
                  icu::DecimalFormatSymbols::kMonetarySeparatorSymbol)),
              tc.expected_monetary_decimal_separator);
    EXPECT_EQ(IcuToUtf8(lazy_syms->getSymbol(
                  icu::DecimalFormatSymbols::kMonetaryGroupingSeparatorSymbol)),
              tc.expected_monetary_grouping_separator);
    EXPECT_EQ(IcuToUtf8(lazy_syms->getPatternForCurrencySpacing(
                  UNUM_CURRENCY_MATCH, /*beforeCurrency=*/false, status)),
              tc.expected_currency_spacing_match);
    EXPECT_EQ(IcuToUtf8(lazy_syms->getPatternForCurrencySpacing(
                  UNUM_CURRENCY_INSERT, /*beforeCurrency=*/false, status)),
              tc.expected_currency_spacing_insert);
    ASSERT_TRUE(U_SUCCESS(status));

    // 2. Verify createCurrencyInstance against hardcoded expected string.
    std::unique_ptr<icu::NumberFormat> currency_nf(
        icu::NumberFormat::createCurrencyInstance(locale, status));
    ASSERT_TRUE(U_SUCCESS(status));
    auto* currency_df = static_cast<icu::DecimalFormat*>(currency_nf.get());
    ASSERT_NE(currency_df, nullptr);

    icu::UnicodeString currency_pattern;
    currency_df->toPattern(currency_pattern);

    icu::UnicodeString formatted_default_currency;
    currency_df->format(1234.56, formatted_default_currency);
    EXPECT_EQ(IcuToUtf8(formatted_default_currency),
              tc.expected_default_currency);

    // 3. Reconfigure a non-currency DecimalFormat (created via createInstance)
    // by applying the currency pattern; it must lazily load the locale's
    // default currency data and match the hardcoded expected string.
    std::unique_ptr<icu::NumberFormat> decimal_nf(
        icu::NumberFormat::createInstance(locale, status));
    ASSERT_TRUE(U_SUCCESS(status));
    auto* reconfigured_df = static_cast<icu::DecimalFormat*>(decimal_nf.get());
    ASSERT_NE(reconfigured_df, nullptr);
    reconfigured_df->applyPattern(currency_pattern, status);
    ASSERT_TRUE(U_SUCCESS(status));

    icu::UnicodeString actual_default_currency;
    reconfigured_df->format(1234.56, actual_default_currency);
    EXPECT_EQ(IcuToUtf8(actual_default_currency), tc.expected_default_currency);

    // 4. Reconfigure both a currency DecimalFormat and a percent DecimalFormat
    // with an explicit currency ("EUR") and currency pattern, and verify both
    // match the hardcoded expected string.
    currency_df->setCurrency(u"EUR", status);
    currency_df->applyPattern(currency_pattern, status);
    ASSERT_TRUE(U_SUCCESS(status));
    icu::UnicodeString formatted_eur;
    currency_df->format(1234.56, formatted_eur);
    EXPECT_EQ(IcuToUtf8(formatted_eur), tc.expected_eur_currency);

    std::unique_ptr<icu::NumberFormat> percent_nf(
        icu::NumberFormat::createPercentInstance(locale, status));
    ASSERT_TRUE(U_SUCCESS(status));
    auto* percent_df = static_cast<icu::DecimalFormat*>(percent_nf.get());
    ASSERT_NE(percent_df, nullptr);
    percent_df->setCurrency(u"EUR", status);
    percent_df->applyPattern(currency_pattern, status);
    ASSERT_TRUE(U_SUCCESS(status));

    icu::UnicodeString actual_eur;
    percent_df->format(1234.56, actual_eur);
    EXPECT_EQ(IcuToUtf8(actual_eur), tc.expected_eur_currency);

    // 5. Verify international currency pattern ("¤¤ #,##0.00") on a
    // non-currency DecimalFormat against the hardcoded expected string.
    std::unique_ptr<icu::NumberFormat> intl_decimal_nf(
        icu::NumberFormat::createInstance(locale, status));
    ASSERT_TRUE(U_SUCCESS(status));
    auto* intl_df = static_cast<icu::DecimalFormat*>(intl_decimal_nf.get());
    intl_df->applyPattern(u"¤¤ #,##0.00", status);
    ASSERT_TRUE(U_SUCCESS(status));

    icu::UnicodeString actual_intl;
    intl_df->format(1234.56, actual_intl);
    EXPECT_EQ(IcuToUtf8(actual_intl), tc.expected_intl_currency);

    // 6. Verify symmetric DecimalFormat::operator== between a currency
    // instance and a decimal instance when given the same pattern (ICU-12567).
    std::unique_ptr<icu::NumberFormat> eq_curr(
        icu::NumberFormat::createCurrencyInstance(locale, status));
    std::unique_ptr<icu::NumberFormat> eq_dec(
        icu::NumberFormat::createInstance(locale, status));
    ASSERT_TRUE(U_SUCCESS(status));
    static_cast<icu::DecimalFormat*>(eq_curr.get())
        ->applyPattern(u"0.00", status);
    static_cast<icu::DecimalFormat*>(eq_dec.get())
        ->applyPattern(u"0.00", status);
    ASSERT_TRUE(U_SUCCESS(status));
    EXPECT_TRUE(*eq_curr == *eq_dec);
    EXPECT_TRUE(*eq_dec == *eq_curr);
  }
}

// Validates subtle edge cases of lazy currency data loading in
// DecimalFormatSymbols and NumberFormatterImpl:
// - Custom currency / monetary symbols set via setSymbol or unum_setSymbol on a
//   non-currency formatter before currency data is loaded.
// - Currency-specific CLDR pattern and monetary separator overrides (e.g.
//   Portuguese Escudo "PTE" in pt-PT, which uses '$' as the decimal separator).
// - All UNumberFormatStyle currency styles (UNUM_CURRENCY, UNUM_CURRENCY_ISO,
//   UNUM_CURRENCY_PLURAL, UNUM_CURRENCY_ACCOUNTING, UNUM_CASH_CURRENCY,
//   UNUM_CURRENCY_STANDARD).
// - DecimalFormat copy/clone/assignment and parse/parseCurrency on non-currency
//   and reconfigured instances.
TEST(NumberFormattingTest, IcuLazyCurrencyLoadingEdgeCases) {
  // 1. Custom currency symbol and custom monetary separators set via
  // unum_setSymbol on a non-currency UNumberFormat (UNUM_DECIMAL) must not be
  // clobbered by lazy default currency loading.
  {
    UErrorCode status = U_ZERO_ERROR;
    icu::LocalUNumberFormatPointer fmt(
        unum_open(UNUM_DECIMAL, nullptr, 0, "en_US", nullptr, &status));
    ASSERT_TRUE(U_SUCCESS(status));

    // Customize currency symbol to "₿" before any currency data is loaded.
    unum_setSymbol(fmt.getAlias(), UNUM_CURRENCY_SYMBOL, u"\u20bf", -1,
                   &status);
    ASSERT_TRUE(U_SUCCESS(status));

    char16_t sym_buf[16];
    int32_t sym_len = unum_getSymbol(fmt.getAlias(), UNUM_CURRENCY_SYMBOL,
                                     sym_buf, 16, &status);
    ASSERT_TRUE(U_SUCCESS(status));
    EXPECT_EQ(UTF16ToUTF8(std::u16string_view(sym_buf, sym_len)),
              "\xe2\x82\xbf");

    // Monetary separators and intl currency symbol should still have their
    // locale defaults ("USD", ".", ",") loaded lazily without clobbering "₿".
    int32_t intl_len = unum_getSymbol(fmt.getAlias(), UNUM_INTL_CURRENCY_SYMBOL,
                                      sym_buf, 16, &status);
    ASSERT_TRUE(U_SUCCESS(status));
    EXPECT_EQ(UTF16ToUTF8(std::u16string_view(sym_buf, intl_len)), "USD");

    unum_applyPattern(fmt.getAlias(), false, u"\u00a4#,##0.00", -1, nullptr,
                      &status);
    ASSERT_TRUE(U_SUCCESS(status));

    char16_t formatted_buf[64];
    int32_t formatted_len = unum_formatDouble(
        fmt.getAlias(), 1234.56, formatted_buf, 64, nullptr, &status);
    ASSERT_TRUE(U_SUCCESS(status));
    EXPECT_EQ(UTF16ToUTF8(std::u16string_view(formatted_buf, formatted_len)),
              "\xe2\x82\xbf"
              "1,234.56");
  }

  // 2. Custom monetary decimal and grouping separators set on a non-currency
  // DecimalFormat's symbols copy before currency data is loaded.
  {
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::NumberFormat> nf(
        icu::NumberFormat::createInstance(icu::Locale("en", "US"), status));
    ASSERT_TRUE(U_SUCCESS(status));
    auto* df = static_cast<icu::DecimalFormat*>(nf.get());
    icu::DecimalFormatSymbols syms(*df->getDecimalFormatSymbols());
    syms.setSymbol(icu::DecimalFormatSymbols::kMonetarySeparatorSymbol, u"_");
    syms.setSymbol(icu::DecimalFormatSymbols::kMonetaryGroupingSeparatorSymbol,
                   u"/");
    df->setDecimalFormatSymbols(syms);
    df->applyPattern(u"\u00a4#,##0.00", status);
    ASSERT_TRUE(U_SUCCESS(status));

    icu::UnicodeString out;
    df->format(1234.56, out);
    // Default currency symbol for en-US ("$") is lazily loaded while custom
    // monetary separators ("_" and "/") are preserved.
    EXPECT_EQ(IcuToUtf8(out), "$1/234_56");
  }

  // 3. Currencies with custom CLDR pattern and monetary separators:
  // - Cape Verdean Escudo (CVE) in pt-CV defines Currencies/CVE[2] =
  //   { "#,##0.00 ¤", "$", " " } where '$' is the monetary decimal separator!
  // - Euro (EUR) in en-AT defines Currencies/EUR[2] =
  //   { "¤ #,##0.00", ",", "." } overriding English "." / "," separators.
  {
    UErrorCode status = U_ZERO_ERROR;
    icu::Locale pt_cv("pt", "CV");
    std::unique_ptr<icu::NumberFormat> cv_curr_nf(
        icu::NumberFormat::createCurrencyInstance(pt_cv, status));
    ASSERT_TRUE(U_SUCCESS(status));
    icu::UnicodeString cv_curr_out;
    cv_curr_nf->format(1234.56, cv_curr_out);
#if BUILDFLAG(IS_ANDROID) || (BUILDFLAG(IS_IOS) && !BUILDFLAG(USE_BLINK))
    // On Android and non-Blink iOS,
    // `third_party/icu/filters/{android,ios}.json` filters `curr_tree` to the
    // top 150 currencies, excluding `CVE`, so `pt_CV` falls back to `pt`
    // formatting ("1 234,56 CVE" and "," decimal separator).
    // TODO(crbug.com/568824848): Check whether this is intended.
    EXPECT_EQ(IcuToUtf8(cv_curr_out),
              "1\xc2\xa0"
              "234,56\xc2\xa0"
              "CVE");
#else
    EXPECT_EQ(IcuToUtf8(cv_curr_out),
              "1\xc2\xa0"
              "234$56\xc2\xa0\xe2\x80\x8b");
#endif

    // Verify lazy loading of monetary separators on a non-currency
    // DecimalFormat for pt-CV and en-AT.
    std::unique_ptr<icu::NumberFormat> cv_dec_nf(
        icu::NumberFormat::createInstance(pt_cv, status));
    ASSERT_TRUE(U_SUCCESS(status));
    const icu::DecimalFormatSymbols* cv_dec_syms =
        static_cast<icu::DecimalFormat*>(cv_dec_nf.get())
            ->getDecimalFormatSymbols();
    ASSERT_NE(cv_dec_syms, nullptr);
#if BUILDFLAG(IS_ANDROID) || (BUILDFLAG(IS_IOS) && !BUILDFLAG(USE_BLINK))
    EXPECT_EQ(IcuToUtf8(cv_dec_syms->getSymbol(
                  icu::DecimalFormatSymbols::kMonetarySeparatorSymbol)),
              ",");
#else
    EXPECT_EQ(IcuToUtf8(cv_dec_syms->getSymbol(
                  icu::DecimalFormatSymbols::kMonetarySeparatorSymbol)),
              "$");
#endif
    EXPECT_EQ(IcuToUtf8(cv_dec_syms->getSymbol(
                  icu::DecimalFormatSymbols::kMonetaryGroupingSeparatorSymbol)),
              "\xc2\xa0");

    icu::Locale en_at("en", "AT");
    std::unique_ptr<icu::NumberFormat> at_curr_nf(
        icu::NumberFormat::createCurrencyInstance(en_at, status));
    ASSERT_TRUE(U_SUCCESS(status));
    icu::UnicodeString at_curr_out;
    at_curr_nf->format(1234.56, at_curr_out);
    EXPECT_EQ(IcuToUtf8(at_curr_out),
              "\xe2\x82\xac\xc2\xa0"
              "1.234,56");

    std::unique_ptr<icu::NumberFormat> at_dec_nf(
        icu::NumberFormat::createInstance(en_at, status));
    ASSERT_TRUE(U_SUCCESS(status));
    const icu::DecimalFormatSymbols* at_dec_syms =
        static_cast<icu::DecimalFormat*>(at_dec_nf.get())
            ->getDecimalFormatSymbols();
    ASSERT_NE(at_dec_syms, nullptr);
    EXPECT_EQ(IcuToUtf8(at_dec_syms->getSymbol(
                  icu::DecimalFormatSymbols::kDecimalSeparatorSymbol)),
              ",");
    EXPECT_EQ(IcuToUtf8(at_dec_syms->getSymbol(
                  icu::DecimalFormatSymbols::kMonetarySeparatorSymbol)),
              ",");
    EXPECT_EQ(IcuToUtf8(at_dec_syms->getSymbol(
                  icu::DecimalFormatSymbols::kMonetaryGroupingSeparatorSymbol)),
              ".");

    // Also test UNumberFormatter with currency unit for en-AT.
    icu::LocalUNumberFormatterPointer uformatter(unumf_openForSkeletonAndLocale(
        u"currency/EUR", -1, en_at.getName(), &status));
    icu::LocalUFormattedNumberPointer uresult(unumf_openResult(&status));
    ASSERT_TRUE(U_SUCCESS(status));
    unumf_formatDouble(uformatter.getAlias(), 1234.56, uresult.getAlias(),
                       &status);
    ASSERT_TRUE(U_SUCCESS(status));
    int32_t buf_len =
        unumf_resultToString(uresult.getAlias(), nullptr, 0, &status);
    status = U_ZERO_ERROR;
    std::u16string uformatter_str(static_cast<size_t>(buf_len + 1), u'\0');
    unumf_resultToString(uresult.getAlias(), uformatter_str.data(), buf_len + 1,
                         &status);
    ASSERT_TRUE(U_SUCCESS(status));
    uformatter_str.resize(static_cast<size_t>(buf_len));
    EXPECT_EQ(UTF16ToUTF8(uformatter_str),
              "\xe2\x82\xac\xc2\xa0"
              "1.234,56");
  }

  // 4. All UNumberFormatStyle currency variants.
  {
    struct StyleCase {
      UNumberFormatStyle style;
      double amount;
      const char* expected_en_us;
      const char* expected_de_de;
    };
    static constexpr auto kStyleCases = std::to_array<StyleCase>({
        {UNUM_CURRENCY, 1234.56, "$1,234.56", "1.234,56\xc2\xa0\xe2\x82\xac"},
        {UNUM_CURRENCY_ISO, 1234.56,
         "USD\xc2\xa0"
         "1,234.56",
         "1.234,56\xc2\xa0"
         "EUR"},
        {UNUM_CURRENCY_PLURAL, 1234.56, "1,234.56 US dollars", "1.234,56 Euro"},
        {UNUM_CURRENCY_ACCOUNTING, -1234.56, "($1,234.56)",
         "-1.234,56\xc2\xa0\xe2\x82\xac"},
        {UNUM_CASH_CURRENCY, 1234.56, "$1,234.56",
         "1.234,56\xc2\xa0\xe2\x82\xac"},
        {UNUM_CURRENCY_STANDARD, 1234.56, "$1,234.56",
         "1.234,56\xc2\xa0\xe2\x82\xac"},
    });

    for (const auto& tc : kStyleCases) {
      UErrorCode status = U_ZERO_ERROR;
      std::unique_ptr<icu::NumberFormat> nf_en(
          icu::NumberFormat::createInstance(icu::Locale("en", "US"), tc.style,
                                            status));
      std::unique_ptr<icu::NumberFormat> nf_de(
          icu::NumberFormat::createInstance(icu::Locale("de", "DE"), tc.style,
                                            status));
      ASSERT_TRUE(U_SUCCESS(status));

      icu::UnicodeString out_en, out_de;
      nf_en->format(tc.amount, out_en);
      nf_de->format(tc.amount, out_de);
      EXPECT_EQ(IcuToUtf8(out_en), tc.expected_en_us);
      EXPECT_EQ(IcuToUtf8(out_de), tc.expected_de_de);
    }
  }

  // 5. Copy construction, assignment, clone(), and parse()/parseCurrency() on
  // non-currency and reconfigured DecimalFormat instances.
  {
    UErrorCode status = U_ZERO_ERROR;
    std::unique_ptr<icu::NumberFormat> base_nf(
        icu::NumberFormat::createInstance(icu::Locale("en", "US"), status));
    ASSERT_TRUE(U_SUCCESS(status));
    auto* base_df = static_cast<icu::DecimalFormat*>(base_nf.get());

    // Clone and copy before currency data is loaded on base_df.
    std::unique_ptr<icu::DecimalFormat> cloned_df(base_df->clone());
    icu::DecimalFormat copied_df(*base_df);
    icu::DecimalFormat assigned_df(u"0", status);
    ASSERT_TRUE(U_SUCCESS(status));
    assigned_df = *base_df;

    // Parse a number using the non-currency copy.
    icu::Formattable parsed_num;
    cloned_df->parse(u"12,345.67", parsed_num, status);
    ASSERT_TRUE(U_SUCCESS(status));
    EXPECT_DOUBLE_EQ(parsed_num.getDouble(), 12345.67);

    // Reconfigure the copy and assigned instances for currency and verify both
    // formatting and parseCurrency.
    copied_df.applyPattern(u"\u00a4#,##0.00", status);
    assigned_df.applyPattern(u"\u00a4#,##0.00", status);
    ASSERT_TRUE(U_SUCCESS(status));

    icu::UnicodeString copied_str, assigned_str;
    copied_df.format(1234.56, copied_str);
    assigned_df.format(1234.56, assigned_str);
    EXPECT_EQ(IcuToUtf8(copied_str), "$1,234.56");
    EXPECT_EQ(IcuToUtf8(assigned_str), "$1,234.56");

    icu::ParsePosition pos(0);
    std::unique_ptr<icu::CurrencyAmount> curr_amt(
        copied_df.parseCurrency(u"$1,234.56", pos));
    ASSERT_NE(curr_amt, nullptr);
    EXPECT_GT(pos.getIndex(), 0);
    EXPECT_DOUBLE_EQ(curr_amt->getNumber().getDouble(status), 1234.56);
    EXPECT_EQ(icu::UnicodeString(curr_amt->getISOCurrency()), u"USD");
  }
}

// Validates concurrent UResourceBundle open/lookup/close and NumberFormat
// creation across 20 locales and parent fallback chains against hardcoded
// expected results.
TEST(NumberFormattingTest, IcuResourceBundleAndNumberFormatConcurrency) {
  struct ExpectedLocaleCase {
    const char* locale_name;
    const char* expected_valid_locale;
    const char* expected_percent_42;
    const char* expected_number_12345;
    const char* expected_currency_99;
  };
  static constexpr auto kCases = std::to_array<ExpectedLocaleCase>({
      {"en-US", "en_US", "42%", "12,345.67", "$99.95"},
      {"en-GB", "en_GB", "42%", "12,345.67",
       "\xc2\xa3"
       "99.95"},
      {"de-DE", "de_DE", "42\xc2\xa0%", "12.345,67",
       "99,95\xc2\xa0\xe2\x82\xac"},
      {"de-CH", "de_CH", "42%", "12'345.67",
       "CHF\xc2\xa0"
       "99.95"},
      {"fr-FR", "fr_FR", "42\xc2\xa0%",
       "12\xe2\x80\xaf"
       "345,67",
       "99,95\xc2\xa0\xe2\x82\xac"},
      {"fr-CA", "fr_CA", "42\xc2\xa0%",
       "12\xc2\xa0"
       "345,67",
       "99,95\xc2\xa0$"},
      {"es-419", "es_419", "42%", "12,345.67",
       "XXX\xc2\xa0"
       "99.95"},
      {"pt-BR", "pt_BR", "42%", "12.345,67",
       "R$\xc2\xa0"
       "99,95"},
      {"zh-CN", "zh_Hans_CN", "42%", "12,345.67",
       "\xc2\xa5"
       "99.95"},
      {"zh-TW", "zh_Hant_TW", "42%", "12,345.67", "$99.95"},
      {"ja-JP", "ja_JP", "42%", "12,345.67",
       "\xef\xbf\xa5"
       "100"},
      {"ko-KR", "ko_KR", "42%", "12,345.67",
       "\xe2\x82\xa9"
       "100"},
      {"ru-RU", "ru_RU", "42\xc2\xa0%",
       "12\xc2\xa0"
       "345,67",
       "99,95\xc2\xa0\xe2\x82\xbd"},
      {"uk-UA", "uk_UA", "42%",
       "12\xc2\xa0"
       "345,67",
       "99,95\xc2\xa0\xd0\xb3\xd1\x80\xd0\xbd"},
      {"pl-PL", "pl_PL", "42%",
       "12\xc2\xa0"
       "345,67",
       "99,95\xc2\xa0z\xc5\x82"},
      {"tr-TR", "tr_TR", "%42", "12.345,67",
       "\xe2\x82\xba"
       "99,95"},
      {"ar-EG", "ar_EG", "\xd9\xa4\xd9\xa2\xd9\xaa\xd8\x9c",
       "\xd9\xa1\xd9\xa2\xd9\xac\xd9\xa3\xd9\xa4\xd9\xa5\xd9\xab\xd9\xa6\xd9"
       "\xa7",
       "\xe2\x80\x8f\xd9\xa9\xd9\xa9\xd9\xab\xd9\xa9\xd9\xa5\xc2\xa0\xd8\xac."
       "\xd9\x85.\xe2\x80\x8f"},
      {"fa-IR", "fa_IR", "\xdb\xb4\xdb\xb2\xd9\xaa",
       "\xdb\xb1\xdb\xb2\xd9\xac\xdb\xb3\xdb\xb4\xdb\xb5\xd9\xab\xdb\xb6\xdb"
       "\xb7",
       "\xe2\x80\x8e\xd8\xb1\xdb\x8c\xd8\xa7\xd9\x84\xc2\xa0\xdb\xb1\xdb\xb0"
       "\xdb\xb0"},
      {"hi-IN", "hi_IN", "42%", "12,345.67",
       "\xe2\x82\xb9"
       "99.95"},
#if BUILDFLAG(IS_IOS) && !BUILDFLAG(USE_BLINK)
      // On non-Blink iOS, `third_party/icu/filters/ios.json` abridges "bn"
      // locale data and excludes "bn" from `curr_tree`.
      // TODO(crbug.com/568824848): Check whether this fallback is intended.
      {"bn-BD", "bn_BD", "42%", "12,345.67",
       "BDT\xc2\xa0"
       "99.95"},
#else
      {"bn-BD", "bn_BD", "\xe0\xa7\xaa\xe0\xa7\xa8%",
       "\xe0\xa7\xa7\xe0\xa7\xa8,\xe0\xa7\xa9\xe0\xa7\xaa\xe0\xa7\xab.\xe0\xa7"
       "\xac\xe0\xa7\xad",
       "\xe0\xa7\xaf\xe0\xa7\xaf.\xe0\xa7\xaf\xe0\xa7\xab\xe0\xa7\xb3"},
#endif
  });

  std::vector<icu::Locale> locales(kCases.size());
  for (size_t i = 0; i < kCases.size(); ++i) {
    UErrorCode status = U_ZERO_ERROR;
    locales[i] = icu::Locale::forLanguageTag(kCases[i].locale_name, status);
    ASSERT_TRUE(U_SUCCESS(status));
  }

  class IcuConcurrencyWorker : public base::SimpleThread {
   public:
    IcuConcurrencyWorker(base::WaitableEvent* start_event,
                         const std::vector<icu::Locale>* locales,
                         size_t thread_idx)
        : SimpleThread("IcuConcurrencyWorker"),
          start_event_(start_event),
          locales_(locales),
          thread_idx_(thread_idx) {}

    void Run() override {
      start_event_->Wait();
      for (size_t iter = 0; iter < 15; ++iter) {
        for (size_t j = 0; j < kCases.size(); ++j) {
          const size_t idx = (j + thread_idx_ + iter) % kCases.size();
          const auto& tc = kCases[idx];
          const icu::Locale& locale = (*locales_)[idx];
          UErrorCode status = U_ZERO_ERROR;

          // Exercise UResourceBundle open, top-level fallback lookup along
          // parent chains, and close.
          icu::LocalUResourceBundlePointer rb(
              ures_open(nullptr, locale.getName(), &status));
          icu::LocalUResourceBundlePointer num_elements(
              ures_getByKey(rb.getAlias(), "NumberElements", nullptr, &status));
          const char* valid_loc = ures_getLocaleByType(
              num_elements.getAlias(), ULOC_VALID_LOCALE, &status);
          EXPECT_TRUE(U_SUCCESS(status));
          if (U_SUCCESS(status) && valid_loc != nullptr) {
            EXPECT_STREQ(valid_loc, tc.expected_valid_locale);
          }

          // Exercise NumberFormat creation across styles and compare against
          // hardcoded expected strings.
          std::unique_ptr<icu::NumberFormat> pf(
              icu::NumberFormat::createPercentInstance(locale, status));
          std::unique_ptr<icu::NumberFormat> nf(
              icu::NumberFormat::createInstance(locale, status));
          std::unique_ptr<icu::NumberFormat> cf(
              icu::NumberFormat::createCurrencyInstance(locale, status));
          EXPECT_TRUE(U_SUCCESS(status));
          if (U_SUCCESS(status)) {
            icu::UnicodeString s_pf, s_nf, s_cf;
            pf->format(0.42, s_pf);
            nf->format(12345.67, s_nf);
            cf->format(99.95, s_cf);
            EXPECT_EQ(IcuToUtf8(s_pf), tc.expected_percent_42);
            EXPECT_EQ(IcuToUtf8(s_nf), tc.expected_number_12345);
            EXPECT_EQ(IcuToUtf8(s_cf), tc.expected_currency_99);
          }
        }
      }
    }

   private:
    const raw_ptr<base::WaitableEvent> start_event_;
    const raw_ptr<const std::vector<icu::Locale>> locales_;
    const size_t thread_idx_;
  };

  base::WaitableEvent start_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  std::vector<std::unique_ptr<IcuConcurrencyWorker>> workers;
  for (size_t t = 0; t < 16; ++t) {
    workers
        .emplace_back(
            std::make_unique<IcuConcurrencyWorker>(&start_event, &locales, t))
        ->Start();
  }
  start_event.Signal();
  for (auto& w : workers) {
    w->Join();
  }
}

// Validates UResourceBundle edge cases affected by ParentChain and lock-free
// refcounting:
// - %%ALIAS locale bundles (iw_IL -> he_IL, in_ID -> id_ID, tl_PH -> fil_PH)
// - Explicit %%Parent overrides (es_AR -> es_419 -> es -> root,
//   en_AU -> en_001 -> en -> root)
// - %%ParentIsRoot (zh_Hant_TW -> zh_Hant -> root, skipping zh)
// - Nonexistent subtags that chop back to parent (en_US_FOOBAR -> en_US)
// - ures_openDirect (no fallback) on root vs nonexistent bundle.
TEST(NumberFormattingTest, IcuResourceBundleFallbackAndAliasEdgeCases) {
  struct BundleCase {
    const char* input_locale;
    const char* expected_valid_locale;
    const char* expected_actual_number_elements_locale;
    const char* expected_number_12345;
    const char* expected_currency_99;
  };
  static constexpr auto kCases = std::to_array<BundleCase>({
      // %%ALIAS locales
      {"iw_IL", "he_IL", "he", "12,345.67",
       "\xe2\x80\x8f"
       "99.95\xc2\xa0\xe2\x80\x8f\xe2\x82\xaa"},
      {"in_ID", "id_ID", "id", "12.345,67",
       "Rp\xc2\xa0"
       "100"},
      {"tl_PH", "fil_PH", "fil", "12,345.67",
       "\xe2\x82\xb1"
       "99.95"},
      // %%Parent chains (es_AR -> es_419 -> es -> root)
      {"es_AR", "es_AR", "es_AR", "12.345,67",
       "$\xc2\xa0"
       "99,95"},
      {"es_MX", "es_MX", "es_MX", "12,345.67", "$99.95"},
      // %%Parent chains (en_AU -> en_001 -> en -> root)
      {"en_AU", "en_AU", "en_AU", "12,345.67", "$99.95"},
      {"en_IE", "en_IE", "en", "12,345.67",
       "\xe2\x82\xac"
       "99.95"},
      // %%ParentIsRoot (zh_Hant_TW -> zh_Hant -> root)
      {"zh_Hant_TW", "zh_Hant_TW", "zh_Hant", "12,345.67", "$99.95"},
      {"zh_Hant_HK", "zh_Hant_HK", "zh_Hant_HK", "12,345.67", "HK$99.95"},
      // Nonexistent variant subtag that chops back to en_US
      {"en_US_FOOBAR", "en_US", "en", "12,345.67", "$99.95"},
  });

  for (const auto& tc : kCases) {
    SCOPED_TRACE(tc.input_locale);
    UErrorCode status = U_ZERO_ERROR;
    icu::LocalUResourceBundlePointer rb(
        ures_open(nullptr, tc.input_locale, &status));
    ASSERT_TRUE(U_SUCCESS(status));

    const char* valid_loc =
        ures_getLocaleByType(rb.getAlias(), ULOC_VALID_LOCALE, &status);
    ASSERT_TRUE(U_SUCCESS(status));
    EXPECT_STREQ(valid_loc, tc.expected_valid_locale);

    icu::LocalUResourceBundlePointer num_elements(
        ures_getByKey(rb.getAlias(), "NumberElements", nullptr, &status));
    ASSERT_TRUE(U_SUCCESS(status));
    const char* actual_loc = ures_getLocaleByType(num_elements.getAlias(),
                                                  ULOC_ACTUAL_LOCALE, &status);
    ASSERT_TRUE(U_SUCCESS(status));
    EXPECT_STREQ(actual_loc, tc.expected_actual_number_elements_locale);

    icu::Locale loc(tc.input_locale);
    std::unique_ptr<icu::NumberFormat> nf(
        icu::NumberFormat::createInstance(loc, status));
    std::unique_ptr<icu::NumberFormat> cf(
        icu::NumberFormat::createCurrencyInstance(loc, status));
    ASSERT_TRUE(U_SUCCESS(status));

    icu::UnicodeString s_nf, s_cf;
    nf->format(12345.67, s_nf);
    cf->format(99.95, s_cf);
    EXPECT_EQ(IcuToUtf8(s_nf), tc.expected_number_12345);
    EXPECT_EQ(IcuToUtf8(s_cf), tc.expected_currency_99);
  }

  // Test ures_openDirect on "root" (succeeds) and on a nonexistent locale
  // "zz_ZZ" (fails with U_MISSING_RESOURCE_ERROR without falling back).
  {
    UErrorCode status = U_ZERO_ERROR;
    icu::LocalUResourceBundlePointer root_rb(
        ures_openDirect(nullptr, "root", &status));
    ASSERT_TRUE(U_SUCCESS(status));
    EXPECT_STREQ(
        ures_getLocaleByType(root_rb.getAlias(), ULOC_ACTUAL_LOCALE, &status),
        "root");

    UErrorCode missing_status = U_ZERO_ERROR;
    icu::LocalUResourceBundlePointer missing_rb(
        ures_openDirect(nullptr, "zz_ZZ_NONEXISTENT", &missing_status));
    EXPECT_EQ(missing_status, U_MISSING_RESOURCE_ERROR);
  }
}

}  // namespace
}  // namespace base
