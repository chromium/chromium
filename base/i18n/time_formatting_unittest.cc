// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/time_formatting.h"

#include <memory>

#include "base/i18n/rtl.h"
#include "base/i18n/unicodestring.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/uversion.h"
#include "third_party/icu/source/i18n/unicode/calendar.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/icu/source/i18n/unicode/tzfmt.h"

namespace base {
namespace {

const Time::Exploded kTestDateTimeExploded = {
    2011, 4,  6, 30,  // Sat, Apr 30, 2011
    22,   42, 7, 0    // 22:42:07.000 in UTC = 15:42:07 in US PDT.
};

// Returns difference between the local time and GMT formatted as string.
// This function gets |time| because the difference depends on time,
// see https://en.wikipedia.org/wiki/Daylight_saving_time for details.
string16 GetShortTimeZone(const Time& time) {
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());
  std::unique_ptr<icu::TimeZoneFormat> zone_formatter(
      icu::TimeZoneFormat::createInstance(icu::Locale::getDefault(), status));
  EXPECT_TRUE(U_SUCCESS(status));
  icu::UnicodeString name;
  zone_formatter->format(UTZFMT_STYLE_SPECIFIC_SHORT, *zone,
                         static_cast<UDate>(time.ToDoubleT() * 1000),
                         name, nullptr);
  return i18n::UnicodeStringToString16(name);
}

// Calls TimeDurationFormat() with |delta| and |width| and returns the resulting
// string. On failure, adds a failed expectation and returns an empty string.
string16 TimeDurationFormatString(const TimeDelta& delta,
                                  DurationFormatWidth width) {
  string16 str;
  EXPECT_TRUE(TimeDurationFormat(delta, width, &str))
      << "Failed to format " << delta.ToInternalValue() << " with width "
      << width;
  return str;
}

// Calls TimeDurationFormatWithSeconds() with |delta| and |width| and returns
// the resulting string. On failure, adds a failed expectation and returns an
// empty string.
string16 TimeDurationFormatWithSecondsString(const TimeDelta& delta,
                                             DurationFormatWidth width) {
  string16 str;
  EXPECT_TRUE(TimeDurationFormatWithSeconds(delta, width, &str))
      << "Failed to format " << delta.ToInternalValue() << " with width "
      << width;
  return str;
}

class ScopedRestoreDefaultTimezone {
 public:
  ScopedRestoreDefaultTimezone(const char* zoneid) {
    original_zone_.reset(icu::TimeZone::createDefault());
    icu::TimeZone::adoptDefault(icu::TimeZone::createTimeZone(zoneid));
  }
  ~ScopedRestoreDefaultTimezone() {
    icu::TimeZone::adoptDefault(original_zone_.release());
  }

  ScopedRestoreDefaultTimezone(const ScopedRestoreDefaultTimezone&) = delete;
  ScopedRestoreDefaultTimezone& operator=(const ScopedRestoreDefaultTimezone&) =
      delete;

 private:
  std::unique_ptr<icu::TimeZone> original_zone_;
};

TEST(TimeFormattingTest, TimeFormatTimeOfDayDefault12h) {
  // Test for a locale defaulted to 12h clock.
  // As an instance, we use third_party/icu/source/data/locales/en.txt.
  test::ScopedRestoreICUDefaultLocale restore_locale;
  i18n::SetICUDefaultLocale("en_US");
  ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  Time time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestDateTimeExploded, &time));
  string16 clock24h(ASCIIToUTF16("15:42"));
  string16 clock12h_pm(ASCIIToUTF16("3:42 PM"));
  string16 clock12h(ASCIIToUTF16("3:42"));
  string16 clock24h_millis(ASCIIToUTF16("15:42:07.000"));

  // The default is 12h clock.
  EXPECT_EQ(clock12h_pm, TimeFormatTimeOfDay(time));
  EXPECT_EQ(clock24h_millis, TimeFormatTimeOfDayWithMilliseconds(time));
  EXPECT_EQ(k12HourClock, GetHourClockType());
  // k{Keep,Drop}AmPm should not affect for 24h clock.
  EXPECT_EQ(clock24h,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k24HourClock,
                                                 kKeepAmPm));
  EXPECT_EQ(clock24h,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k24HourClock,
                                                 kDropAmPm));
  // k{Keep,Drop}AmPm affects for 12h clock.
  EXPECT_EQ(clock12h_pm,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k12HourClock,
                                                 kKeepAmPm));
  EXPECT_EQ(clock12h,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k12HourClock,
                                                 kDropAmPm));
}

TEST(TimeFormattingTest, TimeFormatTimeOfDayDefault24h) {
  // Test for a locale defaulted to 24h clock.
  // As an instance, we use third_party/icu/source/data/locales/en_GB.txt.
  test::ScopedRestoreICUDefaultLocale restore_locale;
  i18n::SetICUDefaultLocale("en_GB");
  ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  Time time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestDateTimeExploded, &time));
  string16 clock24h(ASCIIToUTF16("15:42"));
  string16 clock12h_pm(ASCIIToUTF16("3:42 pm"));
  string16 clock12h(ASCIIToUTF16("3:42"));
  string16 clock24h_millis(ASCIIToUTF16("15:42:07.000"));

  // The default is 24h clock.
  EXPECT_EQ(clock24h, TimeFormatTimeOfDay(time));
  EXPECT_EQ(clock24h_millis, TimeFormatTimeOfDayWithMilliseconds(time));
  EXPECT_EQ(k24HourClock, GetHourClockType());
  // k{Keep,Drop}AmPm should not affect for 24h clock.
  EXPECT_EQ(clock24h,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k24HourClock,
                                                 kKeepAmPm));
  EXPECT_EQ(clock24h,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k24HourClock,
                                                 kDropAmPm));
  // k{Keep,Drop}AmPm affects for 12h clock.
  EXPECT_EQ(clock12h_pm,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k12HourClock,
                                                 kKeepAmPm));
  EXPECT_EQ(clock12h,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k12HourClock,
                                                 kDropAmPm));
}

TEST(TimeFormattingTest, TimeFormatTimeOfDayJP) {
  // Test for a locale that uses different mark than "AM" and "PM".
  // As an instance, we use third_party/icu/source/data/locales/ja.txt.
  test::ScopedRestoreICUDefaultLocale restore_locale;
  i18n::SetICUDefaultLocale("ja_JP");
  ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  Time time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestDateTimeExploded, &time));
  string16 clock24h(ASCIIToUTF16("15:42"));
  string16 clock12h_pm(UTF8ToUTF16(u8"午後3:42"));
  string16 clock12h(ASCIIToUTF16("3:42"));

  // The default is 24h clock.
  EXPECT_EQ(clock24h, TimeFormatTimeOfDay(time));
  EXPECT_EQ(k24HourClock, GetHourClockType());
  // k{Keep,Drop}AmPm should not affect for 24h clock.
  EXPECT_EQ(clock24h, TimeFormatTimeOfDayWithHourClockType(time, k24HourClock,
                                                           kKeepAmPm));
  EXPECT_EQ(clock24h, TimeFormatTimeOfDayWithHourClockType(time, k24HourClock,
                                                           kDropAmPm));
  // k{Keep,Drop}AmPm affects for 12h clock.
  EXPECT_EQ(clock12h_pm, TimeFormatTimeOfDayWithHourClockType(
                             time, k12HourClock, kKeepAmPm));
  EXPECT_EQ(clock12h, TimeFormatTimeOfDayWithHourClockType(time, k12HourClock,
                                                           kDropAmPm));
}

TEST(TimeFormattingTest, TimeFormatTimeOfDayDE) {
  // German uses 24h by default, but uses 'AM', 'PM' for 12h format.
  test::ScopedRestoreICUDefaultLocale restore_locale;
  i18n::SetICUDefaultLocale("de");
  ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  Time time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestDateTimeExploded, &time));
  string16 clock24h(ASCIIToUTF16("15:42"));
  string16 clock12h_pm(UTF8ToUTF16("3:42 PM"));
  string16 clock12h(ASCIIToUTF16("3:42"));

  // The default is 24h clock.
  EXPECT_EQ(clock24h, TimeFormatTimeOfDay(time));
  EXPECT_EQ(k24HourClock, GetHourClockType());
  // k{Keep,Drop}AmPm should not affect for 24h clock.
  EXPECT_EQ(clock24h,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k24HourClock,
                                                 kKeepAmPm));
  EXPECT_EQ(clock24h,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k24HourClock,
                                                 kDropAmPm));
  // k{Keep,Drop}AmPm affects for 12h clock.
  EXPECT_EQ(clock12h_pm,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k12HourClock,
                                                 kKeepAmPm));
  EXPECT_EQ(clock12h,
            TimeFormatTimeOfDayWithHourClockType(time,
                                                 k12HourClock,
                                                 kDropAmPm));
}

TEST(TimeFormattingTest, TimeFormatDateUS) {
  // See third_party/icu/source/data/locales/en.txt.
  // The date patterns are "EEEE, MMMM d, y", "MMM d, y", and "M/d/yy".
  test::ScopedRestoreICUDefaultLocale restore_locale;
  i18n::SetICUDefaultLocale("en_US");
  ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  Time time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestDateTimeExploded, &time));

  EXPECT_EQ(ASCIIToUTF16("Apr 30, 2011"), TimeFormatShortDate(time));
  EXPECT_EQ(ASCIIToUTF16("4/30/11"), TimeFormatShortDateNumeric(time));

  EXPECT_EQ(ASCIIToUTF16("4/30/11, 3:42:07 PM"),
            TimeFormatShortDateAndTime(time));
  EXPECT_EQ(ASCIIToUTF16("4/30/11, 3:42:07 PM ") + GetShortTimeZone(time),
            TimeFormatShortDateAndTimeWithTimeZone(time));

  EXPECT_EQ(ASCIIToUTF16("April 2011"), TimeFormatMonthAndYear(time));

  EXPECT_EQ(ASCIIToUTF16("Saturday, April 30, 2011 at 3:42:07 PM"),
            TimeFormatFriendlyDateAndTime(time));

  EXPECT_EQ(ASCIIToUTF16("Saturday, April 30, 2011"),
            TimeFormatFriendlyDate(time));
}

TEST(TimeFormattingTest, TimeFormatDateGB) {
  // See third_party/icu/source/data/locales/en_GB.txt.
  // The date patterns are "EEEE, d MMMM y", "d MMM y", and "dd/MM/yyyy".
  test::ScopedRestoreICUDefaultLocale restore_locale;
  i18n::SetICUDefaultLocale("en_GB");
  ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  Time time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestDateTimeExploded, &time));

  EXPECT_EQ(ASCIIToUTF16("30 Apr 2011"), TimeFormatShortDate(time));
  EXPECT_EQ(ASCIIToUTF16("30/04/2011"), TimeFormatShortDateNumeric(time));
  EXPECT_EQ(ASCIIToUTF16("30/04/2011, 15:42:07"),
            TimeFormatShortDateAndTime(time));
  EXPECT_EQ(ASCIIToUTF16("30/04/2011, 15:42:07 ") + GetShortTimeZone(time),
            TimeFormatShortDateAndTimeWithTimeZone(time));
  EXPECT_EQ(ASCIIToUTF16("April 2011"), TimeFormatMonthAndYear(time));
  EXPECT_EQ(ASCIIToUTF16("Saturday, 30 April 2011 at 15:42:07"),
            TimeFormatFriendlyDateAndTime(time));
  EXPECT_EQ(ASCIIToUTF16("Saturday, 30 April 2011"),
            TimeFormatFriendlyDate(time));
}

TEST(TimeFormattingTest, TimeFormatWithPattern) {
  test::ScopedRestoreICUDefaultLocale restore_locale;
  ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  Time time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestDateTimeExploded, &time));

  i18n::SetICUDefaultLocale("en_US");
  EXPECT_EQ(ASCIIToUTF16("Apr 30, 2011"), TimeFormatWithPattern(time, "yMMMd"));
  EXPECT_EQ(ASCIIToUTF16("April 30, 3:42:07 PM"),
            TimeFormatWithPattern(time, "MMMMdjmmss"));

  i18n::SetICUDefaultLocale("en_GB");
  EXPECT_EQ(ASCIIToUTF16("30 Apr 2011"), TimeFormatWithPattern(time, "yMMMd"));
  EXPECT_EQ(ASCIIToUTF16("30 April, 15:42:07"),
            TimeFormatWithPattern(time, "MMMMdjmmss"));

  i18n::SetICUDefaultLocale("ja_JP");
  EXPECT_EQ(UTF8ToUTF16(u8"2011年4月30日"),
            TimeFormatWithPattern(time, "yMMMd"));
  EXPECT_EQ(UTF8ToUTF16(u8"4月30日 15:42:07"),
            TimeFormatWithPattern(time, "MMMMdjmmss"));
}

TEST(TimeFormattingTest, TimeDurationFormat) {
  test::ScopedRestoreICUDefaultLocale restore_locale;
  TimeDelta delta = TimeDelta::FromMinutes(15 * 60 + 42);

  // US English.
  i18n::SetICUDefaultLocale("en_US");
  EXPECT_EQ(ASCIIToUTF16("15 hours, 42 minutes"),
            TimeDurationFormatString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(ASCIIToUTF16("15 hr, 42 min"),
            TimeDurationFormatString(delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(ASCIIToUTF16("15h 42m"),
            TimeDurationFormatString(delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(ASCIIToUTF16("15:42"),
            TimeDurationFormatString(delta, DURATION_WIDTH_NUMERIC));

  // Danish, with Latin alphabet but different abbreviations and punctuation.
  i18n::SetICUDefaultLocale("da");
  EXPECT_EQ(ASCIIToUTF16("15 timer og 42 minutter"),
            TimeDurationFormatString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(ASCIIToUTF16("15 t og 42 min."),
            TimeDurationFormatString(delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(ASCIIToUTF16("15 t og 42 m"),
            TimeDurationFormatString(delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(ASCIIToUTF16("15.42"),
            TimeDurationFormatString(delta, DURATION_WIDTH_NUMERIC));

  // Persian, with non-Arabic numbers.
  i18n::SetICUDefaultLocale("fa");
  string16 fa_wide = UTF8ToUTF16(
      u8"\u06f1\u06f5 \u0633\u0627\u0639\u062a \u0648 \u06f4\u06f2 \u062f\u0642"
      u8"\u06cc\u0642\u0647");
  string16 fa_short = UTF8ToUTF16(
      u8"\u06f1\u06f5 \u0633\u0627\u0639\u062a\u060c\u200f \u06f4\u06f2 \u062f"
      u8"\u0642\u06cc\u0642\u0647");
  string16 fa_narrow = UTF8ToUTF16(
      u8"\u06f1\u06f5 \u0633\u0627\u0639\u062a \u06f4\u06f2 \u062f\u0642\u06cc"
      u8"\u0642\u0647");
  string16 fa_numeric = UTF8ToUTF16(u8"\u06f1\u06f5:\u06f4\u06f2");
  EXPECT_EQ(fa_wide, TimeDurationFormatString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(fa_short, TimeDurationFormatString(delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(fa_narrow, TimeDurationFormatString(delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(fa_numeric,
            TimeDurationFormatString(delta, DURATION_WIDTH_NUMERIC));
}

TEST(TimeFormattingTest, TimeDurationFormatWithSeconds) {
  test::ScopedRestoreICUDefaultLocale restore_locale;

  // US English.
  i18n::SetICUDefaultLocale("en_US");

  // Test different formats.
  TimeDelta delta = TimeDelta::FromSeconds(15 * 3600 + 42 * 60 + 30);
  EXPECT_EQ(ASCIIToUTF16("15 hours, 42 minutes, 30 seconds"),
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(ASCIIToUTF16("15 hr, 42 min, 30 sec"),
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(ASCIIToUTF16("15h 42m 30s"),
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(ASCIIToUTF16("15:42:30"),
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_NUMERIC));

  // Test edge case when hour >= 100.
  delta = TimeDelta::FromSeconds(125 * 3600 + 42 * 60 + 30);
  EXPECT_EQ(ASCIIToUTF16("125 hours, 42 minutes, 30 seconds"),
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(ASCIIToUTF16("125 hr, 42 min, 30 sec"),
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(ASCIIToUTF16("125h 42m 30s"),
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_NARROW));

  // Test edge case when minute = 0.
  delta = TimeDelta::FromSeconds(15 * 3600 + 0 * 60 + 30);
  EXPECT_EQ(ASCIIToUTF16("15 hours, 0 minutes, 30 seconds"),
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(ASCIIToUTF16("15 hr, 0 min, 30 sec"),
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(ASCIIToUTF16("15h 0m 30s"),
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(ASCIIToUTF16("15:00:30"),
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_NUMERIC));

  // Test edge case when second = 0.
  delta = TimeDelta::FromSeconds(15 * 3600 + 42 * 60 + 0);
  EXPECT_EQ(ASCIIToUTF16("15 hours, 42 minutes, 0 seconds"),
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(ASCIIToUTF16("15 hr, 42 min, 0 sec"),
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(ASCIIToUTF16("15h 42m 0s"),
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(ASCIIToUTF16("15:42:00"),
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_NUMERIC));
}

TEST(TimeFormattingTest, TimeIntervalFormat) {
  test::ScopedRestoreICUDefaultLocale restore_locale;
  i18n::SetICUDefaultLocale("en_US");
  ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  const Time::Exploded kTestIntervalEndTimeExploded = {
      2011, 5,  6, 28,  // Sat, May 28, 2012
      22,   42, 7, 0    // 22:42:07.000
  };

  Time begin_time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestDateTimeExploded, &begin_time));
  Time end_time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestIntervalEndTimeExploded, &end_time));

  EXPECT_EQ(
      UTF8ToUTF16(u8"Saturday, April 30 – Saturday, May 28"),
      DateIntervalFormat(begin_time, end_time, DATE_FORMAT_MONTH_WEEKDAY_DAY));

  const Time::Exploded kTestIntervalBeginTimeExploded = {
      2011, 5,  1, 16,  // Mon, May 16, 2012
      22,   42, 7, 0    // 22:42:07.000
  };
  EXPECT_TRUE(
      Time::FromUTCExploded(kTestIntervalBeginTimeExploded, &begin_time));
  EXPECT_EQ(
      UTF8ToUTF16(u8"Monday, May 16 – Saturday, May 28"),
      DateIntervalFormat(begin_time, end_time, DATE_FORMAT_MONTH_WEEKDAY_DAY));

  i18n::SetICUDefaultLocale("en_GB");
  EXPECT_EQ(
      UTF8ToUTF16(u8"Monday 16 May – Saturday 28 May"),
      DateIntervalFormat(begin_time, end_time, DATE_FORMAT_MONTH_WEEKDAY_DAY));

  i18n::SetICUDefaultLocale("ja");
  EXPECT_EQ(
      UTF8ToUTF16(u8"5月16日(月曜日)～28日(土曜日)"),
      DateIntervalFormat(begin_time, end_time, DATE_FORMAT_MONTH_WEEKDAY_DAY));
}

}  // namespace
}  // namespace base
