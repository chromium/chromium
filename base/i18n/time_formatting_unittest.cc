// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/time_formatting.h"

#include <memory>

#include "base/i18n/rtl.h"
#include "base/i18n/unicodestring.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/uversion.h"
#include "third_party/icu/source/i18n/unicode/calendar.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/icu/source/i18n/unicode/tzfmt.h"

namespace base {
namespace {

constexpr Time::Exploded kTestDateTimeExploded = {.year = 2011,
                                                  .month = 4,
                                                  .day_of_week = 6,
                                                  .day_of_month = 30,
                                                  .hour = 22,
                                                  .minute = 42,
                                                  .second = 7};

// Returns difference between the local time and GMT formatted as string.
// This function gets |time| because the difference depends on time,
// see https://en.wikipedia.org/wiki/Daylight_saving_time for details.
std::u16string GetShortTimeZone(const Time& time) {
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::TimeZone> zone(icu::TimeZone::createDefault());
  std::unique_ptr<icu::TimeZoneFormat> zone_formatter(
      icu::TimeZoneFormat::createInstance(icu::Locale::getDefault(), status));
  EXPECT_TRUE(U_SUCCESS(status));
  icu::UnicodeString name;
  zone_formatter->format(
      UTZFMT_STYLE_SPECIFIC_SHORT, *zone,
      static_cast<UDate>(time.InSecondsFSinceUnixEpoch() * 1000), name,
      nullptr);
  return i18n::UnicodeStringToString16(name);
}

// Calls TimeDurationFormat() with |delta| and |width| and returns the resulting
// string. On failure, adds a failed expectation and returns an empty string.
std::u16string TimeDurationFormatString(const TimeDelta& delta,
                                        DurationFormatWidth width) {
  std::u16string str;
  EXPECT_TRUE(TimeDurationFormat(delta, width, &str))
      << "Failed to format " << delta.ToInternalValue() << " with width "
      << width;
  return str;
}

// Calls TimeDurationFormatWithSeconds() with |delta| and |width| and returns
// the resulting string. On failure, adds a failed expectation and returns an
// empty string.
std::u16string TimeDurationFormatWithSecondsString(const TimeDelta& delta,
                                                   DurationFormatWidth width) {
  std::u16string str;
  EXPECT_TRUE(TimeDurationFormatWithSeconds(delta, width, &str))
      << "Failed to format " << delta.ToInternalValue() << " with width "
      << width;
  return str;
}

// Calls TimeDurationCompactFormatWithSeconds() with |delta| and |width| and
// returns the resulting string. On failure, adds a failed expectation and
// returns an empty string.
std::u16string TimeDurationCompactFormatWithSecondsString(
    const TimeDelta& delta,
    DurationFormatWidth width) {
  std::u16string str;
  EXPECT_TRUE(TimeDurationCompactFormatWithSeconds(delta, width, &str))
      << "Failed to format " << delta.ToInternalValue() << " with width "
      << width;
  return str;
}

TEST(TimeFormattingTest, TimeFormatTimeOfDayDefault12h) {
  // Test for a locale defaulted to 12h clock.
  // As an instance, we use third_party/icu/source/data/locales/en.txt.
  test::ScopedRestoreICUDefaultLocale restore_locale;
  i18n::SetICUDefaultLocale("en_US");
  test::ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  Time time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestDateTimeExploded, &time));
  std::u16string clock24h(u"15:42");
  std::u16string clock12h_pm(u"3:42\u202fPM");
  std::u16string clock12h(u"3:42");
  std::u16string clock24h_millis(u"15:42:07.000");

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
  test::ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  Time time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestDateTimeExploded, &time));
  std::u16string clock24h(u"15:42");
  std::u16string clock12h_pm(u"3:42\u202fpm");
  std::u16string clock12h(u"3:42");
  std::u16string clock24h_millis(u"15:42:07.000");

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
  test::ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  Time time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestDateTimeExploded, &time));
  std::u16string clock24h(u"15:42");
  std::u16string clock12h_pm(u"午後3:42");
  std::u16string clock12h(u"3:42");

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
  test::ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  Time time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestDateTimeExploded, &time));
  std::u16string clock24h(u"15:42");
  std::u16string clock12h_pm(u"3:42\u202fPM");
  std::u16string clock12h(u"3:42");

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST(TimeFormattingTest, TimeMonthYearInUTC) {
  // See third_party/icu/source/data/locales/en.txt.
  // The date patterns are "EEEE, MMMM d, y", "MMM d, y", and "M/d/yy".
  test::ScopedRestoreICUDefaultLocale restore_locale;
  i18n::SetICUDefaultLocale("en_US");
  test::ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  Time time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestDateTimeExploded, &time));
  EXPECT_EQ(u"April 2011",
            TimeFormatMonthAndYearForTimeZone(time, icu::TimeZone::getGMT()));
  EXPECT_EQ(u"April 2011", TimeFormatMonthAndYear(time));

  const Time::Exploded kDiffMonthsForDiffTzTime = {
      2011, 4, 5, 1,  // Fri, Apr 1, 2011 UTC = Thurs, March 31, 2011 US PDT.
      0,    0, 0, 0   // 00:00:00.000 UTC = 05:00:00 previous day US PDT.
  };

  EXPECT_TRUE(Time::FromUTCExploded(kDiffMonthsForDiffTzTime, &time));
  EXPECT_EQ(u"April 2011",
            TimeFormatMonthAndYearForTimeZone(time, icu::TimeZone::getGMT()));
  EXPECT_EQ(u"March 2011", TimeFormatMonthAndYear(time));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

TEST(TimeFormattingTest, TimeFormatDateUS) {
  // See third_party/icu/source/data/locales/en.txt.
  // The date patterns are "EEEE, MMMM d, y", "MMM d, y", and "M/d/yy".
  test::ScopedRestoreICUDefaultLocale restore_locale;
  i18n::SetICUDefaultLocale("en_US");
  test::ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  Time time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestDateTimeExploded, &time));

  EXPECT_EQ(u"Apr 30, 2011", TimeFormatShortDate(time));
  EXPECT_EQ(u"4/30/11", TimeFormatShortDateNumeric(time));

  EXPECT_EQ(u"4/30/11, 3:42:07\u202fPM", TimeFormatShortDateAndTime(time));
  EXPECT_EQ(u"4/30/11, 3:42:07\u202fPM " + GetShortTimeZone(time),
            TimeFormatShortDateAndTimeWithTimeZone(time));

  EXPECT_EQ(u"April 2011", TimeFormatMonthAndYear(time));

  EXPECT_EQ(u"Saturday, April 30, 2011 at 3:42:07\u202fPM",
            TimeFormatFriendlyDateAndTime(time));

  EXPECT_EQ(u"Saturday, April 30, 2011", TimeFormatFriendlyDate(time));
}

TEST(TimeFormattingTest, TimeFormatDateGB) {
  // See third_party/icu/source/data/locales/en_GB.txt.
  // The date patterns are "EEEE, d MMMM y", "d MMM y", and "dd/MM/yyyy".
  test::ScopedRestoreICUDefaultLocale restore_locale;
  i18n::SetICUDefaultLocale("en_GB");
  test::ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  Time time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestDateTimeExploded, &time));

  EXPECT_EQ(u"30 Apr 2011", TimeFormatShortDate(time));
  EXPECT_EQ(u"30/04/2011", TimeFormatShortDateNumeric(time));
  EXPECT_EQ(u"30/04/2011, 15:42:07", TimeFormatShortDateAndTime(time));
  EXPECT_EQ(u"30/04/2011, 15:42:07 " + GetShortTimeZone(time),
            TimeFormatShortDateAndTimeWithTimeZone(time));
  EXPECT_EQ(u"April 2011", TimeFormatMonthAndYear(time));
  EXPECT_EQ(u"Saturday 30 April 2011 at 15:42:07",
            TimeFormatFriendlyDateAndTime(time));
  EXPECT_EQ(u"Saturday 30 April 2011", TimeFormatFriendlyDate(time));
}

TEST(TimeFormattingTest, TimeFormatWithPattern) {
  test::ScopedRestoreICUDefaultLocale restore_locale;
  test::ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  Time time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestDateTimeExploded, &time));

  i18n::SetICUDefaultLocale("en_US");
  EXPECT_EQ(u"Apr 30, 2011", LocalizedTimeFormatWithPattern(time, "yMMMd"));
  EXPECT_EQ(u"April 30 at 3:42:07\u202fPM",
            LocalizedTimeFormatWithPattern(time, "MMMMdjmmss"));
  EXPECT_EQ(
      "Sat! 30 Apr 2011 at 15.42+07",
      UnlocalizedTimeFormatWithPattern(time, "E! dd MMM y 'at' HH.mm+ss"));
  EXPECT_EQ("Sat! 30 Apr 2011 at 22.42+07",
            UnlocalizedTimeFormatWithPattern(time, "E! dd MMM y 'at' HH.mm+ss",
                                             icu::TimeZone::getGMT()));

  i18n::SetICUDefaultLocale("en_GB");
  EXPECT_EQ(u"30 Apr 2011", LocalizedTimeFormatWithPattern(time, "yMMMd"));
  EXPECT_EQ(u"30 April at 15:42:07",
            LocalizedTimeFormatWithPattern(time, "MMMMdjmmss"));
  EXPECT_EQ(
      "Sat! 30 Apr 2011 at 15.42+07",
      UnlocalizedTimeFormatWithPattern(time, "E! dd MMM y 'at' HH.mm+ss"));

  i18n::SetICUDefaultLocale("ja_JP");
  EXPECT_EQ(u"2011年4月30日", LocalizedTimeFormatWithPattern(time, "yMMMd"));
  EXPECT_EQ(u"4月30日 15:42:07",
            LocalizedTimeFormatWithPattern(time, "MMMMdjmmss"));
  EXPECT_EQ(
      "Sat! 30 Apr 2011 at 15.42+07",
      UnlocalizedTimeFormatWithPattern(time, "E! dd MMM y 'at' HH.mm+ss"));
}

TEST(TimeFormattingTest, UnlocalizedTimeFormatWithPatternMicroseconds) {
  Time no_micros;
  EXPECT_TRUE(Time::FromUTCExploded(kTestDateTimeExploded, &no_micros));
  const Time micros = no_micros + Microseconds(987);

  // Should support >3 'S' characters, truncating.
  EXPECT_EQ("07.0009", UnlocalizedTimeFormatWithPattern(micros, "ss.SSSS"));
  EXPECT_EQ("07.00098", UnlocalizedTimeFormatWithPattern(micros, "ss.SSSSS"));
  EXPECT_EQ("07.000987", UnlocalizedTimeFormatWithPattern(micros, "ss.SSSSSS"));

  // >6 'S' characters is also valid, and should be zero-filled.
  EXPECT_EQ("07.0009870",
            UnlocalizedTimeFormatWithPattern(micros, "ss.SSSSSSS"));

  // Quoted 'S's should be ignored.
  EXPECT_EQ("07.SSSSSS",
            UnlocalizedTimeFormatWithPattern(micros, "ss.'SSSSSS'"));

  // Multiple substitutions are possible.
  EXPECT_EQ("07.000987'000987.07",
            UnlocalizedTimeFormatWithPattern(micros, "ss.SSSSSS''SSSSSS.ss"));

  // All the above should still work when the number of microseconds is zero.
  EXPECT_EQ("07.0000", UnlocalizedTimeFormatWithPattern(no_micros, "ss.SSSS"));
  EXPECT_EQ("07.00000",
            UnlocalizedTimeFormatWithPattern(no_micros, "ss.SSSSS"));
  EXPECT_EQ("07.000000",
            UnlocalizedTimeFormatWithPattern(no_micros, "ss.SSSSSS"));
  EXPECT_EQ("07.0000000",
            UnlocalizedTimeFormatWithPattern(no_micros, "ss.SSSSSSS"));
  EXPECT_EQ("07.SSSSSS",
            UnlocalizedTimeFormatWithPattern(no_micros, "ss.'SSSSSS'"));
  EXPECT_EQ("07.000000'000000.07", UnlocalizedTimeFormatWithPattern(
                                       no_micros, "ss.SSSSSS''SSSSSS.ss"));
}

TEST(TimeFormattingTest, TimeFormatAsIso8601) {
  Time time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestDateTimeExploded, &time));
  EXPECT_EQ("2011-04-30T22:42:07.000Z", TimeFormatAsIso8601(time));
}

TEST(TimeFormattingTest, TimeFormatHTTP) {
  Time time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestDateTimeExploded, &time));
  EXPECT_EQ("Sat, 30 Apr 2011 22:42:07 GMT", TimeFormatHTTP(time));
}

TEST(TimeFormattingTest, TimeDurationFormat) {
  test::ScopedRestoreICUDefaultLocale restore_locale;
  TimeDelta delta = Minutes(15 * 60 + 42);

  // US English.
  i18n::SetICUDefaultLocale("en_US");
  EXPECT_EQ(u"15 hours, 42 minutes",
            TimeDurationFormatString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(u"15 hr, 42 min",
            TimeDurationFormatString(delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(u"15h 42m", TimeDurationFormatString(delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(u"15:42", TimeDurationFormatString(delta, DURATION_WIDTH_NUMERIC));

  // Danish, with Latin alphabet but different abbreviations and punctuation.
  i18n::SetICUDefaultLocale("da");
  EXPECT_EQ(u"15 timer og 42 minutter",
            TimeDurationFormatString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(u"15 t. og 42 min.",
            TimeDurationFormatString(delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(u"15 t og 42 m",
            TimeDurationFormatString(delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(u"15.42", TimeDurationFormatString(delta, DURATION_WIDTH_NUMERIC));

  // Persian, with non-Arabic numbers.
  i18n::SetICUDefaultLocale("fa");
  std::u16string fa_wide =
      u"\u06f1\u06f5 \u0633\u0627\u0639\u062a \u0648 \u06f4\u06f2 \u062f\u0642"
      u"\u06cc\u0642\u0647";
  std::u16string fa_short =
      u"\u06f1\u06f5 \u0633\u0627\u0639\u062a\u060c\u200f \u06f4\u06f2 \u062f"
      u"\u0642\u06cc\u0642\u0647";
  std::u16string fa_narrow = u"\u06f1\u06f5h \u06f4\u06f2m";
  std::u16string fa_numeric = u"\u06f1\u06f5:\u06f4\u06f2";
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
  TimeDelta delta = Seconds(15 * 3600 + 42 * 60 + 30);
  EXPECT_EQ(u"15 hours, 42 minutes, 30 seconds",
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(u"15 hr, 42 min, 30 sec",
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(u"15h 42m 30s",
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(u"15:42:30",
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_NUMERIC));

  // Test edge case when hour >= 100.
  delta = Seconds(125 * 3600 + 42 * 60 + 30);
  EXPECT_EQ(u"125 hours, 42 minutes, 30 seconds",
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(u"125 hr, 42 min, 30 sec",
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(u"125h 42m 30s",
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(u"125:42:30",
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_NUMERIC));

  // Test edge case when minute = 0.
  delta = Seconds(15 * 3600 + 0 * 60 + 30);
  EXPECT_EQ(u"15 hours, 0 minutes, 30 seconds",
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(u"15 hr, 0 min, 30 sec",
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(u"15h 0m 30s",
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(u"15:00:30",
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_NUMERIC));

  // Test edge case when second = 0.
  delta = Seconds(15 * 3600 + 42 * 60 + 0);
  EXPECT_EQ(u"15 hours, 42 minutes, 0 seconds",
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(u"15 hr, 42 min, 0 sec",
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(u"15h 42m 0s",
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(u"15:42:00",
            TimeDurationFormatWithSecondsString(delta, DURATION_WIDTH_NUMERIC));
}

TEST(TimeFormattingTest, TimeDurationCompactFormatWithSeconds) {
  test::ScopedRestoreICUDefaultLocale restore_locale;

  // US English.
  i18n::SetICUDefaultLocale("en_US");

  // Test different formats.
  TimeDelta delta = Seconds(15 * 3600 + 42 * 60 + 30);
  EXPECT_EQ(
      u"15 hours, 42 minutes, 30 seconds",
      TimeDurationCompactFormatWithSecondsString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(
      u"15 hr, 42 min, 30 sec",
      TimeDurationCompactFormatWithSecondsString(delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(u"15h 42m 30s", TimeDurationCompactFormatWithSecondsString(
                                delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(u"15:42:30", TimeDurationCompactFormatWithSecondsString(
                             delta, DURATION_WIDTH_NUMERIC));

  // Test edge case when hour >= 100.
  delta = Seconds(125 * 3600 + 42 * 60 + 30);
  EXPECT_EQ(
      u"125 hours, 42 minutes, 30 seconds",
      TimeDurationCompactFormatWithSecondsString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(
      u"125 hr, 42 min, 30 sec",
      TimeDurationCompactFormatWithSecondsString(delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(u"125h 42m 30s", TimeDurationCompactFormatWithSecondsString(
                                 delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(u"125:42:30", TimeDurationCompactFormatWithSecondsString(
                              delta, DURATION_WIDTH_NUMERIC));

  // Test edge case when hour = 0.
  delta = Seconds(0 * 3600 + 7 * 60 + 30);
  EXPECT_EQ(
      u"7 minutes, 30 seconds",
      TimeDurationCompactFormatWithSecondsString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(u"7 min, 30 sec", TimeDurationCompactFormatWithSecondsString(
                                  delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(u"7m 30s", TimeDurationCompactFormatWithSecondsString(
                           delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(u"0:07:30", TimeDurationCompactFormatWithSecondsString(
                            delta, DURATION_WIDTH_NUMERIC));

  // Test edge case when hour = 1.
  delta = Seconds(1 * 3600 + 7 * 60 + 30);
  EXPECT_EQ(
      u"1 hour, 7 minutes, 30 seconds",
      TimeDurationCompactFormatWithSecondsString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(u"1 hr, 7 min, 30 sec", TimeDurationCompactFormatWithSecondsString(
                                        delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(u"1h 7m 30s", TimeDurationCompactFormatWithSecondsString(
                              delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(u"1:07:30", TimeDurationCompactFormatWithSecondsString(
                            delta, DURATION_WIDTH_NUMERIC));

  // Test edge case when minute = 0.
  delta = Seconds(15 * 3600 + 0 * 60 + 30);
  EXPECT_EQ(
      u"15 hours, 0 minutes, 30 seconds",
      TimeDurationCompactFormatWithSecondsString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(u"15 hr, 0 min, 30 sec", TimeDurationCompactFormatWithSecondsString(
                                         delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(u"15h 0m 30s", TimeDurationCompactFormatWithSecondsString(
                               delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(u"15:00:30", TimeDurationCompactFormatWithSecondsString(
                             delta, DURATION_WIDTH_NUMERIC));

  // Test edge case when minute = 1.
  delta = Seconds(15 * 3600 + 1 * 60 + 30);
  EXPECT_EQ(
      u"15 hours, 1 minute, 30 seconds",
      TimeDurationCompactFormatWithSecondsString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(u"15 hr, 1 min, 30 sec", TimeDurationCompactFormatWithSecondsString(
                                         delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(u"15h 1m 30s", TimeDurationCompactFormatWithSecondsString(
                               delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(u"15:01:30", TimeDurationCompactFormatWithSecondsString(
                             delta, DURATION_WIDTH_NUMERIC));

  // Test edge case when hour = 0 and minute = 0.
  delta = Seconds(0 * 3600 + 0 * 60 + 30);
  EXPECT_EQ(u"30 seconds", TimeDurationCompactFormatWithSecondsString(
                               delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(u"30 sec", TimeDurationCompactFormatWithSecondsString(
                           delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(u"30s", TimeDurationCompactFormatWithSecondsString(
                        delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(u"0:00:30", TimeDurationCompactFormatWithSecondsString(
                            delta, DURATION_WIDTH_NUMERIC));

  // Test edge case when second = 0.
  delta = Seconds(15 * 3600 + 42 * 60 + 0);
  EXPECT_EQ(
      u"15 hours, 42 minutes, 0 seconds",
      TimeDurationCompactFormatWithSecondsString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(u"15 hr, 42 min, 0 sec", TimeDurationCompactFormatWithSecondsString(
                                         delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(u"15h 42m 0s", TimeDurationCompactFormatWithSecondsString(
                               delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(u"15:42:00", TimeDurationCompactFormatWithSecondsString(
                             delta, DURATION_WIDTH_NUMERIC));

  // Test edge case when second = 1.
  delta = Seconds(15 * 3600 + 42 * 60 + 1);
  EXPECT_EQ(
      u"15 hours, 42 minutes, 1 second",
      TimeDurationCompactFormatWithSecondsString(delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(u"15 hr, 42 min, 1 sec", TimeDurationCompactFormatWithSecondsString(
                                         delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(u"15h 42m 1s", TimeDurationCompactFormatWithSecondsString(
                               delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(u"15:42:01", TimeDurationCompactFormatWithSecondsString(
                             delta, DURATION_WIDTH_NUMERIC));

  // Test edge case when delta = 0.
  delta = Seconds(0);
  EXPECT_EQ(u"0 seconds", TimeDurationCompactFormatWithSecondsString(
                              delta, DURATION_WIDTH_WIDE));
  EXPECT_EQ(u"0 sec", TimeDurationCompactFormatWithSecondsString(
                          delta, DURATION_WIDTH_SHORT));
  EXPECT_EQ(u"0s", TimeDurationCompactFormatWithSecondsString(
                       delta, DURATION_WIDTH_NARROW));
  EXPECT_EQ(u"0:00:00", TimeDurationCompactFormatWithSecondsString(
                            delta, DURATION_WIDTH_NUMERIC));
}

TEST(TimeFormattingTest, TimeIntervalFormat) {
  test::ScopedRestoreICUDefaultLocale restore_locale;
  i18n::SetICUDefaultLocale("en_US");
  test::ScopedRestoreDefaultTimezone la_time("America/Los_Angeles");

  const Time::Exploded kTestIntervalEndTimeExploded = {
      2011, 5,  6, 28,  // Sat, May 28, 2012
      22,   42, 7, 0    // 22:42:07.000
  };

  Time begin_time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestDateTimeExploded, &begin_time));
  Time end_time;
  EXPECT_TRUE(Time::FromUTCExploded(kTestIntervalEndTimeExploded, &end_time));

  EXPECT_EQ(
      u"Saturday, April 30\u2009–\u2009Saturday, May 28",
      DateIntervalFormat(begin_time, end_time, DATE_FORMAT_MONTH_WEEKDAY_DAY));

  const Time::Exploded kTestIntervalBeginTimeExploded = {
      2011, 5,  1, 16,  // Mon, May 16, 2012
      22,   42, 7, 0    // 22:42:07.000
  };
  EXPECT_TRUE(
      Time::FromUTCExploded(kTestIntervalBeginTimeExploded, &begin_time));
  EXPECT_EQ(
      u"Monday, May 16\u2009–\u2009Saturday, May 28",
      DateIntervalFormat(begin_time, end_time, DATE_FORMAT_MONTH_WEEKDAY_DAY));

  i18n::SetICUDefaultLocale("en_GB");
  EXPECT_EQ(
      u"Monday 16 May\u2009–\u2009Saturday 28 May",
      DateIntervalFormat(begin_time, end_time, DATE_FORMAT_MONTH_WEEKDAY_DAY));

  i18n::SetICUDefaultLocale("ja");
  EXPECT_EQ(
      u"5月16日(月曜日)～28日(土曜日)",
      DateIntervalFormat(begin_time, end_time, DATE_FORMAT_MONTH_WEEKDAY_DAY));
}

}  // namespace
}  // namespace base
