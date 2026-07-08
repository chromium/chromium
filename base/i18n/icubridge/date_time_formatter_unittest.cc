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
    DateTimeFormatterOptions options;
    struct Expectation {
      std::string_view locale;
      std::string_view expected;
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
        std::string_view expected_str = expectation.expected;

        auto language_tag =
            LanguageTagConverter::GetInstance().FromString(locale_str);
        ASSERT_TRUE(language_tag.has_value())
            << "Invalid locale: " << locale_str;

        std::string actual = base::UTF16ToUTF8(
            formatter.Format(time, *language_tag, entry.options));
        EXPECT_THAT(actual, ::testing::StrEq(expected_str))
            << "Failed: " << entry.description << " Locale: " << locale_str
            << " Actual: " << actual << " Expected: " << expected_str;
      }
    }
  }
};

TEST_F(DateTimeFormatterTest, YearTests) {
  RunExactMatchTests({
      {
          .description = "Exact match for: y => y (long)",
          .value = "2020-01-07 08:25:07",
          .options = datetime_options::Y::Long(),
          .expectations = {{"en", "2020"},
                           {"en-GB", "2020"},
                           {"ja", "2020年"},
                           {"de", "2020"},
                           {"fa", "۱۳۹۸"}},
      },
  });
  RunExactMatchTests({
      {
          .description = "Exact match for: y => y (medium)",
          .value = "2020-01-07 08:25:07",
          .options = datetime_options::Y::Medium(),
          .expectations = {{"en", "2020"},
                           {"en-GB", "2020"},
                           {"ja", "2020年"},
                           {"de", "2020"},
                           {"fa", "۱۳۹۸"}},
      },
  });
  RunExactMatchTests({
      {
          .description = "Exact match for: y => y (short)",
          .value = "2020-01-07 08:25:07",
          .options = datetime_options::Y::Short(),
          .expectations =
              {{"en", "20"}, {"ja", "20年"}, {"pt", "20"}, {"fa", "۹۸"}},
      },
  });
}

TEST_F(DateTimeFormatterTest, MonthTests) {
  RunExactMatchTests({
      {
          .description = "Month: Short (numeric)",
          .value = "2020-01-07 08:25:07",
          .options = datetime_options::M::Short(),
          .expectations = {{"en", "1"}, {"ja", "1月"}, {"de", "1"}},
      },
      {
          .description = "Month: Medium (abbreviated)",
          .value = "2020-01-07 08:25:07",
          .options = datetime_options::M::Medium(),
          .expectations =
              {{"en", "Jan"}, {"ja", "1月"}, {"de", "Jan"}, {"fa", "دی"}},
      },
      {
          .description = "Month: Long (full)",
          .value = "2020-01-07 08:25:07",
          .options = datetime_options::M::Long(),
          .expectations = {{"en", "January"},
                           {"ja", "1月"},
                           {"de", "Januar"},
                           {"fa", "دی"}},
      },
  });
}

TEST_F(DateTimeFormatterTest, DayAndWeekdayTests) {
  RunExactMatchTests({
      {
          .description = "Day and Weekday: Medium",
          .value = "2020-01-07 08:25:07",
          .options = datetime_options::DE::Medium(),
          .expectations = {{"en", "7 Tue"},
                           {"en-GB", "Tue 7"},
                           {"ja", "7日(火)"},
                           {"de", "Di., 7."},
                           {"fa", "سه‌شنبه ۱۷م"}},
      },
      {
          .description = "Day and Weekday: Long",
          .value = "2020-01-07 08:25:07",
          .options = datetime_options::DE::Long(),
          .expectations = {{"en", "07 Tuesday"},
                           {"ja", "07日火曜日"},
                           {"de", "Dienstag, 07."},
                           {"fa", "سه‌شنبه ۱۷م"}},
      },
      {
          .description = "Day: Medium",
          .value = "2020-01-07 08:25:07",
          .options = datetime_options::D::Short(),
          .expectations =
              {{"en", "7"}, {"ja", "7日"}, {"de", "7"}, {"fa", "۱۷"}},
      },
      {
          .description = "Day: Long",
          .value = "2020-01-07 08:25:07",
          .options = datetime_options::D::Long(),
          .expectations =
              {{"en", "07"}, {"ja", "07日"}, {"de", "07"}, {"fa", "۱۷"}},
      },
      {
          .description = "Exact match for: E => ccc",
          .value = "2020-01-07 08:25:07",
          .options = datetime_options::E::Medium(),
          .expectations = {{"en", "Tue"},
                           {"en-GB", "Tue"},
                           {"ja", "火"},
                           {"de", "Di"},
                           {"fa", "سه‌شنبه"}},
      },
  });
}

TEST_F(DateTimeFormatterTest, CombinationTests) {
  RunExactMatchTests({
      {
          .description = "Exact match for: yMd => M/d/y",
          .value = "2020-01-07 08:25:07",
          .options = datetime_options::YMD::Medium(),
          .expectations = {{"en", "Jan 7, 2020"},
                           {"en-GB", "7 Jan 2020"},
                           {"ja", "2020/01/07"},
                           {"de", "07.01.2020"},
                           {"fa", "۱۷ دی ۱۳۹۸"}},
      },
      {
          .description = "Exact match for: yMd => M/d/y",
          .value = "2020-01-07 08:25:07",
          .options = datetime_options::YMD::Short(),
          .expectations = {{"en", "1/7/20"},
                           {"en-GB", "07/01/2020"},
                           {"ja", "2020/01/07"},
                           {"de", "07.01.20"},
                           {"fa", "۱۳۹۸/۱۰/۱۷"}},
      },
      {
          .description = "Exact match for: yMdE => E, M/d/y",
          .value = "2020-01-07 08:25:07",
          .options = datetime_options::YMDE::Medium(),
          .expectations = {{"en", "Tue, Jan 7, 2020"},
                           {"en-GB", "Tue, 7 Jan 2020"},
                           {"ja", "2020/01/07(火)"},
                           {"de", "Di., 07.01.2020"},
                           // ICU4X outputs "سه‌شنبه ۱۷ دی ۱۳۹۸"
                           {"fa", "۱۳۹۸ دی ۱۷, سه‌شنبه"}},
      },
      {
          .description = "Exact match for: yMMMM => MMMM y",
          .value = "2020-01-07 08:25:07",
          .options = datetime_options::YM::Long(),
          .expectations = {{"en", "January 2020"},
                           {"en-GB", "January 2020"},
                           {"ja", "2020年1月"},
                           {"de", "Januar 2020"},
                           {"fa", "۱۳۹۸ دی"}},
      },
      {
          .description = "Exact match for: Md => M/d",
          .value = "2020-01-07 08:25:07",
          .options = datetime_options::MD::Short(),
          .expectations = {{"en", "1/7"},
                           {"en-GB", "07/01"},
                           {"ja", "01/07"},
                           {"de", "07.01."},
                           {"fa", "۱۰/۱۷"}},
      },
      {
          .description = "Exact match for: MdE => E, M/d",
          .value = "2020-01-07 08:25:07",
          .options = datetime_options::MDE::Short(),
          .expectations =
              {{"en", "Tue, 1/7"},
               {"en-GB", "Tue 07/01"},
               {"ja", "01/07(火)"},
               {"de", "Di., 07.01."},
               // Also, ICU4X produces this output in gregorian calendar.
               {"fa", "سه‌شنبه ۱۰/۱۷"}},
      },
  });
}

TEST_F(DateTimeFormatterTest, JapaneseCalendarTests) {
  RunExactMatchTests({
      {
          .description = "Japanese Era: Reiwa 2",
          .value = "2020-02-20 00:12:00",
          .options = datetime_options::Y::Medium().with_year_style(
              DateTimeFormatterOptions::YearStyle::kWithEra),
          .expectations = {{"en-u-ca-japanese", "2 Reiwa"},
                           {"ja-u-ca-japanese", "令和2年"}},
      },
      {
          .description = "Japanese Era: Heisei 22",
          .value = "2010-02-20 00:12:00",
          .options = datetime_options::Y::Medium().with_year_style(
              DateTimeFormatterOptions::YearStyle::kWithEra),
          .expectations = {{"en-u-ca-japanese", "22 Heisei"},
                           {"ja-u-ca-japanese", "平成22年"}},
      },
  });
}

TEST_F(DateTimeFormatterTest, TimeAndEraTests) {
  RunExactMatchTests({
      {
          .description = "Full Date Time with Era",
          .value = "2020-01-21 08:25:07",
          .options = datetime_options::YMDET::Long()
                         .with_year_style(
                             DateTimeFormatterOptions::YearStyle::kWithEra)
                         .with_time_precision(
                             DateTimeFormatterOptions::TimePrecision::kSecond)
                         .with_hour_clock_type(base::k24HourClock),
          .expectations =
              {{"en-u-hc-h23", "Tuesday, January 21, 2020 AD at 08:25:07"},
               {"en-GB-u-hc-h23", "Tuesday, 21 January 2020 AD at 08:25:07"},
               {"de-u-hc-h23", "Dienstag, 21. Januar 2020 n. Chr. um 08:25:07"},
               // ICU4X outputs in gregorian calendar: سه‌شنبه ۲۱
               // ژانویهٔ ۲۰۲۰ م. ساعت ۸:۲۵:۰۷
               {"fa-u-hc-h23",
                "سه‌شنبه ۱ بهمن ۱۳۹۸ ه‍.ش. ساعت "
                "۸:۲۵:۰۷"}},
      },
      {
          .description = "Time Only with Second",
          .value = "2022-05-03 14:15:07.123",
          .options = datetime_options::T::Short()
                         .with_time_precision(
                             DateTimeFormatterOptions::TimePrecision::kSecond)
                         .with_hour_clock_type(base::k24HourClock),
          .expectations =
              {
                  {"en-u-hc-h23", "14:15:07"},
                  {"en-GB-u-hc-h23", "14:15:07"},
                  {"ja-u-hc-h23", "14:15:07"},
                  {"de-u-hc-h23", "14:15:07"},
                  {"fa-u-hc-h23", "۱۴:۱۵:۰۷"},
                  {"fa", "۱۴:۱۵:۰۷"},
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

}  // namespace base::i18n
