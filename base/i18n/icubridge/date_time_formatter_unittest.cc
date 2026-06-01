// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icubridge/date_time_formatter.h"

#include "base/i18n/icu_util.h"
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace base::i18n {

class DateTimeFormatterTest : public testing::Test {
 public:
  void SetUp() override {
    base::i18n::InitializeICU();
    // Force UTC timezone for predictable results.
    icu::TimeZone::adoptDefault(icu::TimeZone::getGMT()->clone());
  }
};

TEST_F(DateTimeFormatterTest, FormatShortDate) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result =
      formatter.Format(time, datetime_options::YMDT::Short());

  EXPECT_FALSE(result.empty());
  // US short date/time might be "5/25/26, 10:30 AM"
  EXPECT_NE(result.find(u"25"), std::u16string::npos);
  EXPECT_NE(result.find(u"10:30"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatYMD) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result =
      formatter.Format(time, datetime_options::YMD::Medium());

  EXPECT_FALSE(result.empty());
  EXPECT_NE(result.find(u"25"), std::u16string::npos);
  EXPECT_NE(result.find(u"2026"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatY) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result = formatter.Format(time, datetime_options::Y::Medium());

  EXPECT_EQ(result, u"2026");
}

TEST_F(DateTimeFormatterTest, FormatE) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  // 2026-05-25 is a Monday
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result = formatter.Format(time, datetime_options::E::Medium());

  EXPECT_EQ(result, u"Mon");
}

TEST_F(DateTimeFormatterTest, FormatWithPrecision) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result = formatter.Format(
      time, datetime_options::YMD::Medium().with_time_precision(
                DateTimeFormatterOptions::TimePrecision::kMinute));

  EXPECT_FALSE(result.empty());
  EXPECT_NE(result.find(u"10:30"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatWithEra) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result = formatter.Format(
      time, datetime_options::Y::Medium().with_year_style(
                DateTimeFormatterOptions::YearStyle::kWithEra));

  EXPECT_NE(result.find(u"AD"), std::u16string::npos);
}

// New ComponentBagType Tests
TEST_F(DateTimeFormatterTest, FormatD) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  EXPECT_EQ(formatter.Format(time, datetime_options::D::Medium()), u"25");
}

TEST_F(DateTimeFormatterTest, FormatDE) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result =
      formatter.Format(time, datetime_options::DE::Medium());
  EXPECT_NE(result.find(u"Mon"), std::u16string::npos);
  EXPECT_NE(result.find(u"25"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatDET) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result =
      formatter.Format(time, datetime_options::DET::Medium());
  EXPECT_NE(result.find(u"Mon"), std::u16string::npos);
  EXPECT_NE(result.find(u"25"), std::u16string::npos);
  EXPECT_NE(result.find(u"10:30:00"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatDT) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result =
      formatter.Format(time, datetime_options::DT::Medium());
  EXPECT_NE(result.find(u"25"), std::u16string::npos);
  EXPECT_NE(result.find(u"10:30:00"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatET) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result =
      formatter.Format(time, datetime_options::ET::Medium());
  EXPECT_NE(result.find(u"Mon"), std::u16string::npos);
  EXPECT_NE(result.find(u"10:30:00"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatM) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  EXPECT_EQ(formatter.Format(time, datetime_options::M::Medium()), u"May");
}

TEST_F(DateTimeFormatterTest, FormatMD) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  EXPECT_EQ(formatter.Format(time, datetime_options::MD::Medium()), u"May 25");
}

TEST_F(DateTimeFormatterTest, FormatMDE) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result =
      formatter.Format(time, datetime_options::MDE::Medium());
  EXPECT_NE(result.find(u"Mon"), std::u16string::npos);
  EXPECT_NE(result.find(u"May"), std::u16string::npos);
  EXPECT_NE(result.find(u"25"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatMDET) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result =
      formatter.Format(time, datetime_options::MDET::Medium());
  EXPECT_NE(result.find(u"Mon"), std::u16string::npos);
  EXPECT_NE(result.find(u"May"), std::u16string::npos);
  EXPECT_NE(result.find(u"25"), std::u16string::npos);
  EXPECT_NE(result.find(u"10:30:00"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatMDT) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result =
      formatter.Format(time, datetime_options::MDT::Medium());
  EXPECT_NE(result.find(u"May"), std::u16string::npos);
  EXPECT_NE(result.find(u"25"), std::u16string::npos);
  EXPECT_NE(result.find(u"10:30:00"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatT) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  EXPECT_NE(
      formatter.Format(time, datetime_options::T::Medium()).find(u"10:30:00"),
      std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatYM) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  EXPECT_EQ(formatter.Format(time, datetime_options::YM::Medium()),
            u"May 2026");
}

TEST_F(DateTimeFormatterTest, FormatYMDE) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result =
      formatter.Format(time, datetime_options::YMDE::Medium());
  EXPECT_NE(result.find(u"Mon"), std::u16string::npos);
  EXPECT_NE(result.find(u"25"), std::u16string::npos);
  EXPECT_NE(result.find(u"2026"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatYMDET) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result =
      formatter.Format(time, datetime_options::YMDET::Medium());
  EXPECT_NE(result.find(u"Mon"), std::u16string::npos);
  EXPECT_NE(result.find(u"5/25/2026"), std::u16string::npos);
  EXPECT_NE(result.find(u"10:30:00"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, FormatYMDT) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00", &time));
  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();
  std::u16string result =
      formatter.Format(time, datetime_options::YMDT::Medium());
  EXPECT_NE(result.find(u"10:30:00"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, HourClockType) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  // 10:30 PM is 22:30
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 22:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();

  // 12-hour clock
  std::u16string result12 = formatter.Format(
      time,
      datetime_options::T::Short().with_hour_clock_type(base::k12HourClock));
  EXPECT_NE(result12.find(u"10:30"), std::u16string::npos);
  EXPECT_NE(result12.find(u"PM"), std::u16string::npos);

  // 24-hour clock
  std::u16string result24 = formatter.Format(
      time,
      datetime_options::T::Short().with_hour_clock_type(base::k24HourClock));
  EXPECT_NE(result24.find(u"22:30"), std::u16string::npos);
  EXPECT_EQ(result24.find(u"PM"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, AmPmClockType) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  // 10:30 PM
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 22:30:00", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();

  // 12-hour clock, keep AM/PM
  std::u16string result_keep =
      formatter.Format(time, datetime_options::T::Short()
                                 .with_hour_clock_type(base::k12HourClock)
                                 .with_am_pm_clock_type(base::kKeepAmPm));
  EXPECT_NE(result_keep.find(u"10:30"), std::u16string::npos);
  EXPECT_NE(result_keep.find(u"PM"), std::u16string::npos);

  // 12-hour clock, drop AM/PM
  std::u16string result_drop =
      formatter.Format(time, datetime_options::T::Short()
                                 .with_hour_clock_type(base::k12HourClock)
                                 .with_am_pm_clock_type(base::kDropAmPm));
  EXPECT_NE(result_drop.find(u"10:30"), std::u16string::npos);
  EXPECT_EQ(result_drop.find(u"PM"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, HourClockTypeWithLength) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
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
  std::u16string result12 = formatter.Format(time, options);
  EXPECT_NE(result12.find(u"10:30"), std::u16string::npos);
  EXPECT_NE(result12.find(u"PM"), std::u16string::npos);

  // 24-hour clock
  options.hour_clock_type = base::k24HourClock;
  std::u16string result24 = formatter.Format(time, options);

  EXPECT_NE(result24.find(u"22:30"), std::u16string::npos);
  EXPECT_EQ(result24.find(u"PM"), std::u16string::npos);
}

TEST_F(DateTimeFormatterTest, SubsecondPrecision) {
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale::setDefault(icu::Locale::getUS(), status);
  base::Time time;
  ASSERT_TRUE(base::Time::FromUTCString("2026-05-25 10:30:00.987", &time));

  const IcuBridge::DateTimeFormatter& formatter =
      IcuBridge::GetInstance().date_time_formatter();

  // kSubsecond_3 should show .987
  std::u16string result = formatter.Format(
      time, datetime_options::T::Medium().with_time_precision(
                DateTimeFormatterOptions::TimePrecision::kSubsecond_3));
  EXPECT_NE(result.find(u"10:30:00.987"), std::u16string::npos);
}

}  // namespace base::i18n
