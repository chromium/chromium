// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/date_helper.h"

#include <string_view>

#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/test/ash_test_base.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/rtl.h"
#include "base/i18n/tag_converters.h"
#include "base/i18n/test/locales_for_test.h"
#include "base/i18n/test/scoped_icu_locale.h"
#include "chromeos/ash/components/settings/scoped_timezone_settings.h"

namespace ash {

using ::base::i18n::GetKnownLanguageTag;
using ::base::i18n::LanguageTag;

class DateHelperUnittest : public AshTestBase {
 public:
  DateHelperUnittest() = default;
  DateHelperUnittest(const DateHelperUnittest& other) = delete;
  DateHelperUnittest& operator=(const DateHelperUnittest& other) = delete;
  ~DateHelperUnittest() override = default;

  void TearDown() override {
    DateHelper::GetInstance()->ResetForTesting();
    locale_override_.reset();
    AshTestBase::TearDown();
  }

  void SetDefaultLocale(LanguageTag lang) {
    locale_override_.emplace(lang);
    DateHelper::GetInstance()->ResetFormatters();
    DateHelper::GetInstance()->CalculateLocalWeekTitles();
  }

  std::u16string Format12HrClockInterval(const base::Time& start_time,
                                         const base::Time& end_time) {
    return DateHelper::GetInstance()->GetFormattedInterval(
        DateHelper::GetInstance()->twelve_hour_clock_interval_formatter(),
        start_time, end_time);
  }

  std::u16string Format24HrClockInterval(const base::Time& start_time,
                                         const base::Time& end_time) {
    return DateHelper::GetInstance()->GetFormattedInterval(
        DateHelper::GetInstance()->twenty_four_hour_clock_interval_formatter(),
        start_time, end_time);
  }

 private:
  std::optional<base::i18n::ScopedDefaultIcuLocale> locale_override_;
};

// Gets the calendar week titles in different language and order.
TEST_F(DateHelperUnittest, GetWeekTitle) {
  SetDefaultLocale(GetKnownLanguageTag("zh-CN"));
  std::vector<std::u16string> week_titles =
      DateHelper::GetInstance()->week_titles();
  EXPECT_EQ(u"一", week_titles[0]);  // Monday
  EXPECT_EQ(u"二", week_titles[1]);  // Tuesday
  EXPECT_EQ(u"三", week_titles[2]);  // Wednesday
  EXPECT_EQ(u"四", week_titles[3]);  // Thursday
  EXPECT_EQ(u"五", week_titles[4]);  // Friday
  EXPECT_EQ(u"六", week_titles[5]);  // Saturday
  EXPECT_EQ(u"日", week_titles[6]);  // Sunday

  SetDefaultLocale(GetKnownLanguageTag("ar"));
  week_titles = DateHelper::GetInstance()->week_titles();
  EXPECT_EQ(u"س", week_titles[0]);  // s
  EXPECT_EQ(u"ح", week_titles[1]);  // h
  EXPECT_EQ(u"ن", week_titles[2]);  // n
  EXPECT_EQ(u"ث", week_titles[3]);  // w
  EXPECT_EQ(u"ر", week_titles[4]);  // R
  EXPECT_EQ(u"خ", week_titles[5]);  // Kh
  EXPECT_EQ(u"ج", week_titles[6]);  // c

  SetDefaultLocale(GetKnownLanguageTag("ko"));
  week_titles = DateHelper::GetInstance()->week_titles();
  EXPECT_EQ(u"일", week_titles[0]);
  EXPECT_EQ(u"월", week_titles[1]);
  EXPECT_EQ(u"화", week_titles[2]);
  EXPECT_EQ(u"수", week_titles[3]);
  EXPECT_EQ(u"목", week_titles[4]);
  EXPECT_EQ(u"금", week_titles[5]);
  EXPECT_EQ(u"토", week_titles[6]);

  SetDefaultLocale(GetKnownLanguageTag("en-US"));
  week_titles = DateHelper::GetInstance()->week_titles();
  EXPECT_EQ(u"S", week_titles[0]);
  EXPECT_EQ(u"M", week_titles[1]);
  EXPECT_EQ(u"T", week_titles[2]);
  EXPECT_EQ(u"W", week_titles[3]);
  EXPECT_EQ(u"T", week_titles[4]);
  EXPECT_EQ(u"F", week_titles[5]);
  EXPECT_EQ(u"S", week_titles[6]);
}

// Tests getting the calendar week titles in all languages.
TEST_F(DateHelperUnittest, GetWeekTitleForAllLocales) {
  for (auto locale : base::i18n::GetLocalesForTest()) {
    SetDefaultLocale(locale);
    EXPECT_EQ(7U, DateHelper::GetInstance()->week_titles().size());
  }
}

// Formats the interval between two dates in different languages.
TEST_F(DateHelperUnittest, GetFormattedInterval) {
  ash::system::ScopedTimezoneSettings timezone_settings(u"GMT");

  base::Time date1;  // Start date
  base::Time date2;  // End date, same meridiem
  base::Time date3;  // End date, different meridiem
  ASSERT_TRUE(base::Time::FromString("22 Nov 2021 10:00 GMT", &date1));
  ASSERT_TRUE(base::Time::FromString("22 Nov 2021 11:45 GMT", &date2));
  ASSERT_TRUE(base::Time::FromString("22 Nov 2021 22:30 GMT", &date3));

  SetDefaultLocale(GetKnownLanguageTag("en-US"));
  EXPECT_EQ(u"10:00\u2009–\u200911:45\u202fAM",
            Format12HrClockInterval(date1, date2));
  EXPECT_EQ(u"10:00\u202fAM\u2009–\u200910:30\u202fPM",
            Format12HrClockInterval(date1, date3));
  EXPECT_EQ(u"10:00\u2009–\u200911:45", Format24HrClockInterval(date1, date2));
  EXPECT_EQ(u"10:00\u2009–\u200922:30", Format24HrClockInterval(date1, date3));

  SetDefaultLocale(GetKnownLanguageTag("zh-Hant"));
  EXPECT_EQ(u"上午10:00至11:45", Format12HrClockInterval(date1, date2));
  EXPECT_EQ(u"上午10:00至晚上10:30", Format12HrClockInterval(date1, date3));
  EXPECT_EQ(u"10:00 – 11:45", Format24HrClockInterval(date1, date2));
  EXPECT_EQ(u"10:00 – 22:30", Format24HrClockInterval(date1, date3));

  SetDefaultLocale(GetKnownLanguageTag("ar"));
  EXPECT_EQ(u"10:00–11:45 ص", Format12HrClockInterval(date1, date2));
  EXPECT_EQ(u"10:00 ص – 10:30 م", Format12HrClockInterval(date1, date3));
  EXPECT_EQ(u"10:00–11:45", Format24HrClockInterval(date1, date2));
  EXPECT_EQ(u"10:00–22:30", Format24HrClockInterval(date1, date3));
}

}  // namespace ash
