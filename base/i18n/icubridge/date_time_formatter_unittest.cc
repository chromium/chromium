// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icubridge/date_time_formatter.h"

#include <optional>
#include <vector>

#include "base/i18n/icu_util.h"
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/i18n/tags.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace base::i18n {

class DateTimeFormatterTest : public testing::Test {
 public:
  void SetUp() override {
    base::i18n::InitializeICU();
  }

 protected:
  // Force UTC timezone for predictable results.
  base::test::ScopedRestoreDefaultTimezone gmt_timezone_{"GMT"};

  struct ExactMatchTestEntry {
    std::string_view description;
    std::string_view value;
    std::vector<DateTimeFormatterOptions> options;
    struct Expectation {
      std::string_view locale;
      std::vector<std::u16string_view> expected;
    };
    std::vector<Expectation> expectations;
  };

  void RunExactMatchTests(const std::vector<ExactMatchTestEntry>& test_data) {
    const IcuBridge::DateTimeFormatter& formatter =
        IcuBridge::GetInstance().date_time_formatter();

    for (const auto& entry : test_data) {
      base::Time time;
      ASSERT_TRUE(
          base::Time::FromUTCString(std::string(entry.value).c_str(), &time))
          << "Failed to parse time: " << entry.value;

      for (const auto& expectation : entry.expectations) {
        std::string locale_str(expectation.locale);
#if BUILDFLAG(IS_IOS) || BUILDFLAG(IS_ANDROID)
        // iOS and some Android ICU data is missing some calendar-specific names
        // for Persian and Japanese calendars, leading to incorrect formatting
        // or empty era names.
        if (locale_str.find("fa") != std::string::npos ||
            locale_str.find("japanese") != std::string::npos) {
          continue;
        }
#endif
#if BUILDFLAG(IS_CHROMEOS)
        // ChromeOS ICU data for Urdu is currently causing test failures.
        if (locale_str == "ur") {
          continue;
        }
#endif
#if BUILDFLAG(IS_IOS)
        // iOS ICU data is missing some locale-specific formatting for these
        // locales.
        static constexpr std::string_view kIosSkipLocales[] = {
            "af", "bn", "et", "gu", "kn", "ml", "mr", "ms", "ta", "te", "ur"};
        bool skip = false;
        for (const auto& skip_locale : kIosSkipLocales) {
          if (locale_str == skip_locale) {
            skip = true;
            break;
          }
        }
        if (skip) {
          continue;
        }
#endif
        auto language_tag =
            LanguageTagConverter::GetInstance().FromString(locale_str);
        ASSERT_TRUE(language_tag.has_value())
            << "Invalid locale: " << locale_str;

        ASSERT_EQ(entry.options.size(), expectation.expected.size())
            << "Mismatch between number of options and expectations for "
               "locale: "
            << locale_str << " in " << entry.description;

        for (size_t i = 0; i < entry.options.size(); ++i) {
          std::u16string actual =
              formatter.Format(time, *language_tag, entry.options[i]);
          std::u16string_view expected_str = expectation.expected[i];
          EXPECT_THAT(actual, ::testing::Eq(expected_str))
              << "Failed: " << entry.description << " Locale: " << locale_str
              << " Option index: " << i
              << " Actual: " << base::UTF16ToUTF8(actual)
              << " Expected: " << base::UTF16ToUTF8(expected_str);
        }
      }
    }
  }
};

TEST_F(DateTimeFormatterTest, YearTests) {
  RunExactMatchTests({
      {
          .description = "Exact match for: y => y (long/medium)",
          .value = "2020-01-07 08:25:07",
          .options = {datetime_options::Y::Long(),
                      datetime_options::Y::Medium()},
          .expectations = {{"en", {u"2020", u"2020"}},
                           {"en-GB", {u"2020", u"2020"}},
                           {"ja", {u"2020年", u"2020年"}},
                           {"de", {u"2020", u"2020"}},
                           {"fa", {u"۱۳۹۸", u"۱۳۹۸"}}},
      },
      {
          .description = "Exact match for: y => y (short)",
          .value = "2020-01-07 08:25:07",
          .options = {datetime_options::Y::Short()},
          .expectations = {{"en", {u"20"}},
                           {"ja", {u"20年"}},
                           {"pt", {u"20"}},
                           {"fa", {u"۹۸"}}},
      },
  });
}

TEST_F(DateTimeFormatterTest, MonthTests) {
  RunExactMatchTests({
      {
          .description = "Month: Short (numeric)",
          .value = "2020-01-07 08:25:07",
          .options = {datetime_options::M::Short()},
          .expectations = {{"en", {u"1"}}, {"ja", {u"1月"}}, {"de", {u"1"}}},
      },
      {
          .description = "Month: Medium (abbreviated)",
          .value = "2020-01-07 08:25:07",
          .options = {datetime_options::M::Medium()},
          .expectations = {{"en", {u"Jan"}},
                           {"ja", {u"1月"}},
                           {"de", {u"Jan"}},
                           {"fa", {u"دی"}}},
      },
      {
          .description = "Month: Long (full)",
          .value = "2020-01-07 08:25:07",
          .options = {datetime_options::M::Long()},
          .expectations = {{"en", {u"January"}},
                           {"ja", {u"1月"}},
                           {"de", {u"Januar"}},
                           {"fa", {u"دی"}}},
      },
  });
}

TEST_F(DateTimeFormatterTest, DayAndWeekdayTests) {
  RunExactMatchTests({
      {
          .description = "Day and Weekday: Medium",
          .value = "2020-01-07 08:25:07",
          .options = {datetime_options::DE::Medium()},
          .expectations = {{"en", {u"7 Tue"}},
                           {"en-GB", {u"Tue 7"}},
                           {"ja", {u"7日(火)"}},
                           {"de", {u"Di., 7."}},
                           {"fa", {u"سه‌شنبه ۱۷م"}}},
      },
      {
          .description = "Day and Weekday: Long",
          .value = "2020-01-07 08:25:07",
          .options = {datetime_options::DE::Long()},
          .expectations = {{"en", {u"07 Tuesday"}},
                           {"ja", {u"07日火曜日"}},
                           {"de", {u"Dienstag, 07."}},
                           {"fa", {u"سه‌شنبه ۱۷م"}}},
      },
      {
          .description = "Day: Medium",
          .value = "2020-01-07 08:25:07",
          .options = {datetime_options::D::Short()},
          .expectations = {{"en", {u"7"}},
                           {"ja", {u"7日"}},
                           {"de", {u"7"}},
                           {"fa", {u"۱۷"}}},
      },
      {
          .description = "Day: Long",
          .value = "2020-01-07 08:25:07",
          .options = {datetime_options::D::Long()},
          .expectations = {{"en", {u"07"}},
                           {"ja", {u"07日"}},
                           {"de", {u"07"}},
                           {"fa", {u"۱۷"}}},
      },
      {
          .description = "Exact match for: E => ccc",
          .value = "2020-01-07 08:25:07",
          .options = {datetime_options::E::Medium()},
          .expectations = {{"en", {u"Tue"}},
                           {"en-GB", {u"Tue"}},
                           {"ja", {u"火"}},
                           {"de", {u"Di"}},
                           {"fa", {u"سه‌شنبه"}}},
      },
  });
}

TEST_F(DateTimeFormatterTest, CombinationTests) {
  RunExactMatchTests({
      {
          .description = "Exact match for: YMD::Medium()",
          .value = "2020-01-07 08:25:07",
          .options = {datetime_options::YMD::Medium()},
          .expectations = {{"en", {u"Jan 7, 2020"}},
                           {"en-GB", {u"7 Jan 2020"}},
                           {"ja", {u"2020/01/07"}},
                           {"de", {u"07.01.2020"}},
                           {"fa", {u"۱۷ دی ۱۳۹۸"}}},
      },
      {
          .description = "Exact match for: YMD::Short()",
          .value = "2020-01-07 08:25:07",
          .options = {datetime_options::YMD::Short()},
          .expectations = {{"en", {u"1/7/20"}},
                           {"en-GB", {u"07/01/2020"}},
                           {"ja", {u"2020/01/07"}},
                           {"de", {u"07.01.20"}},
                           {"fa", {u"۱۳۹۸/۱۰/۱۷"}}},
      },
      {
          .description = "Exact match for: YMDE::Medium()",
          .value = "2020-01-07 08:25:07",
          .options = {datetime_options::YMDE::Medium()},
          .expectations = {{"en", {u"Tue, Jan 7, 2020"}},
                           {"en-GB", {u"Tue, 7 Jan 2020"}},
                           {"ja", {u"2020/01/07(火)"}},
                           {"de", {u"Di., 07.01.2020"}},
                           // ICU4X outputs "سه‌شنبه ۱۷ دی ۱۳۹۸"
                           {"fa", {u"۱۳۹۸ دی ۱۷, سه‌شنبه"}}},
      },
      {
          .description = "Exact match for: YM::Long()",
          .value = "2020-01-07 08:25:07",
          .options = {datetime_options::YM::Long()},
          .expectations = {{"en", {u"January 2020"}},
                           {"en-GB", {u"January 2020"}},
                           {"ja", {u"2020年1月"}},
                           {"de", {u"Januar 2020"}},
                           {"fa", {u"۱۳۹۸ دی"}}},
      },
      {
          .description = "Exact match for: MD::Short()",
          .value = "2020-01-07 08:25:07",
          .options = {datetime_options::MD::Short()},
          .expectations = {{"en", {u"1/7"}},
                           {"en-GB", {u"07/01"}},
                           {"ja", {u"01/07"}},
                           {"de", {u"07.01."}},
                           {"fa", {u"۱۰/۱۷"}}},
      },
      {
          .description = "Exact match for: MDE::Short()",
          .value = "2020-01-07 08:25:07",
          .options = {datetime_options::MDE::Short()},
          .expectations =
              {{"en", {u"Tue, 1/7"}},
               {"en-GB", {u"Tue 07/01"}},
               {"ja", {u"01/07(火)"}},
               {"de", {u"Di., 07.01."}},
               // Also, ICU4X produces this output in gregorian calendar.
               {"fa", {u"سه‌شنبه ۱۰/۱۷"}}},
      },
  });
}

TEST_F(DateTimeFormatterTest, JapaneseCalendarTests) {
  RunExactMatchTests({
      {
          .description = "Japanese Era: Reiwa 2",
          .value = "2020-02-20 00:12:00",
          .options = {datetime_options::Y::Medium().with_year_style(
              DateTimeFormatterOptions::YearStyle::kWithEra)},
          .expectations = {{"en-u-ca-japanese", {u"2 Reiwa"}},
                           {"ja-u-ca-japanese", {u"令和2年"}}},
      },
      {
          .description = "Japanese Era: Heisei 22",
          .value = "2010-02-20 00:12:00",
          .options = {datetime_options::Y::Medium().with_year_style(
              DateTimeFormatterOptions::YearStyle::kWithEra)},
          .expectations = {{"en-u-ca-japanese", {u"22 Heisei"}},
                           {"ja-u-ca-japanese", {u"平成22年"}}},
      },
  });
}

TEST_F(DateTimeFormatterTest, TimeAndEraTests) {
  RunExactMatchTests({
      {
          .description = "Full Date Time with Era",
          .value = "2020-01-21 08:25:07",
          .options = {datetime_options::YMDET::Long()
                          .with_year_style(
                              DateTimeFormatterOptions::YearStyle::kWithEra)
                          .with_time_precision(
                              DateTimeFormatterOptions::TimePrecision::kSecond)
                          .with_hour_clock_type(base::k24HourClock)},
          .expectations =
              {{"en-u-hc-h23", {u"Tuesday, January 21, 2020 AD at 08:25:07"}},
               {"en-GB-u-hc-h23", {u"Tuesday, 21 January 2020 AD at 08:25:07"}},
               {"de-u-hc-h23",
                {u"Dienstag, 21. Januar 2020 n. Chr. um 08:25:07"}},
               // ICU4X outputs in gregorian calendar: سه‌شنبه ۲۱
               // ژانویهٔ ۲۰۲۰ م. ساعت ۸:۲۵:۰۷
               {"fa-u-hc-h23",
                {u"سه‌شنبه ۱ بهمن ۱۳۹۸ ه‍.ش. ساعت "
                 u"۸:۲۵:۰۷"}}},
      },
      {
          .description = "Time Only with Second",
          .value = "2022-05-03 14:15:07.123",
          .options = {datetime_options::T::Short()
                          .with_time_precision(
                              DateTimeFormatterOptions::TimePrecision::kSecond)
                          .with_hour_clock_type(base::k24HourClock)},
          .expectations =
              {
                  {"en-u-hc-h23", {u"14:15:07"}},
                  {"en-GB-u-hc-h23", {u"14:15:07"}},
                  {"ja-u-hc-h23", {u"14:15:07"}},
                  {"de-u-hc-h23", {u"14:15:07"}},
                  {"fa-u-hc-h23", {u"۱۴:۱۵:۰۷"}},
                  {"fa", {u"۱۴:۱۵:۰۷"}},
              },
      },
  });
}

TEST_F(DateTimeFormatterTest, FormatShortDate) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result = formatter.Format(time, GetKnownLanguageTag("en-US"),
                                           datetime_options::YMDT::Short());

  EXPECT_FALSE(result.empty());
  // US short date/time might be "5/25/26, 10:30 AM"
  EXPECT_NE(result.find(u"25"), std::u16string::npos);
  EXPECT_NE(result.find(u"10:30"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatYMD) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result = formatter.Format(time, GetKnownLanguageTag("en-US"),
                                           datetime_options::YMD::Medium());

  EXPECT_FALSE(result.empty());
  EXPECT_NE(result.find(u"25"), std::u16string::npos);
  EXPECT_NE(result.find(u"2026"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatY) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result = formatter.Format(time, GetKnownLanguageTag("en-US"),
                                           datetime_options::Y::Medium());

  EXPECT_EQ(result, u"2026");
}

TEST_F(DateTimeFormatterTest, FormatE) {
  base::Time time;
  // 2026-05-25 is a Monday
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result = formatter.Format(time, GetKnownLanguageTag("en-US"),
                                           datetime_options::E::Medium());

  EXPECT_EQ(result, u"Mon");
}

TEST_F(DateTimeFormatterTest, FormatWithPrecision) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result =
      formatter.Format(time, GetKnownLanguageTag("en-US"),
                       datetime_options::YMDT::Medium().with_time_precision(
                           DateTimeFormatterOptions::TimePrecision::kMinute));

  EXPECT_FALSE(result.empty());
  EXPECT_NE(result.find(u"10:30"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatWithEra) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result =
      formatter.Format(time, GetKnownLanguageTag("en-US"),
                       datetime_options::Y::Medium().with_year_style(
                           DateTimeFormatterOptions::YearStyle::kWithEra));

  EXPECT_NE(result.find(u"AD"), std::u16string::npos);
}

// New ComponentBagType Tests
TEST_F(DateTimeFormatterTest, FormatD) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  EXPECT_EQ(formatter.Format(time, GetKnownLanguageTag("en-US"),
                             datetime_options::D::Medium()),
            u"25");
  EXPECT_EQ(formatter.Format(time, GetKnownLanguageTag("en-US"),
                             datetime_options::D::Short()),
            u"25");
  EXPECT_EQ(formatter.Format(time, GetKnownLanguageTag("en-US"),
                             datetime_options::D::Long()),
            u"25");
}

TEST_F(DateTimeFormatterTest, FormatDE) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result = formatter.Format(time, GetKnownLanguageTag("en-US"),
                                           datetime_options::DE::Medium());
  EXPECT_NE(result.find(u"Mon"), std::u16string::npos);
  EXPECT_NE(result.find(u"25"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatDET) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result = formatter.Format(time, GetKnownLanguageTag("en-US"),
                                           datetime_options::DET::Medium());
  EXPECT_NE(result.find(u"Mon"), std::u16string::npos);
  EXPECT_NE(result.find(u"25"), std::u16string::npos);
  EXPECT_NE(result.find(u"10:30:00"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatDT) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result = formatter.Format(time, GetKnownLanguageTag("en-US"),
                                           datetime_options::DT::Medium());
  EXPECT_NE(result.find(u"25"), std::u16string::npos);
  EXPECT_NE(result.find(u"10:30:00"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatET) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result = formatter.Format(time, GetKnownLanguageTag("en-US"),
                                           datetime_options::ET::Medium());
  EXPECT_NE(result.find(u"Mon"), std::u16string::npos);
  EXPECT_NE(result.find(u"10:30:00"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatM) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  EXPECT_EQ(formatter.Format(time, GetKnownLanguageTag("en-US"),
                             datetime_options::M::Medium()),
            u"May");
}

TEST_F(DateTimeFormatterTest, FormatMD) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  EXPECT_EQ(formatter.Format(time, GetKnownLanguageTag("en-US"),
                             datetime_options::MD::Medium()),
            u"May 25");
}

TEST_F(DateTimeFormatterTest, FormatMDE) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result = formatter.Format(time, GetKnownLanguageTag("en-US"),
                                           datetime_options::MDE::Medium());
  EXPECT_NE(result.find(u"Mon"), std::u16string::npos);
  EXPECT_NE(result.find(u"May"), std::u16string::npos);
  EXPECT_NE(result.find(u"25"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatMDET) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result = formatter.Format(time, GetKnownLanguageTag("en-US"),
                                           datetime_options::MDET::Medium());
  EXPECT_NE(result.find(u"Mon"), std::u16string::npos);
  EXPECT_NE(result.find(u"May"), std::u16string::npos);
  EXPECT_NE(result.find(u"25"), std::u16string::npos);
  EXPECT_NE(result.find(u"10:30:00"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatMDT) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result = formatter.Format(time, GetKnownLanguageTag("en-US"),
                                           datetime_options::MDT::Medium());
  EXPECT_NE(result.find(u"May"), std::u16string::npos);
  EXPECT_NE(result.find(u"25"), std::u16string::npos);
  EXPECT_NE(result.find(u"10:30:00"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatT) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  EXPECT_NE(formatter
                .Format(time, GetKnownLanguageTag("en-US"),
                        datetime_options::T::Medium())
                .find(u"10:30:00"),
            std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatYM) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  EXPECT_EQ(formatter.Format(time, GetKnownLanguageTag("en-US"),
                             datetime_options::YM::Medium()),
            u"May 2026");
}

TEST_F(DateTimeFormatterTest, FormatYMDE) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result = formatter.Format(time, GetKnownLanguageTag("en-US"),
                                           datetime_options::YMDE::Medium());
  EXPECT_NE(result.find(u"Mon"), std::u16string::npos);
  EXPECT_NE(result.find(u"25"), std::u16string::npos);
  EXPECT_NE(result.find(u"2026"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatYMDET) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result = formatter.Format(time, GetKnownLanguageTag("en-US"),
                                           datetime_options::YMDET::Medium());
  EXPECT_NE(result.find(u"Mon"), std::u16string::npos);
  EXPECT_NE(result.find(u"25"), std::u16string::npos);
  EXPECT_NE(result.find(u"2026"), std::u16string::npos);
  EXPECT_NE(result.find(u"10:30:00"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatYMDT) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result = formatter.Format(time, GetKnownLanguageTag("en-US"),
                                           datetime_options::YMDT::Medium());
  EXPECT_NE(result.find(u"25"), std::u16string::npos);
  EXPECT_NE(result.find(u"2026"), std::u16string::npos);
  EXPECT_NE(result.find(u"10:30:00"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, HourClockType) {
  base::Time time;
  // 10:30 PM is 22:30
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 22:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();

  // 12-hour clock
  std::u16string result12 = formatter.Format(
      time, GetKnownLanguageTag("en-US"),
      datetime_options::T::Short().with_hour_clock_type(base::k12HourClock));
  EXPECT_NE(result12.find(u"10:30"), std::u16string::npos);
  EXPECT_NE(result12.find(u"PM"), std::u16string::npos);

  // 24-hour clock
  std::u16string result24 = formatter.Format(
      time, GetKnownLanguageTag("en-US"),
      datetime_options::T::Short().with_hour_clock_type(base::k24HourClock));
  EXPECT_NE(result24.find(u"22:30"), std::u16string::npos);
  EXPECT_EQ(result24.find(u"PM"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, AmPmClockType) {
  base::Time time;
  // 10:30 PM
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 22:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();

  // 12-hour clock, keep AM/PM
  std::u16string result_keep =
      formatter.Format(time, GetKnownLanguageTag("en-US"),
                       datetime_options::T::Short()
                           .with_hour_clock_type(base::k12HourClock)
                           .with_am_pm_clock_type(base::kKeepAmPm));
  EXPECT_NE(result_keep.find(u"10:30"), std::u16string::npos);
  EXPECT_NE(result_keep.find(u"PM"), std::u16string::npos);

  // 12-hour clock, drop AM/PM
  std::u16string result_drop =
      formatter.Format(time, GetKnownLanguageTag("en-US"),
                       datetime_options::T::Short()
                           .with_hour_clock_type(base::k12HourClock)
                           .with_am_pm_clock_type(base::kDropAmPm));
  EXPECT_NE(result_drop.find(u"10:30"), std::u16string::npos);
  EXPECT_EQ(result_drop.find(u"PM"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, HourClockTypeWithLength) {
  base::Time time;
  // 10:30 PM is 22:30
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 22:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();

  DateTimeFormatterOptions options;
  options.format_identifier = DateTimeFormatterOptions::FormatIdentifier::kT;
  options.length = DateTimeFormatterOptions::ItemLength::kShort;

  // 12-hour clock
  options.hour_clock_type = base::k12HourClock;
  std::u16string result12 =
      formatter.Format(time, GetKnownLanguageTag("en-US"), options);
  EXPECT_NE(result12.find(u"10:30"), std::u16string::npos);
  EXPECT_NE(result12.find(u"PM"), std::u16string::npos);

  // 24-hour clock
  options.hour_clock_type = base::k24HourClock;
  std::u16string result24 =
      formatter.Format(time, GetKnownLanguageTag("en-US"), options);

  EXPECT_NE(result24.find(u"22:30"), std::u16string::npos);
  EXPECT_EQ(result24.find(u"PM"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, MultiLocaleFormat) {
  base::Time time;
  // 2011-04-30 is a Saturday
  ASSERT_TRUE(base::Time::FromUTCString("2011-04-30 15:42:07", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();

  {
    EXPECT_EQ(u"4/30/11", formatter.Format(time, GetKnownLanguageTag("en-US"),
                                           datetime_options::YMD::Short()));
    EXPECT_EQ(u"Apr 30, 2011",
              formatter.Format(time, GetKnownLanguageTag("en-US"),
                               datetime_options::YMD::Medium()));
    // "3:42:07 PM" (might have narrow non-breaking space)
    std::u16string result_t = formatter.Format(
        time, GetKnownLanguageTag("en-US"), datetime_options::T::Medium());
    EXPECT_NE(result_t.find(u"3:42:07"), std::u16string::npos);
    EXPECT_NE(result_t.find(u"PM"), std::u16string::npos);
  }
  {
    EXPECT_EQ(u"30/04/2011",
              formatter.Format(time, GetKnownLanguageTag("en-GB"),
                               datetime_options::YMD::Short()));
    EXPECT_EQ(u"30 Apr 2011",
              formatter.Format(time, GetKnownLanguageTag("en-GB"),
                               datetime_options::YMD::Medium()));
    EXPECT_EQ(u"30 April 2011",
              formatter.Format(time, GetKnownLanguageTag("en-GB"),
                               datetime_options::YMD::Long()));
    EXPECT_EQ(u"15:42:07", formatter.Format(time, GetKnownLanguageTag("en-GB"),
                                            datetime_options::T::Medium()));
  }
  {
    EXPECT_EQ(u"2011/04/30", formatter.Format(time, GetKnownLanguageTag("ja"),
                                              datetime_options::YMD::Short()));
    EXPECT_EQ(u"2011/04/30", formatter.Format(time, GetKnownLanguageTag("ja"),
                                              datetime_options::YMD::Medium()));
    EXPECT_EQ(u"2011年4月30日",
              formatter.Format(time, GetKnownLanguageTag("ja"),
                               datetime_options::YMD::Long()));
    EXPECT_EQ(u"15:42:07", formatter.Format(time, GetKnownLanguageTag("ja"),
                                            datetime_options::T::Medium()));
  }
  {
    EXPECT_EQ(u"30.04.11", formatter.Format(time, GetKnownLanguageTag("de-DE"),
                                            datetime_options::YMD::Short()));
    EXPECT_EQ(u"30.04.2011",
              formatter.Format(time, GetKnownLanguageTag("de-DE"),
                               datetime_options::YMD::Medium()));
    EXPECT_EQ(u"30. April 2011",
              formatter.Format(time, GetKnownLanguageTag("de-DE"),
                               datetime_options::YMD::Long()));
    EXPECT_EQ(u"Samstag, 30. April 2011",
              formatter.Format(time, GetKnownLanguageTag("de-DE"),
                               datetime_options::YMDE::Long()));
    EXPECT_EQ(u"15:42:07", formatter.Format(time, GetKnownLanguageTag("de-DE"),
                                            datetime_options::T::Medium()));
  }
}

TEST_F(DateTimeFormatterTest, SubsecondPrecision) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00.987", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();

  // kSubsecond_3 should show .987
  std::u16string result = formatter.Format(
      time, GetKnownLanguageTag("en-US"),
      datetime_options::T::Medium().with_time_precision(
          DateTimeFormatterOptions::TimePrecision::kSubsecond_3));
  EXPECT_NE(result.find(u"10:30:00.987"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatShortSpecificTimeZone) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();

  std::u16string result = formatter.Format(
      time, GetKnownLanguageTag("en-US"),
      datetime_options::YMDT::Medium()
          .with_time_zone(
              base::i18n::TimeZone::FromString("America/Los_Angeles"))
          .with_time_zone_style(
              DateTimeFormatterOptions::TimeZoneStyle::kShortSpecific));
  EXPECT_NE(result.find(u"PDT"), std::u16string::npos)
      << base::UTF16ToUTF8(result);
}

TEST_F(DateTimeFormatterTest, FormatLongSpecificTimeZone) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();

  std::u16string result = formatter.Format(
      time, GetKnownLanguageTag("en-US"),
      datetime_options::YMDT::Medium()
          .with_time_zone(
              base::i18n::TimeZone::FromString("America/Los_Angeles"))
          .with_time_zone_style(
              DateTimeFormatterOptions::TimeZoneStyle::kLongSpecific));
  EXPECT_NE(result.find(u"Pacific Daylight Time"), std::u16string::npos)
      << base::UTF16ToUTF8(result);
}

TEST_F(DateTimeFormatterTest, FormatShortGenericTimeZone) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();

  std::u16string result = formatter.Format(
      time, GetKnownLanguageTag("en-US"),
      datetime_options::YMDT::Medium()
          .with_time_zone(
              base::i18n::TimeZone::FromString("America/Los_Angeles"))
          .with_time_zone_style(
              DateTimeFormatterOptions::TimeZoneStyle::kShortGeneric));
  // "PT" or "Pacific Time" depending on ICU data/version.
  EXPECT_TRUE(result.find(u"PT") != std::u16string::npos ||
              result.find(u"Pacific Time") != std::u16string::npos)
      << base::UTF16ToUTF8(result);
}

TEST_F(DateTimeFormatterTest, FormatLongGenericTimeZone) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();

  std::u16string result = formatter.Format(
      time, GetKnownLanguageTag("en-US"),
      datetime_options::YMDT::Medium()
          .with_time_zone(
              base::i18n::TimeZone::FromString("America/Los_Angeles"))
          .with_time_zone_style(
              DateTimeFormatterOptions::TimeZoneStyle::kLongGeneric));
  EXPECT_NE(result.find(u"Pacific Time"), std::u16string::npos)
      << base::UTF16ToUTF8(result);
}

TEST_F(DateTimeFormatterTest, FormatWithSpecificTimeZoneObject) {
  // Default timezone is GMT (set in SetUp).
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();

  // Format with Los Angeles timezone object.
  base::i18n::TimeZone la_tz =
      base::i18n::TimeZone::FromString("America/Los_Angeles");

  std::u16string result = formatter.Format(
      time, GetKnownLanguageTag("en-US"),
      datetime_options::YMDT::Medium()
          .with_time_zone(la_tz)
          .with_time_zone_style(
              DateTimeFormatterOptions::TimeZoneStyle::kShortSpecific));

  // 10:30:00 UTC is 03:30:00 PDT.
  EXPECT_NE(result.find(u"3:30:00"), std::u16string::npos)
      << base::UTF16ToUTF8(result);
  EXPECT_NE(result.find(u"PDT"), std::u16string::npos)
      << base::UTF16ToUTF8(result);
}

TEST_F(DateTimeFormatterTest, FormatWithLanguageTag) {
  base::Time time;
  // 2011-04-30 is a Saturday
  ASSERT_TRUE(base::Time::FromUTCString("2011-04-30 15:42:07", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();

  EXPECT_EQ(u"2011/04/30", formatter.Format(time, GetKnownLanguageTag("ja"),
                                            datetime_options::YMD::Short()));

  EXPECT_EQ(u"4/30/11", formatter.Format(time, GetKnownLanguageTag("en-US"),
                                         datetime_options::YMD::Short()));

  EXPECT_EQ(u"30.04.11", formatter.Format(time, GetKnownLanguageTag("de-DE"),
                                          datetime_options::YMD::Short()));

  // Persian short date for 2011-04-30 (Solar Hijri 1390-02-10) is "۱۳۹۰/۲/۱۰"
  // or similar depending on ICU version.
  EXPECT_EQ(u"۱۳۹۰/۲/۱۰", formatter.Format(time, GetKnownLanguageTag("fa"),
                                           datetime_options::YMD::Short()));
}

TEST_F(DateTimeFormatterTest, FormatWithLanguageTagExtensions) {
  base::Time time;
  // 22:42:07
  ASSERT_TRUE(base::Time::FromUTCString("2011-04-30 22:42:07", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();

  // en-US defaults to 12h clock. Test extension for 24h.
  auto en_us_h23 =
      LanguageTagConverter::GetInstance().FromString("en-US-u-hc-h23");
  ASSERT_TRUE(en_us_h23.has_value());
  std::u16string result =
      formatter.Format(time, *en_us_h23, datetime_options::T::Short());
  EXPECT_NE(result.find(u"22:42"), std::u16string::npos);
  EXPECT_EQ(result.find(u"PM"), std::u16string::npos);

  // Test precedence: Options should override LanguageTag extensions.
  std::u16string result_override = formatter.Format(
      time, *en_us_h23,
      datetime_options::T::Short().with_hour_clock_type(base::k12HourClock));
  EXPECT_NE(result_override.find(u"10:42"), std::u16string::npos);
  EXPECT_NE(result_override.find(u"PM"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatWithInvalidLanguageTag) {
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2011-04-30 15:42:07", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();

  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUK(), status);

  // Use a bogus/invalid tag that doesn't parse to a valid locale.
  // LanguageTag itself might catch some, but if one gets through that ICU
  // doesn't like, it should fall back to default (UK in this case).
  auto bogus = LanguageTagConverter::GetInstance().FromString("xx-Bogus-Tag");
  if (bogus) {
    // UK format is DD/MM/YYYY
    EXPECT_EQ(u"30/04/2011",
              formatter.Format(time, *bogus, datetime_options::YMD::Short()));
  }
}

TEST_F(DateTimeFormatterTest, YMD_AllChromiumPlatformLocales) {
  RunExactMatchTests(
      {{.description = "Exact match for: yMd => M/d/y",
        .value = "2020-01-07 08:25:07",
        .options = {datetime_options::YMD::Short(),
                    datetime_options::YMD::Medium(),
                    datetime_options::YMD::Long()},
        .expectations = {
            {"af", {u"2020-01-07", u"07 Jan. 2020", u"07 Januarie 2020"}},
            {"am", {u"07/01/2020", u"7 ጃን 2020", u"7 ጃንዋሪ 2020"}},
            {"ar",
             {u"7\u200F/1\u200F/2020", u"07\u200F/01\u200F/2020",
              u"7 يناير 2020"}},
            {"ar-XB",
             {u"7\u200F/1\u200F/2020", u"07\u200F/01\u200F/2020",
              u"7 يناير 2020"}},
            {"bg", {u"7.01.20 г.", u"7.01.2020 г.", u"7 януари 2020 г."}},
            {"bn", {u"৭/১/২০", u"৭ জানু, ২০২০", u"৭ জানুয়ারি, ২০২০"}},
            {"ca", {u"7/1/20", u"7 de gen. 2020", u"7 de gener del 2020"}},
            {"cs", {u"07.01.20", u"7. 1. 2020", u"7. ledna 2020"}},
            {"da", {u"07.01.2020", u"7. jan. 2020", u"7. januar 2020"}},
            {"de", {u"07.01.20", u"07.01.2020", u"7. Januar 2020"}},
            {"el", {u"7/1/20", u"7 Ιαν 2020", u"7 Ιανουαρίου 2020"}},
            {"en-GB", {u"07/01/2020", u"7 Jan 2020", u"7 January 2020"}},
            {"en-US", {u"1/7/20", u"Jan 7, 2020", u"January 7, 2020"}},
            {"en-XA", {u"1/7/20", u"Jan 7, 2020", u"January 7, 2020"}},
            {"es", {u"7/1/20", u"7 ene 2020", u"7 de enero de 2020"}},
            {"es-419", {u"7/1/20", u"7 ene 2020", u"7 de enero de 2020"}},
            {"et", {u"07.01.20", u"7. jaan 2020", u"7. jaanuar 2020"}},
            {"fa", {u"۱۳۹۸/۱۰/۱۷", u"۱۷ دی ۱۳۹۸", u"۱۷ دی ۱۳۹۸"}},
            {"fi", {u"7.1.2020", u"7.1.2020", u"7. tammikuuta 2020"}},
            {"fil", {u"1/7/20", u"Ene 7, 2020", u"Enero 7, 2020"}},
            {"fr", {u"07/01/2020", u"7 janv. 2020", u"7 janvier 2020"}},
            {"gu", {u"7/1/20", u"7 જાન્યુ, 2020", u"7 જાન્યુઆરી, 2020"}},
            {"he", {u"7.1.2020", u"7 בינו׳ 2020", u"7 בינואר 2020"}},
            {"hi", {u"7/1/20", u"7 जन॰ 2020", u"7 जनवरी 2020"}},
            {"hr", {u"07. 01. 2020.", u"7. sij 2020.", u"7. siječnja 2020."}},
            {"hu", {u"2020. 01. 07.", u"2020. jan. 7.", u"2020. január 7."}},
            {"id", {u"07/01/20", u"7 Jan 2020", u"7 Januari 2020"}},
            {"it", {u"07/01/20", u"7 gen 2020", u"7 gennaio 2020"}},
            {"ja", {u"2020/01/07", u"2020/01/07", u"2020年1月7日"}},
            {"kn", {u"7/1/20", u"ಜನ 7, 2020", u"ಜನವರಿ 7, 2020"}},
            {"ko", {u"20. 1. 7.", u"2020. 1. 7.", u"2020년 1월 7일"}},
            {"lt", {u"2020-01-07", u"2020-01-07", u"2020 m. sausio 7 d."}},
            {"lv",
             {u"07.01.20", u"2020. gada 7. janv.", u"2020. gada 7. janvāris"}},
            {"ml", {u"7/1/20", u"2020 ജനു 7", u"2020 ജനുവരി 7"}},
            {"mr", {u"७/१/२०", u"७ जाने, २०२०", u"७ जानेवारी, २०२०"}},
            {"ms", {u"7/01/20", u"7 Jan 2020", u"7 Januari 2020"}},
            {"nb", {u"07.01.2020", u"7. jan. 2020", u"7. januar 2020"}},
            {"nl", {u"07-01-2020", u"7 jan 2020", u"7 januari 2020"}},
            {"pl", {u"7.01.2020", u"7 sty 2020", u"7 stycznia 2020"}},
            {"pt-BR",
             {u"07/01/2020", u"7 de jan. de 2020", u"7 de janeiro de 2020"}},
            {"pt-PT", {u"07/01/20", u"07/01/2020", u"7 de janeiro de 2020"}},
            {"ro", {u"07.01.2020", u"7 ian. 2020", u"7 ianuarie 2020"}},
            {"ru", {u"07.01.2020", u"7 янв. 2020 г.", u"7 января 2020 г."}},
            {"sl", {u"7. 1. 2020", u"7. jan. 2020", u"7. januar 2020"}},
            {"sr", {u"7. 1. 2020.", u"7. 1. 2020.", u"7. јануар 2020."}},
            {"sv", {u"2020-01-07", u"7 jan. 2020", u"7 januari 2020"}},
            {"sw", {u"07/01/2020", u"7 Jan 2020", u"7 Januari 2020"}},
            {"ta", {u"7/1/20", u"7 ஜன., 2020", u"7 ஜனவரி, 2020"}},
            {"te", {u"07-01-20", u"7 జన, 2020", u"7 జనవరి, 2020"}},
            {"th", {u"7/1/63", u"7 ม.ค. 2563", u"7 มกราคม 2563"}},
            {"tr", {u"7.01.2020", u"7 Oca 2020", u"7 Ocak 2020"}},
            {"uk", {u"07.01.20", u"7 січ. 2020 р.", u"7 січня 2020 р."}},
            {"ur", {u"7/1/20", u"7 جنوری، 2020", u"7 جنوری، 2020"}},
            {"vi", {u"7/1/20", u"7 thg 1, 2020", u"7 tháng 1, 2020"}},
            {"zh-CN", {u"2020/1/7", u"2020年1月7日", u"2020年1月7日"}},
            {"zh-TW", {u"2020/1/7", u"2020年1月7日", u"2020年1月7日"}},
        }}});
}

TEST_F(DateTimeFormatterTest, YMD_WithEra_AllChromiumPlatformLocales) {
  RunExactMatchTests(
      {{.description = "Exact match for: yMd with Era",
        .value = "2020-01-07 08:25:07",
        .options = {datetime_options::YMD::Short().with_year_style(
                        DateTimeFormatterOptions::YearStyle::kWithEra),
                    datetime_options::YMD::Medium().with_year_style(
                        DateTimeFormatterOptions::YearStyle::kWithEra),
                    datetime_options::YMD::Long().with_year_style(
                        DateTimeFormatterOptions::YearStyle::kWithEra)},
        .expectations = {
            {"af",
             {u"n.C. 2020-01-07", u"07 Jan. 2020 n.C.",
              u"07 Januarie 2020 n.C."}},
            {"am", {u"07/01/2020 ዓ/ም", u"ጃን 7 2020 ዓ/ም", u"ጃንዋሪ 7 2020 ዓ/ም"}},
            {"ar", {u"07-01-2020 م", u"07-01-2020 م", u"7 يناير 2020 م"}},
            {"ar-XB", {u"07-01-2020 م", u"07-01-2020 م", u"7 يناير 2020 م"}},
            {"bg",
             {u"07.01.20\u202fг. сл.Хр.", u"07.01.2020\u202fг. сл.Хр.",
              u"7 януари 2020\u202fг. сл.Хр."}},
            {"bn",
             {u"০৭-০১-২০ খৃষ্টাব্দ", u"৭ জানু, ২০২০ খৃষ্টাব্দ",
              u"৭ জানুয়ারি, ২০২০ খৃষ্টাব্দ"}},
            {"ca",
             {u"07-01-20 dC", u"7 de gen. del 2020 dC",
              u"7 de gener del 2020 dC"}},
            {"cs",
             {u"07. 01. 20 n.l.", u"7. 1. 2020 n.l.", u"7. ledna 2020 n. l."}},
            {"da",
             {u"07.01.2020 eKr", u"7. jan. 2020 e.Kr.",
              u"7. januar 2020 e.Kr."}},
            {"de",
             {u"07.01.20 n. Chr.", u"07.01.2020 n. Chr.",
              u"7. Januar 2020 n. Chr."}},
            {"el",
             {u"7/1/20 μ.Χ.", u"7 Ιαν 2020 μ.Χ.", u"7 Ιανουαρίου 2020 μ.Χ."}},
            {"en-GB",
             {u"07/01/2020 AD", u"7 Jan 2020 AD", u"7 January 2020 AD"}},
            {"en-US", {u"1/7/20 AD", u"Jan 7, 2020 AD", u"January 7, 2020 AD"}},
            {"en-XA", {u"1/7/20 AD", u"Jan 7, 2020 AD", u"January 7, 2020 AD"}},
            {"es",
             {u"7/1/20 d. C.", u"7 ene 2020 d. C.",
              u"7 de enero de 2020 d. C."}},
            {"es-419",
             {u"7/1/20 d.C.", u"7 de ene de 2020 d.C.",
              u"7 de enero de 2020 d.C."}},
            {"et",
             {u"07.01.20 pKr", u"7. jaan 2020 pKr", u"7. jaanuar 2020 pKr"}},
            {"fa-u-ca-persian",
             {u"۱۰/۱۷/۱۳۹۸ ه\u200d.ش.", u"۱۷ دی ۱۳۹۸ ه\u200d.ش.",
              u"۱۷ دی ۱۳۹۸ ه\u200d.ش."}},
            {"fi",
             {u"1.7.2020 jKr.", u"1.7.2020 jKr.", u"7. tammikuuta 2020 jKr."}},
            {"fil", {u"1/7/20 AD", u"Ene 7, 2020 AD", u"Enero 7, 2020 AD"}},
            {"fr",
             {u"07/01/2020 ap. J.-C.", u"7 janv. 2020 ap. J.-C.",
              u"7 janvier 2020 ap. J.-C."}},
            {"gu",
             {u"ઈ.સ. 20-01-07",
              u"7 જાન્યુ, ઈ.સ. 2020",  //
              u"7 જાન્યુઆરી, ઈ.સ. 2020"}},
            {"he",
             {u"7/1/2020 לספירה", u"7 בינו׳ 2020 לספירה",
              u"7 בינואר 2020 לספירה"}},
            {"hi",
             {u"ईस्वी 7/1/20",
              u"7 जन॰ 2020 ईस्वी",  //
              u"7 जनवरी 2020 ईस्वी"}},
            {"hr",
             {u"07. 01. 2020. AD", u"7. sij 2020. po. Kr.",
              u"7. siječnja 2020. po. Kr."}},
            {"hu",
             {u"isz. 2020. 01. 07.", u"i. sz. 2020. jan. 7.",
              u"i. sz. 2020. január 7."}},
            {"id", {u"07/01/20 M", u"7 Jan 2020 M", u"7 Januari 2020 M"}},
            {"it",
             {u"07/01/20 d.C.", u"7 gen 2020 d.C.", u"7 gennaio 2020 d.C."}},
            {"ja", {u"西暦2020/01/07", u"西暦2020/01/07", u"西暦2020年1月7日"}},
            {"kn",
             {u"ಕ್ರಿ.ಶ 20-01-07",
              u"ಕ್ರಿ.ಶ 2020 ಜನ 7",  //
              u"ಕ್ರಿ.ಶ 2020 ಜನವರಿ 7"}},
            {"ko", {u"AD 20/1/7", u"AD 2020/1/7", u"AD 2020년 1월 7일"}},
            {"lt",
             {u"2020-01-07 po Kr.", u"2020-01-07 po Kr.",
              u"2020 m. po Kr. sausio 7 d."}},
            {"lv",
             {u"m.ē. 07-01-20", u"m.ē. 2020. g. 7. janv.",
              u"m.ē. 2020. g. 7. janvāris"}},
            {"ml", {u"എഡി 20/1/7", u"എഡി 2020 ജനു 7", u"എഡി 2020 ജനുവരി 7"}},
            {"mr",
             {u"इ. स. ७/१/२०",
              u"७ जाने, इ. स. २०२०",  //
              u"७ जानेवारी, इ. स. २०२०"}},
            {"ms", {u"TM 20-01-07", u"7 Jan 2020 TM", u"7 Januari 2020 TM"}},
            {"nb",
             {u"07.01.2020 e.Kr.", u"7. jan. 2020 e.Kr.",
              u"7. januar 2020 e.Kr."}},
            {"nl",
             {u"07/01/2020 n.C.", u"7 jan 2020 n.Chr.",
              u"7 januari 2020 n.Chr."}},
            {"pl",
             {u"7.01.2020 n.e.", u"7 sty 2020 n.e.", u"7 stycznia 2020 n.e."}},
            {"pt-BR",
             {u"07/01/2020 d.C.", u"7 de jan. de 2020 d.C.",
              u"7 de janeiro de 2020 d.C."}},
            {"pt-PT",
             {u"07/01/20 d.C.", u"07/01/2020 d.C.",
              u"7 de janeiro de 2020 d.C."}},
            {"ro",
             {u"07.01.2020 d.Hr.", u"7 ian. 2020 d.Hr.",
              u"7 ianuarie 2020 d.Hr."}},
            {"ru",
             {u"07.01.2020 н.э.", u"7 янв. 2020\u202fг. н. э.",
              u"7 января 2020\u202fг. н. э."}},
            {"sl",
             {u"7. 1. 2020 po Kr.", u"7. jan. 2020 po Kr.",
              u"7. januar 2020 po Kr."}},
            {"sr",
             {u"7.01.2020. н.е.", u"7.01.2020. н.е.",
              u"7. јануар 2020. н. е."}},
            {"sv",
             {u"2020-01-07 e.Kr.", u"7 jan. 2020 e.Kr.",
              u"7 januari 2020 e.Kr."}},
            {"sw", {u"07/01/2020 BK", u"7 Jan 2020 BK", u"7 Januari 2020 BK"}},
            {"ta",
             {u"7/1/20 கி.பி.", u"கி.பி. 2020 ஜன. 7", u"கி.பி. 2020 ஜனவரி 7"}},
            {"te",
             {u"క్రీశ 20-01-07",
              u"క్రీశ 7, జన 2020",  //
              u"క్రీశ 7, జనవరి 2020"}},
#if BUILDFLAG(IS_IOS)
            {"th", {u"7/1/พ.ศ. 63", u"7/1/พ.ศ. 2563", u"7/1/พ.ศ. 2563"}},
#else
            {"th",
             {u"7/1/พ.ศ. 63", u"7 ม.ค. พ.ศ. 2563", u"7 มกราคม พ.ศ. 2563"}},
#endif
            {"tr", {u"MS 07.01.2020", u"MS 7 Oca 2020", u"MS 7 Ocak 2020"}},
            {"uk",
             {u"07-01-20 н.е.", u"7 січ. 2020 р. н. е.",
              u"7 січня 2020 р. н. е."}},
            {"ur",
             {u"7/1/20 عیسوی", u"7 جنوری، 2020 عیسوی", u"7 جنوری، 2020 عیسوی"}},
            {"vi",
             {u"7/1/20 SCN", u"7 thg 1, 2020 SCN", u"7 tháng 1, 2020 SCN"}},
            {"zh-CN",
             {u"公元2020-01-07", u"公元2020年1月7日", u"公元2020年1月7日"}},
            {"zh-TW",
             {u"西元 2020/1/7", u"西元2020年1月7日", u"西元2020年1月7日"}},
        }}});
}

TEST_F(DateTimeFormatterTest, YMDE_AllChromiumPlatformLocales) {
  std::vector<ExactMatchTestEntry::Expectation> expectations = {
      {"af",
       {u"Di. 2020-01-07", u"Di. 07 Jan. 2020", u"Dinsdag 07 Januarie 2020"}},
      // Differing from ICU4X in the Long() format.
      {"am", {u"ማክሰ፣ 07/01/2020", u"ማክሰ፣ ጃን 7 2020", u"ማክሰኞ 7 ጃንዋሪ 2020"}},
      // Differing from ICU4X in the Medium() format.
      {"ar",
       {u"الثلاثاء، 7\u200F/1\u200F/2020",
        u"الثلاثاء، 07‏/01‏/2020", u"الثلاثاء، 7 يناير 2020"}},
      {"ar-XB",
       {u"الثلاثاء، 7\u200f/1\u200f/2020",
        u"الثلاثاء، 07‏/01‏/2020", u"الثلاثاء، 7 يناير 2020"}},
      {"bg",
       {u"вт, 7.01.20\u202Fг.", u"вт, 7.01.2020\u202Fг.",
        u"вторник, 7 януари 2020\u202Fг."}},
      {"bn",
       {u"মঙ্গল, ৭/১/২০",                //
        u"মঙ্গল, ৭ জানু, ২০২০",           //
        u"মঙ্গলবার, ৭ জানুয়ারি, ২০২০"}},  //
      {"ca",
       {u"dt., 7/1/20", u"dt., 7 de gen. 2020",
        u"dimarts, 7 de gener del 2020"}},
      {"cs", {u"út 07. 01. 20", u"út 7. 1. 2020", u"úterý 7. ledna 2020"}},
      {"da",
       {u"tirs. 07.01.2020", u"tirs. 7. jan. 2020",
        u"tirsdag den 7. januar 2020"}},
      {"de",
       {u"Di., 07.01.20", u"Di., 07.01.2020", u"Dienstag, 7. Januar 2020"}},
      {"el", {u"Τρί 7/1/20", u"Τρί 7 Ιαν 2020", u"Τρίτη 7 Ιανουαρίου 2020"}},
      {"en-GB",
       {u"Tue, 07/01/2020", u"Tue, 7 Jan 2020", u"Tuesday, 7 January 2020"}},
      {"en-US",
       {u"Tue, 1/7/20", u"Tue, Jan 7, 2020", u"Tuesday, January 7, 2020"}},
      {"en-XA",
       {u"Tue, 1/7/20", u"Tue, Jan 7, 2020", u"Tuesday, January 7, 2020"}},
      {"es",
       {u"mar, 7/1/20", u"mar, 7 ene 2020", u"martes, 7 de enero de 2020"}},
      {"es-419",
       {u"mar 7/1/20", u"mar, 7 ene 2020", u"martes, 7 de enero de 2020"}},
      {"et",
       {u"T, 07.01.20", u"T, 7. jaanuar 2020", u"teisipäev, 7. jaanuar 2020"}},
      // This is differing from ICU4X on Medium()
      {"fa",
       {u"سه\u200Cشنبه ۱۳۹۸/۱۰/۱۷", u"۱۳۹۸ دی ۱۷, سه‌شنبه",
        u"۱۳۹۸ دی ۱۷, سه‌شنبه"}},
      {"fi", {u"ti 7.1.2020", u"ti 7.1.2020", u"tiistai 7. tammikuuta 2020"}},
      {"fil", {u"Mar, 1/7/20", u"Mar, Ene 7, 2020", u"Martes, Enero 7, 2020"}},
      {"fr",
       {u"mar. 07/01/2020", u"mar. 7 janv. 2020", u"mardi 7 janvier 2020"}},
      {"gu",
       {u"મંગળ, 7/1/20",
        u"મંગળ, 7 જાન્યુ, 2020",          //
        u"મંગળવાર, 7 જાન્યુઆરી, 2020"}},  //
      {"he",
       {u"יום ג׳, 7.1.2020", u"יום ג׳, 7 בינו׳ 2020",
        u"יום שלישי, 7 בינואר 2020"}},
      {"hi", {u"मंगल, 7/1/20", u"मंगल, 7 जन॰ 2020", u"मंगलवार, 7 जनवरी 2020"}},
      {"hr",
       {u"uto, 07. 01. 2020.", u"uto, 7. sij 2020.",
        u"utorak, 7. siječnja 2020."}},
      {"hu",
       {u"2020. 01. 07., K", u"2020. jan. 7., K", u"2020. január 7., kedd"}},
      {"id",
       {u"Sel, 07/01/20", u"Sel, 7 Jan 2020", u"Selasa, 07 Januari 2020"}},
      {"it", {u"mar 07/01/20", u"mar 7 gen 2020", u"martedì 7 gennaio 2020"}},
      {"ja", {u"2020/01/07(火)", u"2020/01/07(火)", u"2020年1月7日火曜日"}},
      {"kn", {u"ಮಂಗಳ, 1/7/20", u"ಮಂಗಳ, ಜನ 7, 2020", u"ಮಂಗಳವಾರ, ಜನವರಿ 7, 2020"}},
      {"ko",
       {u"20. 1. 7. (화)", u"2020. 1. 7. (화)", u"2020년 1월 7일 화요일"}},
      {"lt",
       {u"2020-01-07, an", u"2020-01-07, an",
        u"2020 m. sausio 7 d., antradienis"}},
      {"lv",
       {u"otrd., 07.01.20.", u"otrd., 2020. g. 7. janv.",
        u"otrdiena, 2020. gada 7. janvāris"}},
      {"ml",
       {u"7/1/20, ചൊവ്വ", u"2020 ജനു 7, ചൊവ്വ",  //
        u"2020 ജനുവരി 7, ചൊവ്വാഴ്ച"}},
      {"mr",
       {u"मंगळ, ७/१/२०", u"मंगळ, ७, जाने २०२०",  //
        u"मंगळवार, ७ जानेवारी, २०२०"}},
      {"ms", {u"Sel, 7/01/20", u"Sel, 7 Jan 2020", u"Selasa, 7 Januari 2020"}},
      {"nb",
       {u"tir. 07.01.2020", u"tir. 7. jan. 2020", u"tirsdag 7. januar 2020"}},
      {"nl", {u"di 07-01-2020", u"di 7 jan 2020", u"dinsdag 7 januari 2020"}},
      {"pl",
       {u"wt., 7.01.2020", u"wt., 7 sty 2020", u"wtorek, 7 stycznia 2020"}},
      {"pt-BR",
       {u"ter., 07/01/2020", u"ter., 7 de jan. de 2020",
        u"terça-feira, 7 de janeiro de 2020"}},
      {"pt-PT",
       {u"terça, 07/01/20", u"terça, 07/01/2020",
        u"terça-feira, 7 de janeiro de 2020"}},
      {"ro",
       {u"mar., 07.01.2020", u"mar., 7 ian. 2020", u"marți, 7 ianuarie 2020"}},
      {"ru",
       {u"вт, 07.01.2020", u"вт, 7 янв. 2020\u202fг.",
        u"вторник, 7 января 2020\u202fг."}},
      {"sl",
       {u"tor., 7. 1. 2020", u"tor., 7. jan. 2020", u"torek, 7. januar 2020"}},
      {"sr",
       {u"уто, 7. 1. 2020.", u"уто, 7. 1. 2020.", u"уторак, 7. јануар 2020."}},
      {"sv",
       {u"tis, 2020-01-07", u"tis 7 jan. 2020", u"tisdag 7 januari 2020"}},
      {"sw",
       {u"Jumanne, 07/01/2020", u"Jumanne, 7 Jan 2020",
        u"Jumanne, 7 Januari 2020"}},
      {"ta",
       {u"செவ்., 7/1/20", u"செவ்., 7 ஜன., 2020",  //
        u"செவ்வாய், 7 ஜனவரி, 2020"}},
      {"te",
       {u"07/01/20, మంగళ", u"7 జన, 2020, మంగళ", u"7, జనవరి 2020, మంగళవారం"}},
      {"th",
       {u"อังคาร 7/1/63",
        u"อังคาร 7 ม.ค. 2563",         //
        u"วันอังคารที่ 7 มกราคม 2563"}},  //
      {"tr", {u"7.01.2020 Sal", u"7 Oca 2020 Sal", u"7 Ocak 2020 Salı"}},
      {"uk",
       {u"вт, 07.01.20", u"вт, 7 січ. 2020\u202fр.",
        u"вівторок, 7 січня 2020\u202fр."}},
      {"ur", {u"منگل، 7/1/20", u"منگل، 7 جنوری، 2020", u"منگل، 7 جنوری، 2020"}},
      {"vi",
       {u"Thứ 3, 7/1/20", u"Thứ 3, 7 thg 1, 2020", u"Thứ Ba, 7 tháng 1, 2020"}},
      {"zh-CN", {u"2020/1/7周二", u"2020年1月7日周二", u"2020年1月7日星期二"}},
      {"zh-TW",
       {u"2020/1/7（週二）", u"2020年1月7日週二", u"2020年1月7日 星期二"}},
  };
  RunExactMatchTests({{.description = "Exact match for: YMDE",
                       .value = "2020-01-07 08:25:07",
                       .options = {datetime_options::YMDE::Short(),
                                   datetime_options::YMDE::Medium(),
                                   datetime_options::YMDE::Long()},
                       .expectations = expectations}});
}

TEST_F(DateTimeFormatterTest, YMDET_AllChromiumPlatformLocales) {
  std::vector<ExactMatchTestEntry::Expectation> expectations = {
      {"af",
       {u"Di. 2020-01-07 22:05:17", u"Di. 07 Jan. 2020 22:05:17",
        u"Dinsdag 07 Januarie 2020 om 22:05:17"}},
      {"am",
       {u"ማክሰ፣ 07/01/2020 10:05:17 ከሰዓት", u"ማክሰ፣ ጃን 7 2020 10:05:17 ከሰዓት",
        u"ማክሰኞ 7 ጃንዋሪ 2020 10:05:17 ከሰዓት"}},
      {"ar",
       {u"الثلاثاء، 7\u200f/1\u200f/2020، 10:05:17 م",
        u"الثلاثاء، 07\u200f/01\u200f/2020، 10:05:17 م",
        u"الثلاثاء، 7 يناير 2020 في 10:05:17 م"}},
      {"ar-XB",
       {u"الثلاثاء، 7\u200F/1\u200F/2020، 22:05:17",
        u"الثلاثاء، 07‏/01‏/2020، 22:05:17",
        u"الثلاثاء، 7 يناير 2020 في 22:05:17"}},
      {"bg",
       {u"вт, 7.01.20 г., 22:05:17", u"вт, 7.01.2020 г., 22:05:17",
        u"вторник, 7 януари 2020\u202fг. в 22:05:17 ч."}},
      {"bn",
       {u"মঙ্গল, ৭/১/২০, ১০:০৫:১৭ PM", u"মঙ্গল, ৭ জানু, ২০২০, ১০:০৫:১৭ PM",
        u"মঙ্গলবার, ৭ জানুয়ারি, ২০২০ এ ১০:০৫:১৭ PM"}},
      {"ca",
       {u"dt., 7/1/20 22:05:17", u"dt., 7 de gen. 2020, 22:05:17",
        u"dimarts, 7 de gener del 2020, a les 22:05:17"}},
      {"cs",
       {u"út 07. 01. 20 22:05:17", u"út 7. 1. 2020 22:05:17",
        u"úterý 7. ledna 2020 v 22:05:17"}},
      {"da",
       {u"tirs. 07.01.2020, 22.05.17", u"tirs. 7. jan. 2020, 22.05.17",
        u"tirsdag den 7. januar 2020 kl. 22.05.17"}},
      {"de",
       {u"Di., 07.01.20, 22:05:17", u"Di., 07.01.2020, 22:05:17",
        u"Dienstag, 7. Januar 2020 um 22:05:17"}},
      {"el",
       {u"Τρί 7/1/20, 10:05:17\u{202f}μ.μ.",
        u"Τρί 7 Ιαν 2020, 10:05:17\u{202f}μ.μ.",
        u"Τρίτη 7 Ιανουαρίου 2020 στις 10:05:17\u{202f}μ.μ."}},
      {"en-GB",
       {u"Tue, 07/01/2020, 22:05:17", u"Tue, 7 Jan 2020, 22:05:17",
        u"Tuesday, 7 January 2020 at 22:05:17"}},
      {"en-US",
       {u"Tue, 1/7/20, 10:05:17\u202fPM", u"Tue, Jan 7, 2020, 10:05:17\u202fPM",
        u"Tuesday, January 7, 2020 at 10:05:17\u202fPM"}},
      {"en-XA",
       {u"Tue, 1/7/20, 22:05:17", u"Tue, Jan 7, 2020, 22:05:17",
        u"Tuesday, January 7, 2020 at 22:05:17"}},
      {"es",
       {u"mar, 7/1/20, 22:05:17", u"mar, 7 ene 2020, 22:05:17",
        u"martes, 7 de enero de 2020, 22:05:17"}},
      {"es-419",
       {u"mar 7/1/20, 10:05:17\u{202f}p.m.",
        u"mar, 7 ene 2020, 10:05:17\u{202f}p.m.",
        u"martes, 7 de enero de 2020, 10:05:17 p.m."}},
      {"et",
       {u"T, 07.01.20, 22:05:17", u"T, 7. jaanuar 2020, 22:05:17",
        u"teisipäev, 7. jaanuar 2020, kell 22:05:17"}},
      {"fa-u-ca-persian",
       {u"سه\u200cشنبه ۱۳۹۸/۱۰/۱۷, ۲۲:۰۵:۱۷",
        u"۱۳۹۸ دی ۱۷, سه‌شنبه، ۲۲:۰۵:۱۷",
        u"۱۳۹۸ دی ۱۷, سه‌شنبه ساعت ۲۲:۰۵:۱۷"}},
      {"fi",
       {u"ti 7.1.2020 klo 22.05.17", u"ti 7.1.2020 klo 22.05.17",
        u"tiistai 7. tammikuuta 2020 klo 22.05.17"}},
      {"fil",
       {u"Mar, 1/7/20, 10:05:17\u{202f}PM",
        u"Mar, Ene 7, 2020, 10:05:17\u{202f}PM",
        u"Martes, Enero 7, 2020 nang 10:05:17\u{202f}PM"}},
      {"fr",
       {u"mar. 07/01/2020 22:05:17", u"mar. 7 janv. 2020, 22:05:17",
        u"mardi 7 janvier 2020 à 22:05:17"}},
      {"gu",
       {u"મંગળ, 7/1/20 10:05:17 PM", u"મંગળ, 7 જાન્યુ, 2020 10:05:17 PM",
        u"મંગળવાર, 7 જાન્યુઆરી, 2020 10:05:17 PM"}},
      {"he",
       {u"יום ג׳, 7.1.2020, 22:05:17", u"יום ג׳, 7 בינו׳ 2020, 22:05:17",
        u"יום שלישי, 7 בינואר 2020 בשעה 22:05:17"}},
      {"hi",
       {u"मंगल, 7/1/20, 10:05:17 pm", u"मंगल, 7 जन॰ 2020, 10:05:17 pm",
        u"मंगलवार, 7 जनवरी 2020 को 10:05:17 pm बजे"}},
      {"hr",
       {u"uto, 07. 01. 2020. 22:05:17", u"uto, 7. sij 2020. 22:05:17",
        u"utorak, 7. siječnja 2020. u 22:05:17"}},
      {"hu",
       {u"2020. 01. 07., K 22:05:17", u"2020. jan. 7., K 22:05:17",
        u"2020. január 7., kedd 22:05:17"}},
      {"id",
       {u"Sel, 07/01/20, 22.05.17", u"Sel, 7 Jan 2020, 22.05.17",
        u"Selasa, 07 Januari 2020 pukul 22.05.17"}},
      {"it",
       {u"mar 07/01/20, 22:05:17", u"mar 7 gen 2020, 22:05:17",
        u"martedì 7 gennaio 2020 alle ore 22:05:17"}},
      {"ja",
       {u"2020/01/07(火) 22:05:17", u"2020/01/07(火) 22:05:17",
        u"2020年1月7日火曜日 22:05:17"}},
      {"kn",
       {u"ಮಂಗಳ, 1/7/20, 10:05:17 PM", u"ಮಂಗಳ, ಜನ 7, 2020, 10:05:17 PM",
        u"ಮಂಗಳವಾರ, ಜನವರಿ 7, 2020 ರಂದು 10:05:17 PM ಸಮಯಕ್ಕೆ"}},
      {"ko",
       {u"20. 1. 7. (화) 오후 10:05:17", u"2020. 1. 7. (화) 오후 10:05:17",
        u"2020년 1월 7일 화요일 오후 10:05:17"}},
      {"lt",
       {u"2020-01-07, an 22:05:17", u"2020-01-07, an 22:05:17",
        u"2020 m. sausio 7 d., antradienis 22:05:17"}},
      {"lv",
       {u"otrd., 07.01.20. 22:05:17", u"otrd., 2020. g. 7. janv. 22:05:17",
        u"otrdiena, 2020. gada 7. janvāris 22:05:17"}},
      {"ml",
       {u"7/1/20, ചൊവ്വ, 10:05:17 PM", u"2020 ജനു 7, ചൊവ്വ, 10:05:17 PM",
        u"2020 ജനുവരി 7, ചൊവ്വാഴ്ച, 10:05:17 PM-ന്"}},
      {"mr",
       {u"मंगळ, ७/१/२०, १०:०५:१७ PM", u"मंगळ, ७, जाने २०२०, १०:०५:१७ PM",
        u"मंगळवार, ७ जानेवारी, २०२० रोजी १०:०५:१७ PM"}},
      {"ms",
       {u"Sel, 7/01/20, 10:05:17\u{202f}PTG",
        u"Sel, 7 Jan 2020, 10:05:17\u{202f}PTG",
        u"Selasa, 7 Januari 2020 pada 10:05:17\u{202f}PTG"}},
      {"nb",
       {u"tir. 07.01.2020, 22:05:17", u"tir. 7. jan. 2020, 22:05:17",
        u"tirsdag 7. januar 2020 kl. 22:05:17"}},
      {"nl",
       {u"di 07-01-2020, 22:05:17", u"di 7 jan 2020, 22:05:17",
        u"dinsdag 7 januari 2020 om 22:05:17"}},
      {"pl",
       {u"wt., 7.01.2020, 22:05:17", u"wt., 7 sty 2020, 22:05:17",
        u"wtorek, 7 stycznia 2020 22:05:17"}},
      {"pt-BR",
       {u"ter., 07/01/2020, 22:05:17", u"ter., 7 de jan. de 2020, 22:05:17",
        u"terça-feira, 7 de janeiro de 2020 às 22:05:17"}},
      {"pt-PT",
       {u"terça, 07/01/20, 22:05:17", u"terça, 07/01/2020, 22:05:17",
        u"terça-feira, 7 de janeiro de 2020 às 22:05:17"}},
      {"ro",
       {u"mar., 07.01.2020, 22:05:17", u"mar., 7 ian. 2020, 22:05:17",
        u"marți, 7 ianuarie 2020 la 22:05:17"}},
      {"ru",
       {u"вт, 07.01.2020, 22:05:17", u"вт, 7 янв. 2020\u{202f}г., 22:05:17",
        u"вторник, 7 января 2020\u{202f}г. в 22:05:17"}},
      {"sl",
       {u"tor., 7. 1. 2020, 22:05:17", u"tor., 7. jan. 2020, 22:05:17",
        u"torek, 7. januar 2020 ob 22:05:17"}},
      {"sr",
       {u"уто, 7. 1. 2020. 22:05:17", u"уто, 7. 1. 2020. 22:05:17",
        u"уторак, 7. јануар 2020. 22:05:17"}},
      {"sv",
       {u"tis, 2020-01-07 22:05:17", u"tis 7 jan. 2020 22:05:17",
        u"tisdag 7 januari 2020 kl. 22:05:17"}},
      {"sw",
       {u"Jumanne, 07/01/2020, 22:05:17", u"Jumanne, 7 Jan 2020, 22:05:17",
        u"Jumanne, 7 Januari 2020, 22:05:17"}},
      {"ta",
       {u"செவ்., 7/1/20, 10:05:17 PM", u"செவ்., 7 ஜன., 2020, 10:05:17 PM",
        u"செவ்வாய், 7 ஜனவரி, 2020 அன்று 10:05:17 PM"}},
      {"te",
       {u"07/01/20, మంగళ 10:05:17 PM", u"7 జన, 2020, మంగళ 10:05:17 PM",
        u"7, జనవరి 2020, మంగళవారం 10:05:17 PMకి"}},
      {"th",
       {u"อังคาร 7/1/63 22:05:17", u"อังคาร 7 ม.ค. 2563 22:05:17",
        u"วันอังคารที่ 7 มกราคม 2563 เวลา 22:05:17"}},
      {"tr",
       {u"7.01.2020 Sal 22:05:17", u"7 Oca 2020 Sal 22:05:17",
        u"7 Ocak 2020 Salı 22:05:17"}},
      {"uk",
       {u"вт, 07.01.20, 22:05:17", u"вт, 7 січ. 2020\u{202f}р., 22:05:17",
        u"вівторок, 7 січня 2020\u{202f}р. о 22:05:17"}},
      {"ur",
       {u"منگل، 7/1/20، 10:05:17 PM", u"منگل، 7 جنوری، 2020، 10:05:17 PM",
        u"منگل، 7 جنوری، 2020 کو 10:05:17 PM"}},
      {"vi",
       {u"22:05:17 Thứ 3, 7/1/20", u"22:05:17 Thứ 3, 7 thg 1, 2020",
        u"lúc 22:05:17 Thứ Ba, 7 tháng 1, 2020"}},
      {"zh-CN",
       {u"2020/1/7周二 22:05:17", u"2020年1月7日周二 22:05:17",
        u"2020年1月7日星期二 22:05:17"}},
      {"zh-TW",
       {u"2020/1/7（週二） 下午10:05:17", u"2020年1月7日週二 下午10:05:17",
        u"2020年1月7日星期二 下午10:05:17"}},
  };
  RunExactMatchTests({{.description = "Exact match for: YMDET",
                       .value = "2020-01-07 22:05:17.01",
                       .options = {datetime_options::YMDET::Short(),
                                   datetime_options::YMDET::Medium(),
                                   datetime_options::YMDET::Long()},
                       .expectations = expectations}});
}

}  // namespace base::i18n
