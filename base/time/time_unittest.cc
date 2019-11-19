// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"

#include <stdint.h>
#include <time.h>
#include <limits>
#include <string>

#include "base/build_time.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "base/threading/platform_thread.h"
#include "base/time/time_override.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#elif defined(OS_WIN)
#include <windows.h>
#endif

namespace base {

namespace {

TEST(TimeTestOutOfBounds, FromExplodedOutOfBoundsTime) {
  // FromUTCExploded must set time to Time(0) and failure, if the day is set to
  // 31 on a 28-30 day month. Test |exploded| returns Time(0) on 31st of
  // February and 31st of April. New implementation handles this.

  const struct DateTestData {
    Time::Exploded explode;
    bool is_valid;
  } kDateTestData[] = {
      // 31st of February
      {{2016, 2, 0, 31, 12, 30, 0, 0}, true},
      // 31st of April
      {{2016, 4, 0, 31, 8, 43, 0, 0}, true},
      // Negative month
      {{2016, -5, 0, 2, 4, 10, 0, 0}, false},
      // Negative date of month
      {{2016, 6, 0, -15, 2, 50, 0, 0}, false},
      // Negative hours
      {{2016, 7, 0, 10, -11, 29, 0, 0}, false},
      // Negative minutes
      {{2016, 3, 0, 14, 10, -29, 0, 0}, false},
      // Negative seconds
      {{2016, 10, 0, 25, 7, 47, -30, 0}, false},
      // Negative milliseconds
      {{2016, 10, 0, 25, 7, 47, 20, -500}, false},
      // Hours are too large
      {{2016, 7, 0, 10, 26, 29, 0, 0}, false},
      // Minutes are too large
      {{2016, 3, 0, 14, 10, 78, 0, 0}, false},
      // Seconds are too large
      {{2016, 10, 0, 25, 7, 47, 234, 0}, false},
      // Milliseconds are too large
      {{2016, 10, 0, 25, 6, 31, 23, 1643}, false},
      // Test overflow. Time is valid, but overflow case
      // results in Time(0).
      {{9840633, 1, 0, 1, 1, 1, 0, 0}, true},
      // Underflow will fail as well.
      {{-9840633, 1, 0, 1, 1, 1, 0, 0}, true},
      // Test integer overflow and underflow cases for the values themselves.
      {{std::numeric_limits<int>::min(), 1, 0, 1, 1, 1, 0, 0}, true},
      {{std::numeric_limits<int>::max(), 1, 0, 1, 1, 1, 0, 0}, true},
      {{2016, std::numeric_limits<int>::min(), 0, 1, 1, 1, 0, 0}, false},
      {{2016, std::numeric_limits<int>::max(), 0, 1, 1, 1, 0, 0}, false},
  };

  for (const auto& test : kDateTestData) {
    EXPECT_EQ(test.explode.HasValidValues(), test.is_valid);

    base::Time result;
    EXPECT_FALSE(base::Time::FromUTCExploded(test.explode, &result));
    EXPECT_TRUE(result.is_null());
    EXPECT_FALSE(base::Time::FromLocalExploded(test.explode, &result));
    EXPECT_TRUE(result.is_null());
  }
}

// Specialized test fixture allowing time strings without timezones to be
// tested by comparing them to a known time in the local zone.
// See also pr_time_unittests.cc
class TimeTest : public testing::Test {
 protected:
  void SetUp() override {
    // Use mktime to get a time_t, and turn it into a PRTime by converting
    // seconds to microseconds.  Use 15th Oct 2007 12:45:00 local.  This
    // must be a time guaranteed to be outside of a DST fallback hour in
    // any timezone.
    struct tm local_comparison_tm = {
      0,            // second
      45,           // minute
      12,           // hour
      15,           // day of month
      10 - 1,       // month
      2007 - 1900,  // year
      0,            // day of week (ignored, output only)
      0,            // day of year (ignored, output only)
      -1            // DST in effect, -1 tells mktime to figure it out
    };

    time_t converted_time = mktime(&local_comparison_tm);
    ASSERT_GT(converted_time, 0);
    comparison_time_local_ = Time::FromTimeT(converted_time);

    // time_t representation of 15th Oct 2007 12:45:00 PDT
    comparison_time_pdt_ = Time::FromTimeT(1192477500);
  }

  Time comparison_time_local_;
  Time comparison_time_pdt_;
};

// Test conversion to/from TimeDeltas elapsed since the Windows epoch.
// Conversions should be idempotent and non-lossy.
TEST_F(TimeTest, DeltaSinceWindowsEpoch) {
  const TimeDelta delta = TimeDelta::FromMicroseconds(123);
  EXPECT_EQ(delta,
            Time::FromDeltaSinceWindowsEpoch(delta).ToDeltaSinceWindowsEpoch());

  const Time now = Time::Now();
  const Time actual =
      Time::FromDeltaSinceWindowsEpoch(now.ToDeltaSinceWindowsEpoch());
  EXPECT_EQ(now, actual);

  // Null times should remain null after a round-trip conversion. This is an
  // important invariant for the common use case of serialization +
  // deserialization.
  const Time should_be_null =
      Time::FromDeltaSinceWindowsEpoch(Time().ToDeltaSinceWindowsEpoch());
  EXPECT_TRUE(should_be_null.is_null());
}

// Test conversion to/from time_t.
TEST_F(TimeTest, TimeT) {
  EXPECT_EQ(10, Time().FromTimeT(10).ToTimeT());
  EXPECT_EQ(10.0, Time().FromTimeT(10).ToDoubleT());

  // Conversions of 0 should stay 0.
  EXPECT_EQ(0, Time().ToTimeT());
  EXPECT_EQ(0, Time::FromTimeT(0).ToInternalValue());
}

// Test conversions to/from time_t and exploding/unexploding (utc time).
TEST_F(TimeTest, UTCTimeT) {
  // C library time and exploded time.
  time_t now_t_1 = time(nullptr);
  struct tm tms;
#if defined(OS_WIN)
  gmtime_s(&tms, &now_t_1);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  gmtime_r(&now_t_1, &tms);
#endif

  // Convert to ours.
  Time our_time_1 = Time::FromTimeT(now_t_1);
  Time::Exploded exploded;
  our_time_1.UTCExplode(&exploded);

  // This will test both our exploding and our time_t -> Time conversion.
  EXPECT_EQ(tms.tm_year + 1900, exploded.year);
  EXPECT_EQ(tms.tm_mon + 1, exploded.month);
  EXPECT_EQ(tms.tm_mday, exploded.day_of_month);
  EXPECT_EQ(tms.tm_hour, exploded.hour);
  EXPECT_EQ(tms.tm_min, exploded.minute);
  EXPECT_EQ(tms.tm_sec, exploded.second);

  // Convert exploded back to the time struct.
  Time our_time_2;
  EXPECT_TRUE(Time::FromUTCExploded(exploded, &our_time_2));
  EXPECT_TRUE(our_time_1 == our_time_2);

  time_t now_t_2 = our_time_2.ToTimeT();
  EXPECT_EQ(now_t_1, now_t_2);
}

// Test conversions to/from time_t and exploding/unexploding (local time).
TEST_F(TimeTest, LocalTimeT) {
  // C library time and exploded time.
  time_t now_t_1 = time(nullptr);
  struct tm tms;
#if defined(OS_WIN)
  localtime_s(&tms, &now_t_1);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  localtime_r(&now_t_1, &tms);
#endif

  // Convert to ours.
  Time our_time_1 = Time::FromTimeT(now_t_1);
  Time::Exploded exploded;
  our_time_1.LocalExplode(&exploded);

  // This will test both our exploding and our time_t -> Time conversion.
  EXPECT_EQ(tms.tm_year + 1900, exploded.year);
  EXPECT_EQ(tms.tm_mon + 1, exploded.month);
  EXPECT_EQ(tms.tm_mday, exploded.day_of_month);
  EXPECT_EQ(tms.tm_hour, exploded.hour);
  EXPECT_EQ(tms.tm_min, exploded.minute);
  EXPECT_EQ(tms.tm_sec, exploded.second);

  // Convert exploded back to the time struct.
  Time our_time_2;
  EXPECT_TRUE(Time::FromLocalExploded(exploded, &our_time_2));
  EXPECT_TRUE(our_time_1 == our_time_2);

  time_t now_t_2 = our_time_2.ToTimeT();
  EXPECT_EQ(now_t_1, now_t_2);
}

// Test conversions to/from javascript time.
TEST_F(TimeTest, JsTime) {
  Time epoch = Time::FromJsTime(0.0);
  EXPECT_EQ(epoch, Time::UnixEpoch());
  Time t = Time::FromJsTime(700000.3);
  EXPECT_EQ(700.0003, t.ToDoubleT());
  t = Time::FromDoubleT(800.73);
  EXPECT_EQ(800730.0, t.ToJsTime());
}

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
TEST_F(TimeTest, FromTimeVal) {
  Time now = Time::Now();
  Time also_now = Time::FromTimeVal(now.ToTimeVal());
  EXPECT_EQ(now, also_now);
}
#endif  // defined(OS_POSIX) || defined(OS_FUCHSIA)

TEST_F(TimeTest, FromExplodedWithMilliseconds) {
  // Some platform implementations of FromExploded are liable to drop
  // milliseconds if we aren't careful.
  Time now = Time::NowFromSystemTime();
  Time::Exploded exploded1 = {0};
  now.UTCExplode(&exploded1);
  exploded1.millisecond = 500;
  Time time;
  EXPECT_TRUE(Time::FromUTCExploded(exploded1, &time));
  Time::Exploded exploded2 = {0};
  time.UTCExplode(&exploded2);
  EXPECT_EQ(exploded1.millisecond, exploded2.millisecond);
}

TEST_F(TimeTest, ZeroIsSymmetric) {
  Time zero_time(Time::FromTimeT(0));
  EXPECT_EQ(0, zero_time.ToTimeT());

  EXPECT_EQ(0.0, zero_time.ToDoubleT());
}

TEST_F(TimeTest, LocalExplode) {
  Time a = Time::Now();
  Time::Exploded exploded;
  a.LocalExplode(&exploded);

  Time b;
  EXPECT_TRUE(Time::FromLocalExploded(exploded, &b));

  // The exploded structure doesn't have microseconds, and on Mac & Linux, the
  // internal OS conversion uses seconds, which will cause truncation. So we
  // can only make sure that the delta is within one second.
  EXPECT_TRUE((a - b) < TimeDelta::FromSeconds(1));
}

TEST_F(TimeTest, UTCExplode) {
  Time a = Time::Now();
  Time::Exploded exploded;
  a.UTCExplode(&exploded);

  Time b;
  EXPECT_TRUE(Time::FromUTCExploded(exploded, &b));
  EXPECT_TRUE((a - b) < TimeDelta::FromSeconds(1));
}

TEST_F(TimeTest, UTCMidnight) {
  Time::Exploded exploded;
  Time::Now().UTCMidnight().UTCExplode(&exploded);
  EXPECT_EQ(0, exploded.hour);
  EXPECT_EQ(0, exploded.minute);
  EXPECT_EQ(0, exploded.second);
  EXPECT_EQ(0, exploded.millisecond);
}

TEST_F(TimeTest, LocalMidnight) {
  Time::Exploded exploded;
  Time::Now().LocalMidnight().LocalExplode(&exploded);
  EXPECT_EQ(0, exploded.hour);
  EXPECT_EQ(0, exploded.minute);
  EXPECT_EQ(0, exploded.second);
  EXPECT_EQ(0, exploded.millisecond);
}

TEST_F(TimeTest, ParseTimeTest1) {
  time_t current_time = 0;
  time(&current_time);

  struct tm local_time = {};
  char time_buf[64] = {};
#if defined(OS_WIN)
  localtime_s(&local_time, &current_time);
  asctime_s(time_buf, base::size(time_buf), &local_time);
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
  localtime_r(&current_time, &local_time);
  asctime_r(&local_time, time_buf);
#endif

  Time parsed_time;
  EXPECT_TRUE(Time::FromString(time_buf, &parsed_time));
  EXPECT_EQ(current_time, parsed_time.ToTimeT());
}

TEST_F(TimeTest, DayOfWeekSunday) {
  Time time;
  EXPECT_TRUE(Time::FromString("Sun, 06 May 2012 12:00:00 GMT", &time));
  Time::Exploded exploded;
  time.UTCExplode(&exploded);
  EXPECT_EQ(0, exploded.day_of_week);
}

TEST_F(TimeTest, DayOfWeekWednesday) {
  Time time;
  EXPECT_TRUE(Time::FromString("Wed, 09 May 2012 12:00:00 GMT", &time));
  Time::Exploded exploded;
  time.UTCExplode(&exploded);
  EXPECT_EQ(3, exploded.day_of_week);
}

TEST_F(TimeTest, DayOfWeekSaturday) {
  Time time;
  EXPECT_TRUE(Time::FromString("Sat, 12 May 2012 12:00:00 GMT", &time));
  Time::Exploded exploded;
  time.UTCExplode(&exploded);
  EXPECT_EQ(6, exploded.day_of_week);
}

TEST_F(TimeTest, ParseTimeTest2) {
  Time parsed_time;
  EXPECT_TRUE(Time::FromString("Mon, 15 Oct 2007 19:45:00 GMT", &parsed_time));
  EXPECT_EQ(comparison_time_pdt_, parsed_time);
}

TEST_F(TimeTest, ParseTimeTest3) {
  Time parsed_time;
  EXPECT_TRUE(Time::FromString("15 Oct 07 12:45:00", &parsed_time));
  EXPECT_EQ(comparison_time_local_, parsed_time);
}

TEST_F(TimeTest, ParseTimeTest4) {
  Time parsed_time;
  EXPECT_TRUE(Time::FromString("15 Oct 07 19:45 GMT", &parsed_time));
  EXPECT_EQ(comparison_time_pdt_, parsed_time);
}

TEST_F(TimeTest, ParseTimeTest5) {
  Time parsed_time;
  EXPECT_TRUE(Time::FromString("Mon Oct 15 12:45 PDT 2007", &parsed_time));
  EXPECT_EQ(comparison_time_pdt_, parsed_time);
}

TEST_F(TimeTest, ParseTimeTest6) {
  Time parsed_time;
  EXPECT_TRUE(Time::FromString("Monday, Oct 15, 2007 12:45 PM", &parsed_time));
  EXPECT_EQ(comparison_time_local_, parsed_time);
}

TEST_F(TimeTest, ParseTimeTest7) {
  Time parsed_time;
  EXPECT_TRUE(Time::FromString("10/15/07 12:45:00 PM", &parsed_time));
  EXPECT_EQ(comparison_time_local_, parsed_time);
}

TEST_F(TimeTest, ParseTimeTest8) {
  Time parsed_time;
  EXPECT_TRUE(Time::FromString("15-OCT-2007 12:45pm", &parsed_time));
  EXPECT_EQ(comparison_time_local_, parsed_time);
}

TEST_F(TimeTest, ParseTimeTest9) {
  Time parsed_time;
  EXPECT_TRUE(Time::FromString("16 Oct 2007 4:45-JST (Tuesday)", &parsed_time));
  EXPECT_EQ(comparison_time_pdt_, parsed_time);
}

TEST_F(TimeTest, ParseTimeTest10) {
  Time parsed_time;
  EXPECT_TRUE(Time::FromString("15/10/07 12:45", &parsed_time));
  EXPECT_EQ(parsed_time, comparison_time_local_);
}

// Test some of edge cases around epoch, etc.
TEST_F(TimeTest, ParseTimeTestEpoch0) {
  Time parsed_time;

  // time_t == epoch == 0
  EXPECT_TRUE(Time::FromString("Thu Jan 01 01:00:00 +0100 1970",
                               &parsed_time));
  EXPECT_EQ(0, parsed_time.ToTimeT());
  EXPECT_TRUE(Time::FromString("Thu Jan 01 00:00:00 GMT 1970",
                               &parsed_time));
  EXPECT_EQ(0, parsed_time.ToTimeT());
}

TEST_F(TimeTest, ParseTimeTestEpoch1) {
  Time parsed_time;

  // time_t == 1 second after epoch == 1
  EXPECT_TRUE(Time::FromString("Thu Jan 01 01:00:01 +0100 1970",
                               &parsed_time));
  EXPECT_EQ(1, parsed_time.ToTimeT());
  EXPECT_TRUE(Time::FromString("Thu Jan 01 00:00:01 GMT 1970",
                               &parsed_time));
  EXPECT_EQ(1, parsed_time.ToTimeT());
}

TEST_F(TimeTest, ParseTimeTestEpoch2) {
  Time parsed_time;

  // time_t == 2 seconds after epoch == 2
  EXPECT_TRUE(Time::FromString("Thu Jan 01 01:00:02 +0100 1970",
                               &parsed_time));
  EXPECT_EQ(2, parsed_time.ToTimeT());
  EXPECT_TRUE(Time::FromString("Thu Jan 01 00:00:02 GMT 1970",
                               &parsed_time));
  EXPECT_EQ(2, parsed_time.ToTimeT());
}

TEST_F(TimeTest, ParseTimeTestEpochNeg1) {
  Time parsed_time;

  // time_t == 1 second before epoch == -1
  EXPECT_TRUE(Time::FromString("Thu Jan 01 00:59:59 +0100 1970",
                               &parsed_time));
  EXPECT_EQ(-1, parsed_time.ToTimeT());
  EXPECT_TRUE(Time::FromString("Wed Dec 31 23:59:59 GMT 1969",
                               &parsed_time));
  EXPECT_EQ(-1, parsed_time.ToTimeT());
}

// If time_t is 32 bits, a date after year 2038 will overflow time_t and
// cause timegm() to return -1.  The parsed time should not be 1 second
// before epoch.
TEST_F(TimeTest, ParseTimeTestEpochNotNeg1) {
  Time parsed_time;

  EXPECT_TRUE(Time::FromString("Wed Dec 31 23:59:59 GMT 2100",
                               &parsed_time));
  EXPECT_NE(-1, parsed_time.ToTimeT());
}

TEST_F(TimeTest, ParseTimeTestEpochNeg2) {
  Time parsed_time;

  // time_t == 2 seconds before epoch == -2
  EXPECT_TRUE(Time::FromString("Thu Jan 01 00:59:58 +0100 1970",
                               &parsed_time));
  EXPECT_EQ(-2, parsed_time.ToTimeT());
  EXPECT_TRUE(Time::FromString("Wed Dec 31 23:59:58 GMT 1969",
                               &parsed_time));
  EXPECT_EQ(-2, parsed_time.ToTimeT());
}

TEST_F(TimeTest, ParseTimeTestEpoch1960) {
  Time parsed_time;

  // time_t before Epoch, in 1960
  EXPECT_TRUE(Time::FromString("Wed Jun 29 19:40:01 +0100 1960",
                               &parsed_time));
  EXPECT_EQ(-299999999, parsed_time.ToTimeT());
  EXPECT_TRUE(Time::FromString("Wed Jun 29 18:40:01 GMT 1960",
                               &parsed_time));
  EXPECT_EQ(-299999999, parsed_time.ToTimeT());
  EXPECT_TRUE(Time::FromString("Wed Jun 29 17:40:01 GMT 1960",
                               &parsed_time));
  EXPECT_EQ(-300003599, parsed_time.ToTimeT());
}

TEST_F(TimeTest, ParseTimeTestEmpty) {
  Time parsed_time;
  EXPECT_FALSE(Time::FromString("", &parsed_time));
}

TEST_F(TimeTest, ParseTimeTestInvalidString) {
  Time parsed_time;
  EXPECT_FALSE(Time::FromString("Monday morning 2000", &parsed_time));
}

TEST_F(TimeTest, ExplodeBeforeUnixEpoch) {
  static const int kUnixEpochYear = 1970;  // In case this changes (ha!).
  Time t;
  Time::Exploded exploded;

  t = Time::UnixEpoch() - TimeDelta::FromMicroseconds(1);
  t.UTCExplode(&exploded);
  EXPECT_TRUE(exploded.HasValidValues());
  // Should be 1969-12-31 23:59:59 999 milliseconds (and 999 microseconds).
  EXPECT_EQ(kUnixEpochYear - 1, exploded.year);
  EXPECT_EQ(12, exploded.month);
  EXPECT_EQ(31, exploded.day_of_month);
  EXPECT_EQ(23, exploded.hour);
  EXPECT_EQ(59, exploded.minute);
  EXPECT_EQ(59, exploded.second);
  EXPECT_EQ(999, exploded.millisecond);

  t = Time::UnixEpoch() - TimeDelta::FromMicroseconds(1000);
  t.UTCExplode(&exploded);
  EXPECT_TRUE(exploded.HasValidValues());
  // Should be 1969-12-31 23:59:59 999 milliseconds.
  EXPECT_EQ(kUnixEpochYear - 1, exploded.year);
  EXPECT_EQ(12, exploded.month);
  EXPECT_EQ(31, exploded.day_of_month);
  EXPECT_EQ(23, exploded.hour);
  EXPECT_EQ(59, exploded.minute);
  EXPECT_EQ(59, exploded.second);
  EXPECT_EQ(999, exploded.millisecond);

  t = Time::UnixEpoch() - TimeDelta::FromMicroseconds(1001);
  t.UTCExplode(&exploded);
  EXPECT_TRUE(exploded.HasValidValues());
  // Should be 1969-12-31 23:59:59 998 milliseconds (and 999 microseconds).
  EXPECT_EQ(kUnixEpochYear - 1, exploded.year);
  EXPECT_EQ(12, exploded.month);
  EXPECT_EQ(31, exploded.day_of_month);
  EXPECT_EQ(23, exploded.hour);
  EXPECT_EQ(59, exploded.minute);
  EXPECT_EQ(59, exploded.second);
  EXPECT_EQ(998, exploded.millisecond);

  t = Time::UnixEpoch() - TimeDelta::FromMilliseconds(1000);
  t.UTCExplode(&exploded);
  EXPECT_TRUE(exploded.HasValidValues());
  // Should be 1969-12-31 23:59:59.
  EXPECT_EQ(kUnixEpochYear - 1, exploded.year);
  EXPECT_EQ(12, exploded.month);
  EXPECT_EQ(31, exploded.day_of_month);
  EXPECT_EQ(23, exploded.hour);
  EXPECT_EQ(59, exploded.minute);
  EXPECT_EQ(59, exploded.second);
  EXPECT_EQ(0, exploded.millisecond);

  t = Time::UnixEpoch() - TimeDelta::FromMilliseconds(1001);
  t.UTCExplode(&exploded);
  EXPECT_TRUE(exploded.HasValidValues());
  // Should be 1969-12-31 23:59:58 999 milliseconds.
  EXPECT_EQ(kUnixEpochYear - 1, exploded.year);
  EXPECT_EQ(12, exploded.month);
  EXPECT_EQ(31, exploded.day_of_month);
  EXPECT_EQ(23, exploded.hour);
  EXPECT_EQ(59, exploded.minute);
  EXPECT_EQ(58, exploded.second);
  EXPECT_EQ(999, exploded.millisecond);

  // Make sure we still handle at/after Unix epoch correctly.
  t = Time::UnixEpoch();
  t.UTCExplode(&exploded);
  EXPECT_TRUE(exploded.HasValidValues());
  // Should be 1970-12-31 00:00:00 0 milliseconds.
  EXPECT_EQ(kUnixEpochYear, exploded.year);
  EXPECT_EQ(1, exploded.month);
  EXPECT_EQ(1, exploded.day_of_month);
  EXPECT_EQ(0, exploded.hour);
  EXPECT_EQ(0, exploded.minute);
  EXPECT_EQ(0, exploded.second);
  EXPECT_EQ(0, exploded.millisecond);

  t = Time::UnixEpoch() + TimeDelta::FromMicroseconds(1);
  t.UTCExplode(&exploded);
  EXPECT_TRUE(exploded.HasValidValues());
  // Should be 1970-01-01 00:00:00 0 milliseconds (and 1 microsecond).
  EXPECT_EQ(kUnixEpochYear, exploded.year);
  EXPECT_EQ(1, exploded.month);
  EXPECT_EQ(1, exploded.day_of_month);
  EXPECT_EQ(0, exploded.hour);
  EXPECT_EQ(0, exploded.minute);
  EXPECT_EQ(0, exploded.second);
  EXPECT_EQ(0, exploded.millisecond);

  t = Time::UnixEpoch() + TimeDelta::FromMicroseconds(1000);
  t.UTCExplode(&exploded);
  EXPECT_TRUE(exploded.HasValidValues());
  // Should be 1970-01-01 00:00:00 1 millisecond.
  EXPECT_EQ(kUnixEpochYear, exploded.year);
  EXPECT_EQ(1, exploded.month);
  EXPECT_EQ(1, exploded.day_of_month);
  EXPECT_EQ(0, exploded.hour);
  EXPECT_EQ(0, exploded.minute);
  EXPECT_EQ(0, exploded.second);
  EXPECT_EQ(1, exploded.millisecond);

  t = Time::UnixEpoch() + TimeDelta::FromMilliseconds(1000);
  t.UTCExplode(&exploded);
  EXPECT_TRUE(exploded.HasValidValues());
  // Should be 1970-01-01 00:00:01.
  EXPECT_EQ(kUnixEpochYear, exploded.year);
  EXPECT_EQ(1, exploded.month);
  EXPECT_EQ(1, exploded.day_of_month);
  EXPECT_EQ(0, exploded.hour);
  EXPECT_EQ(0, exploded.minute);
  EXPECT_EQ(1, exploded.second);
  EXPECT_EQ(0, exploded.millisecond);

  t = Time::UnixEpoch() + TimeDelta::FromMilliseconds(1001);
  t.UTCExplode(&exploded);
  EXPECT_TRUE(exploded.HasValidValues());
  // Should be 1970-01-01 00:00:01 1 millisecond.
  EXPECT_EQ(kUnixEpochYear, exploded.year);
  EXPECT_EQ(1, exploded.month);
  EXPECT_EQ(1, exploded.day_of_month);
  EXPECT_EQ(0, exploded.hour);
  EXPECT_EQ(0, exploded.minute);
  EXPECT_EQ(1, exploded.second);
  EXPECT_EQ(1, exploded.millisecond);
}

TEST_F(TimeTest, Max) {
  Time max = Time::Max();
  EXPECT_TRUE(max.is_max());
  EXPECT_EQ(max, Time::Max());
  EXPECT_GT(max, Time::Now());
  EXPECT_GT(max, Time());
}

TEST_F(TimeTest, MaxConversions) {
  Time t = Time::Max();
  EXPECT_EQ(std::numeric_limits<int64_t>::max(), t.ToInternalValue());

  t = Time::FromDoubleT(std::numeric_limits<double>::infinity());
  EXPECT_TRUE(t.is_max());
  EXPECT_EQ(std::numeric_limits<double>::infinity(), t.ToDoubleT());

  t = Time::FromJsTime(std::numeric_limits<double>::infinity());
  EXPECT_TRUE(t.is_max());
  EXPECT_EQ(std::numeric_limits<double>::infinity(), t.ToJsTime());

  t = Time::FromTimeT(std::numeric_limits<time_t>::max());
  EXPECT_TRUE(t.is_max());
  EXPECT_EQ(std::numeric_limits<time_t>::max(), t.ToTimeT());

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  struct timeval tval;
  tval.tv_sec = std::numeric_limits<time_t>::max();
  tval.tv_usec = static_cast<suseconds_t>(Time::kMicrosecondsPerSecond) - 1;
  t = Time::FromTimeVal(tval);
  EXPECT_TRUE(t.is_max());
  tval = t.ToTimeVal();
  EXPECT_EQ(std::numeric_limits<time_t>::max(), tval.tv_sec);
  EXPECT_EQ(static_cast<suseconds_t>(Time::kMicrosecondsPerSecond) - 1,
      tval.tv_usec);
#endif

#if defined(OS_MACOSX)
  t = Time::FromCFAbsoluteTime(std::numeric_limits<CFAbsoluteTime>::infinity());
  EXPECT_TRUE(t.is_max());
  EXPECT_EQ(std::numeric_limits<CFAbsoluteTime>::infinity(),
            t.ToCFAbsoluteTime());
#endif

#if defined(OS_WIN)
  FILETIME ftime;
  ftime.dwHighDateTime = std::numeric_limits<DWORD>::max();
  ftime.dwLowDateTime = std::numeric_limits<DWORD>::max();
  t = Time::FromFileTime(ftime);
  EXPECT_TRUE(t.is_max());
  ftime = t.ToFileTime();
  EXPECT_EQ(std::numeric_limits<DWORD>::max(), ftime.dwHighDateTime);
  EXPECT_EQ(std::numeric_limits<DWORD>::max(), ftime.dwLowDateTime);
#endif
}

#if defined(OS_MACOSX)
TEST_F(TimeTest, TimeTOverflow) {
  Time t = Time::FromInternalValue(std::numeric_limits<int64_t>::max() - 1);
  EXPECT_FALSE(t.is_max());
  EXPECT_EQ(std::numeric_limits<time_t>::max(), t.ToTimeT());
}
#endif

#if defined(OS_ANDROID)
TEST_F(TimeTest, FromLocalExplodedCrashOnAndroid) {
  // This crashed inside Time:: FromLocalExploded() on Android 4.1.2.
  // See http://crbug.com/287821
  Time::Exploded midnight = {2013,  // year
                             10,    // month
                             0,     // day_of_week
                             13,    // day_of_month
                             0,     // hour
                             0,     // minute
                             0,     // second
  };
  // The string passed to putenv() must be a char* and the documentation states
  // that it 'becomes part of the environment', so use a static buffer.
  static char buffer[] = "TZ=America/Santiago";
  putenv(buffer);
  tzset();
  Time t;
  EXPECT_TRUE(Time::FromLocalExploded(midnight, &t));
  EXPECT_EQ(1381633200, t.ToTimeT());
}
#endif  // OS_ANDROID

TEST_F(TimeTest, FromExploded_MinMax) {
  Time::Exploded exploded = {0};
  exploded.month = 1;
  exploded.day_of_month = 1;

  Time parsed_time;

  if (Time::kExplodedMinYear != std::numeric_limits<int>::min()) {
    exploded.year = Time::kExplodedMinYear;
    EXPECT_TRUE(Time::FromUTCExploded(exploded, &parsed_time));
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
    // On Windows, January 1, 1601 00:00:00 is actually the null time.
    EXPECT_FALSE(parsed_time.is_null());
#endif

#if !defined(OS_ANDROID) && !defined(OS_MACOSX)
    // The dates earlier than |kExplodedMinYear| that don't work are OS version
    // dependent on Android and Mac (for example, macOS 10.13 seems to support
    // dates before 1902).
    exploded.year--;
    EXPECT_FALSE(Time::FromUTCExploded(exploded, &parsed_time));
    EXPECT_TRUE(parsed_time.is_null());
#endif
  }

  if (Time::kExplodedMaxYear != std::numeric_limits<int>::max()) {
    exploded.year = Time::kExplodedMaxYear;
    exploded.month = 12;
    exploded.day_of_month = 31;
    exploded.hour = 23;
    exploded.minute = 59;
    exploded.second = 59;
    exploded.millisecond = 999;
    EXPECT_TRUE(Time::FromUTCExploded(exploded, &parsed_time));
    EXPECT_FALSE(parsed_time.is_null());

    exploded.year++;
    EXPECT_FALSE(Time::FromUTCExploded(exploded, &parsed_time));
    EXPECT_TRUE(parsed_time.is_null());
  }
}

class TimeOverride {
 public:
  static Time Now() {
    now_time_ += TimeDelta::FromSeconds(1);
    return now_time_;
  }

  static Time now_time_;
};

// static
Time TimeOverride::now_time_;

TEST_F(TimeTest, NowOverride) {
  TimeOverride::now_time_ = Time::UnixEpoch();

  // Choose a reference time that we know to be in the past but close to now.
  Time build_time = GetBuildTime();

  // Override is not active. All Now() methods should return a time greater than
  // the build time.
  EXPECT_LT(build_time, Time::Now());
  EXPECT_GT(Time::Max(), Time::Now());
  EXPECT_LT(build_time, subtle::TimeNowIgnoringOverride());
  EXPECT_GT(Time::Max(), subtle::TimeNowIgnoringOverride());
  EXPECT_LT(build_time, Time::NowFromSystemTime());
  EXPECT_GT(Time::Max(), Time::NowFromSystemTime());
  EXPECT_LT(build_time, subtle::TimeNowFromSystemTimeIgnoringOverride());
  EXPECT_GT(Time::Max(), subtle::TimeNowFromSystemTimeIgnoringOverride());

  {
    // Set override.
    subtle::ScopedTimeClockOverrides overrides(&TimeOverride::Now, nullptr,
                                               nullptr);

    // Overridden value is returned and incremented when Now() or
    // NowFromSystemTime() is called.
    EXPECT_EQ(Time::UnixEpoch() + TimeDelta::FromSeconds(1), Time::Now());
    EXPECT_EQ(Time::UnixEpoch() + TimeDelta::FromSeconds(2), Time::Now());
    EXPECT_EQ(Time::UnixEpoch() + TimeDelta::FromSeconds(3),
              Time::NowFromSystemTime());
    EXPECT_EQ(Time::UnixEpoch() + TimeDelta::FromSeconds(4),
              Time::NowFromSystemTime());

    // IgnoringOverride methods still return real time.
    EXPECT_LT(build_time, subtle::TimeNowIgnoringOverride());
    EXPECT_GT(Time::Max(), subtle::TimeNowIgnoringOverride());
    EXPECT_LT(build_time, subtle::TimeNowFromSystemTimeIgnoringOverride());
    EXPECT_GT(Time::Max(), subtle::TimeNowFromSystemTimeIgnoringOverride());

    // IgnoringOverride methods didn't call NowOverrideClock::Now().
    EXPECT_EQ(Time::UnixEpoch() + TimeDelta::FromSeconds(5), Time::Now());
    EXPECT_EQ(Time::UnixEpoch() + TimeDelta::FromSeconds(6),
              Time::NowFromSystemTime());
  }

  // All methods return real time again.
  EXPECT_LT(build_time, Time::Now());
  EXPECT_GT(Time::Max(), Time::Now());
  EXPECT_LT(build_time, subtle::TimeNowIgnoringOverride());
  EXPECT_GT(Time::Max(), subtle::TimeNowIgnoringOverride());
  EXPECT_LT(build_time, Time::NowFromSystemTime());
  EXPECT_GT(Time::Max(), Time::NowFromSystemTime());
  EXPECT_LT(build_time, subtle::TimeNowFromSystemTimeIgnoringOverride());
  EXPECT_GT(Time::Max(), subtle::TimeNowFromSystemTimeIgnoringOverride());
}

#if defined(OS_FUCHSIA)
TEST(ZxTimeTest, ToFromConversions) {
  Time unix_epoch = Time::UnixEpoch();
  EXPECT_EQ(unix_epoch.ToZxTime(), 0);
  EXPECT_EQ(Time::FromZxTime(6000000000),
            unix_epoch + TimeDelta::FromSeconds(6));

  TimeTicks ticks_now = TimeTicks::Now();
  EXPECT_GE(ticks_now.ToZxTime(), 0);
  TimeTicks ticks_later = ticks_now + TimeDelta::FromSeconds(2);
  EXPECT_EQ((ticks_later.ToZxTime() - ticks_now.ToZxTime()), 2000000000);
  EXPECT_EQ(TimeTicks::FromZxTime(3000000000),
            TimeTicks() + TimeDelta::FromSeconds(3));

  EXPECT_EQ(TimeDelta().ToZxDuration(), 0);
  EXPECT_EQ(TimeDelta::FromZxDuration(0), TimeDelta());

  EXPECT_EQ(TimeDelta::FromSeconds(2).ToZxDuration(), 2000000000);
  EXPECT_EQ(TimeDelta::FromZxDuration(4000000000), TimeDelta::FromSeconds(4));
}
#endif  // defined(OS_FUCHSIA)

TEST(TimeTicks, Deltas) {
  for (int index = 0; index < 50; index++) {
    TimeTicks ticks_start = TimeTicks::Now();
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(10));
    TimeTicks ticks_stop = TimeTicks::Now();
    TimeDelta delta = ticks_stop - ticks_start;
    // Note:  Although we asked for a 10ms sleep, if the
    // time clock has a finer granularity than the Sleep()
    // clock, it is quite possible to wakeup early.  Here
    // is how that works:
    //      Time(ms timer)      Time(us timer)
    //          5                   5010
    //          6                   6010
    //          7                   7010
    //          8                   8010
    //          9                   9000
    // Elapsed  4ms                 3990us
    //
    // Unfortunately, our InMilliseconds() function truncates
    // rather than rounds.  We should consider fixing this
    // so that our averages come out better.
    EXPECT_GE(delta.InMilliseconds(), 9);
    EXPECT_GE(delta.InMicroseconds(), 9000);
    EXPECT_EQ(delta.InSeconds(), 0);
  }
}

static void HighResClockTest(TimeTicks (*GetTicks)()) {
  // IsHighResolution() is false on some systems.  Since the product still works
  // even if it's false, it makes this entire test questionable.
  if (!TimeTicks::IsHighResolution())
    return;

  // Why do we loop here?
  // We're trying to measure that intervals increment in a VERY small amount
  // of time --  less than 15ms.  Unfortunately, if we happen to have a
  // context switch in the middle of our test, the context switch could easily
  // exceed our limit.  So, we iterate on this several times.  As long as we're
  // able to detect the fine-granularity timers at least once, then the test
  // has succeeded.

  const int kTargetGranularityUs = 15000;  // 15ms

  bool success = false;
  int retries = 100;  // Arbitrary.
  TimeDelta delta;
  while (!success && retries--) {
    TimeTicks ticks_start = GetTicks();
    // Loop until we can detect that the clock has changed.  Non-HighRes timers
    // will increment in chunks, e.g. 15ms.  By spinning until we see a clock
    // change, we detect the minimum time between measurements.
    do {
      delta = GetTicks() - ticks_start;
    } while (delta.InMilliseconds() == 0);

    if (delta.InMicroseconds() <= kTargetGranularityUs)
      success = true;
  }

  // In high resolution mode, we expect to see the clock increment
  // in intervals less than 15ms.
  EXPECT_TRUE(success);
}

TEST(TimeTicks, HighRes) {
  HighResClockTest(&TimeTicks::Now);
}

class TimeTicksOverride {
 public:
  static TimeTicks Now() {
    now_ticks_ += TimeDelta::FromSeconds(1);
    return now_ticks_;
  }

  static TimeTicks now_ticks_;
};

// static
TimeTicks TimeTicksOverride::now_ticks_;

TEST(TimeTicks, NowOverride) {
  TimeTicksOverride::now_ticks_ = TimeTicks::Min();

  // Override is not active. All Now() methods should return a sensible value.
  EXPECT_LT(TimeTicks::Min(), TimeTicks::UnixEpoch());
  EXPECT_LT(TimeTicks::UnixEpoch(), TimeTicks::Now());
  EXPECT_GT(TimeTicks::Max(), TimeTicks::Now());
  EXPECT_LT(TimeTicks::UnixEpoch(), subtle::TimeTicksNowIgnoringOverride());
  EXPECT_GT(TimeTicks::Max(), subtle::TimeTicksNowIgnoringOverride());

  {
    // Set override.
    subtle::ScopedTimeClockOverrides overrides(nullptr, &TimeTicksOverride::Now,
                                               nullptr);

    // Overridden value is returned and incremented when Now() is called.
    EXPECT_EQ(TimeTicks::Min() + TimeDelta::FromSeconds(1), TimeTicks::Now());
    EXPECT_EQ(TimeTicks::Min() + TimeDelta::FromSeconds(2), TimeTicks::Now());

    // NowIgnoringOverride() still returns real ticks.
    EXPECT_LT(TimeTicks::UnixEpoch(), subtle::TimeTicksNowIgnoringOverride());
    EXPECT_GT(TimeTicks::Max(), subtle::TimeTicksNowIgnoringOverride());

    // IgnoringOverride methods didn't call NowOverrideTickClock::NowTicks().
    EXPECT_EQ(TimeTicks::Min() + TimeDelta::FromSeconds(3), TimeTicks::Now());
  }

  // All methods return real ticks again.
  EXPECT_LT(TimeTicks::UnixEpoch(), TimeTicks::Now());
  EXPECT_GT(TimeTicks::Max(), TimeTicks::Now());
  EXPECT_LT(TimeTicks::UnixEpoch(), subtle::TimeTicksNowIgnoringOverride());
  EXPECT_GT(TimeTicks::Max(), subtle::TimeTicksNowIgnoringOverride());
}

class ThreadTicksOverride {
 public:
  static ThreadTicks Now() {
    now_ticks_ += TimeDelta::FromSeconds(1);
    return now_ticks_;
  }

  static ThreadTicks now_ticks_;
};

// static
ThreadTicks ThreadTicksOverride::now_ticks_;

// IOS doesn't support ThreadTicks::Now().
#if defined(OS_IOS)
#define MAYBE_NowOverride DISABLED_NowOverride
#else
#define MAYBE_NowOverride NowOverride
#endif
TEST(ThreadTicks, MAYBE_NowOverride) {
  ThreadTicksOverride::now_ticks_ = ThreadTicks::Min();

  // Override is not active. All Now() methods should return a sensible value.
  ThreadTicks initial_thread_ticks = ThreadTicks::Now();
  EXPECT_LE(initial_thread_ticks, ThreadTicks::Now());
  EXPECT_GT(ThreadTicks::Max(), ThreadTicks::Now());
  EXPECT_LE(initial_thread_ticks, subtle::ThreadTicksNowIgnoringOverride());
  EXPECT_GT(ThreadTicks::Max(), subtle::ThreadTicksNowIgnoringOverride());

  {
    // Set override.
    subtle::ScopedTimeClockOverrides overrides(nullptr, nullptr,
                                               &ThreadTicksOverride::Now);

    // Overridden value is returned and incremented when Now() is called.
    EXPECT_EQ(ThreadTicks::Min() + TimeDelta::FromSeconds(1),
              ThreadTicks::Now());
    EXPECT_EQ(ThreadTicks::Min() + TimeDelta::FromSeconds(2),
              ThreadTicks::Now());

    // NowIgnoringOverride() still returns real ticks.
    EXPECT_LE(initial_thread_ticks, subtle::ThreadTicksNowIgnoringOverride());
    EXPECT_GT(ThreadTicks::Max(), subtle::ThreadTicksNowIgnoringOverride());

    // IgnoringOverride methods didn't call NowOverrideTickClock::NowTicks().
    EXPECT_EQ(ThreadTicks::Min() + TimeDelta::FromSeconds(3),
              ThreadTicks::Now());
  }

  // All methods return real ticks again.
  EXPECT_LE(initial_thread_ticks, ThreadTicks::Now());
  EXPECT_GT(ThreadTicks::Max(), ThreadTicks::Now());
  EXPECT_LE(initial_thread_ticks, subtle::ThreadTicksNowIgnoringOverride());
  EXPECT_GT(ThreadTicks::Max(), subtle::ThreadTicksNowIgnoringOverride());
}

TEST(ThreadTicks, ThreadNow) {
  if (ThreadTicks::IsSupported()) {
    ThreadTicks::WaitUntilInitialized();
    TimeTicks begin = TimeTicks::Now();
    ThreadTicks begin_thread = ThreadTicks::Now();
    // Make sure that ThreadNow value is non-zero.
    EXPECT_GT(begin_thread, ThreadTicks());
    // Sleep for 10 milliseconds to get the thread de-scheduled.
    base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(10));
    ThreadTicks end_thread = ThreadTicks::Now();
    TimeTicks end = TimeTicks::Now();
    TimeDelta delta = end - begin;
    TimeDelta delta_thread = end_thread - begin_thread;
    // Make sure that some thread time have elapsed.
    EXPECT_GE(delta_thread.InMicroseconds(), 0);
    // But the thread time is at least 9ms less than clock time.
    TimeDelta difference = delta - delta_thread;
    EXPECT_GE(difference.InMicroseconds(), 9000);
  }
}

TEST(TimeTicks, SnappedToNextTickBasic) {
  base::TimeTicks phase = base::TimeTicks::FromInternalValue(4000);
  base::TimeDelta interval = base::TimeDelta::FromMicroseconds(1000);
  base::TimeTicks timestamp;

  // Timestamp in previous interval.
  timestamp = base::TimeTicks::FromInternalValue(3500);
  EXPECT_EQ(4000,
            timestamp.SnappedToNextTick(phase, interval).ToInternalValue());

  // Timestamp in next interval.
  timestamp = base::TimeTicks::FromInternalValue(4500);
  EXPECT_EQ(5000,
            timestamp.SnappedToNextTick(phase, interval).ToInternalValue());

  // Timestamp multiple intervals before.
  timestamp = base::TimeTicks::FromInternalValue(2500);
  EXPECT_EQ(3000,
            timestamp.SnappedToNextTick(phase, interval).ToInternalValue());

  // Timestamp multiple intervals after.
  timestamp = base::TimeTicks::FromInternalValue(6500);
  EXPECT_EQ(7000,
            timestamp.SnappedToNextTick(phase, interval).ToInternalValue());

  // Timestamp on previous interval.
  timestamp = base::TimeTicks::FromInternalValue(3000);
  EXPECT_EQ(3000,
            timestamp.SnappedToNextTick(phase, interval).ToInternalValue());

  // Timestamp on next interval.
  timestamp = base::TimeTicks::FromInternalValue(5000);
  EXPECT_EQ(5000,
            timestamp.SnappedToNextTick(phase, interval).ToInternalValue());

  // Timestamp equal to phase.
  timestamp = base::TimeTicks::FromInternalValue(4000);
  EXPECT_EQ(4000,
            timestamp.SnappedToNextTick(phase, interval).ToInternalValue());
}

TEST(TimeTicks, SnappedToNextTickOverflow) {
  // int(big_timestamp / interval) < 0, so this causes a crash if the number of
  // intervals elapsed is attempted to be stored in an int.
  base::TimeTicks phase = base::TimeTicks::FromInternalValue(0);
  base::TimeDelta interval = base::TimeDelta::FromMicroseconds(4000);
  base::TimeTicks big_timestamp =
      base::TimeTicks::FromInternalValue(8635916564000);

  EXPECT_EQ(8635916564000,
            big_timestamp.SnappedToNextTick(phase, interval).ToInternalValue());
  EXPECT_EQ(8635916564000,
            big_timestamp.SnappedToNextTick(big_timestamp, interval)
                .ToInternalValue());
}

#if defined(OS_ANDROID)
TEST(TimeTicks, Android_FromUptimeMillis_ClocksMatch) {
  JNIEnv* const env = android::AttachCurrentThread();
  android::ScopedJavaLocalRef<jclass> clazz(
      android::GetClass(env, "android/os/SystemClock"));
  ASSERT_TRUE(clazz.obj());
  const jmethodID method_id =
      android::MethodID::Get<android::MethodID::TYPE_STATIC>(
          env, clazz.obj(), "uptimeMillis", "()J");
  ASSERT_FALSE(!method_id);
  // Subtract 1ms from the expected lower bound to allow millisecon-level
  // truncation performed in uptimeMillis().
  const TimeTicks lower_bound_ticks =
      TimeTicks::Now() - TimeDelta::FromMilliseconds(1);
  const TimeTicks converted_ticks = TimeTicks::FromUptimeMillis(
      env->CallStaticLongMethod(clazz.obj(), method_id));
  const TimeTicks upper_bound_ticks = TimeTicks::Now();
  EXPECT_LE(lower_bound_ticks, converted_ticks);
  EXPECT_GE(upper_bound_ticks, converted_ticks);
}
#endif  // OS_ANDROID

TEST(TimeDelta, FromAndIn) {
  // static_assert also checks that the contained expression is a constant
  // expression, meaning all its components are suitable for initializing global
  // variables.
  static_assert(TimeDelta::FromDays(2) == TimeDelta::FromHours(48), "");
  static_assert(TimeDelta::FromHours(3) == TimeDelta::FromMinutes(180), "");
  static_assert(TimeDelta::FromMinutes(2) == TimeDelta::FromSeconds(120), "");
  static_assert(TimeDelta::FromSeconds(2) == TimeDelta::FromMilliseconds(2000),
                "");
  static_assert(
      TimeDelta::FromMilliseconds(2) == TimeDelta::FromMicroseconds(2000), "");
  static_assert(
      TimeDelta::FromSecondsD(2.3) == TimeDelta::FromMilliseconds(2300), "");
  static_assert(
      TimeDelta::FromMillisecondsD(2.5) == TimeDelta::FromMicroseconds(2500),
      "");
  EXPECT_EQ(TimeDelta::FromDays(13).InDays(), 13);
  EXPECT_EQ(TimeDelta::FromHours(13).InHours(), 13);
  EXPECT_EQ(TimeDelta::FromMinutes(13).InMinutes(), 13);
  EXPECT_EQ(TimeDelta::FromSeconds(13).InSeconds(), 13);
  EXPECT_EQ(TimeDelta::FromSeconds(13).InSecondsF(), 13.0);
  EXPECT_EQ(TimeDelta::FromMilliseconds(13).InMilliseconds(), 13);
  EXPECT_EQ(TimeDelta::FromMilliseconds(13).InMillisecondsF(), 13.0);
  EXPECT_EQ(TimeDelta::FromSecondsD(13.1).InSeconds(), 13);
  EXPECT_EQ(TimeDelta::FromSecondsD(13.1).InSecondsF(), 13.1);
  EXPECT_EQ(TimeDelta::FromMillisecondsD(13.3).InMilliseconds(), 13);
  EXPECT_EQ(TimeDelta::FromMillisecondsD(13.3).InMillisecondsF(), 13.3);
  EXPECT_EQ(TimeDelta::FromMicroseconds(13).InMicroseconds(), 13);
  EXPECT_EQ(TimeDelta::FromMicrosecondsD(13.3).InMicroseconds(), 13);
  EXPECT_EQ(TimeDelta::FromMillisecondsD(3.45678).InMillisecondsF(), 3.456);
  EXPECT_EQ(TimeDelta::FromNanoseconds(12345).InNanoseconds(), 12000);
  EXPECT_EQ(TimeDelta::FromNanosecondsD(12345.678).InNanoseconds(), 12000);
}

TEST(TimeDelta, InRoundsTowardsZero) {
  EXPECT_EQ(TimeDelta::FromHours(23).InDays(), 0);
  EXPECT_EQ(TimeDelta::FromHours(-23).InDays(), 0);
  EXPECT_EQ(TimeDelta::FromMinutes(59).InHours(), 0);
  EXPECT_EQ(TimeDelta::FromMinutes(-59).InHours(), 0);
  EXPECT_EQ(TimeDelta::FromSeconds(59).InMinutes(), 0);
  EXPECT_EQ(TimeDelta::FromSeconds(-59).InMinutes(), 0);
  EXPECT_EQ(TimeDelta::FromMilliseconds(999).InSeconds(), 0);
  EXPECT_EQ(TimeDelta::FromMilliseconds(-999).InSeconds(), 0);
  EXPECT_EQ(TimeDelta::FromMicroseconds(999).InMilliseconds(), 0);
  EXPECT_EQ(TimeDelta::FromMicroseconds(-999).InMilliseconds(), 0);
}

TEST(TimeDelta, InDaysFloored) {
  EXPECT_EQ(TimeDelta::FromHours(-25).InDaysFloored(), -2);
  EXPECT_EQ(TimeDelta::FromHours(-24).InDaysFloored(), -1);
  EXPECT_EQ(TimeDelta::FromHours(-23).InDaysFloored(), -1);

  EXPECT_EQ(TimeDelta::FromHours(-1).InDaysFloored(), -1);
  EXPECT_EQ(TimeDelta::FromHours(0).InDaysFloored(), 0);
  EXPECT_EQ(TimeDelta::FromHours(1).InDaysFloored(), 0);

  EXPECT_EQ(TimeDelta::FromHours(23).InDaysFloored(), 0);
  EXPECT_EQ(TimeDelta::FromHours(24).InDaysFloored(), 1);
  EXPECT_EQ(TimeDelta::FromHours(25).InDaysFloored(), 1);
}

TEST(TimeDelta, InMillisecondsRoundedUp) {
  EXPECT_EQ(TimeDelta::FromMicroseconds(-1001).InMillisecondsRoundedUp(), -1);
  EXPECT_EQ(TimeDelta::FromMicroseconds(-1000).InMillisecondsRoundedUp(), -1);
  EXPECT_EQ(TimeDelta::FromMicroseconds(-999).InMillisecondsRoundedUp(), 0);

  EXPECT_EQ(TimeDelta::FromMicroseconds(-1).InMillisecondsRoundedUp(), 0);
  EXPECT_EQ(TimeDelta::FromMicroseconds(0).InMillisecondsRoundedUp(), 0);
  EXPECT_EQ(TimeDelta::FromMicroseconds(1).InMillisecondsRoundedUp(), 1);

  EXPECT_EQ(TimeDelta::FromMicroseconds(999).InMillisecondsRoundedUp(), 1);
  EXPECT_EQ(TimeDelta::FromMicroseconds(1000).InMillisecondsRoundedUp(), 1);
  EXPECT_EQ(TimeDelta::FromMicroseconds(1001).InMillisecondsRoundedUp(), 2);
}

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
TEST(TimeDelta, TimeSpecConversion) {
  TimeDelta delta = TimeDelta::FromSeconds(0);
  struct timespec result = delta.ToTimeSpec();
  EXPECT_EQ(result.tv_sec, 0);
  EXPECT_EQ(result.tv_nsec, 0);
  EXPECT_EQ(delta, TimeDelta::FromTimeSpec(result));

  delta = TimeDelta::FromSeconds(1);
  result = delta.ToTimeSpec();
  EXPECT_EQ(result.tv_sec, 1);
  EXPECT_EQ(result.tv_nsec, 0);
  EXPECT_EQ(delta, TimeDelta::FromTimeSpec(result));

  delta = TimeDelta::FromMicroseconds(1);
  result = delta.ToTimeSpec();
  EXPECT_EQ(result.tv_sec, 0);
  EXPECT_EQ(result.tv_nsec, 1000);
  EXPECT_EQ(delta, TimeDelta::FromTimeSpec(result));

  delta = TimeDelta::FromMicroseconds(Time::kMicrosecondsPerSecond + 1);
  result = delta.ToTimeSpec();
  EXPECT_EQ(result.tv_sec, 1);
  EXPECT_EQ(result.tv_nsec, 1000);
  EXPECT_EQ(delta, TimeDelta::FromTimeSpec(result));
}
#endif  // defined(OS_POSIX) || defined(OS_FUCHSIA)

// Our internal time format is serialized in things like databases, so it's
// important that it's consistent across all our platforms.  We use the 1601
// Windows epoch as the internal format across all platforms.
TEST(TimeDelta, WindowsEpoch) {
  Time::Exploded exploded;
  exploded.year = 1970;
  exploded.month = 1;
  exploded.day_of_week = 0;  // Should be unusued.
  exploded.day_of_month = 1;
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;
  Time t;
  EXPECT_TRUE(Time::FromUTCExploded(exploded, &t));
  // Unix 1970 epoch.
  EXPECT_EQ(INT64_C(11644473600000000), t.ToInternalValue());

  // We can't test 1601 epoch, since the system time functions on Linux
  // only compute years starting from 1900.
}

// We could define this separately for Time, TimeTicks and TimeDelta but the
// definitions would be identical anyway.
template <class Any>
std::string AnyToString(Any any) {
  std::ostringstream oss;
  oss << any;
  return oss.str();
}

TEST(TimeDelta, Magnitude) {
  constexpr int64_t zero = 0;
  static_assert(TimeDelta::FromMicroseconds(zero) ==
                    TimeDelta::FromMicroseconds(zero).magnitude(),
                "");

  constexpr int64_t one = 1;
  constexpr int64_t negative_one = -1;
  static_assert(TimeDelta::FromMicroseconds(one) ==
                    TimeDelta::FromMicroseconds(one).magnitude(),
                "");
  static_assert(TimeDelta::FromMicroseconds(one) ==
                    TimeDelta::FromMicroseconds(negative_one).magnitude(),
                "");

  constexpr int64_t max_int64_minus_one =
      std::numeric_limits<int64_t>::max() - 1;
  constexpr int64_t min_int64_plus_two =
      std::numeric_limits<int64_t>::min() + 2;
  static_assert(
      TimeDelta::FromMicroseconds(max_int64_minus_one) ==
          TimeDelta::FromMicroseconds(max_int64_minus_one).magnitude(),
      "");
  static_assert(TimeDelta::FromMicroseconds(max_int64_minus_one) ==
                    TimeDelta::FromMicroseconds(min_int64_plus_two).magnitude(),
                "");
}

TEST(TimeDelta, ZeroMinMax) {
  constexpr TimeDelta kZero;
  static_assert(kZero.is_zero(), "");

  constexpr TimeDelta kMax = TimeDelta::Max();
  static_assert(kMax.is_max(), "");
  static_assert(kMax == TimeDelta::Max(), "");
  static_assert(kMax > TimeDelta::FromDays(100 * 365), "");
  static_assert(kMax > kZero, "");

  constexpr TimeDelta kMin = TimeDelta::Min();
  static_assert(kMin.is_min(), "");
  static_assert(kMin == TimeDelta::Min(), "");
  static_assert(kMin < TimeDelta::FromDays(-100 * 365), "");
  static_assert(kMin < kZero, "");
}

TEST(TimeDelta, MaxConversions) {
  // static_assert also confirms constexpr works as intended.
  constexpr TimeDelta kMax = TimeDelta::Max();
  static_assert(kMax.ToInternalValue() == std::numeric_limits<int64_t>::max(),
                "");
  EXPECT_EQ(kMax.InDays(), std::numeric_limits<int>::max());
  EXPECT_EQ(kMax.InHours(), std::numeric_limits<int>::max());
  EXPECT_EQ(kMax.InMinutes(), std::numeric_limits<int>::max());
  EXPECT_EQ(kMax.InSecondsF(), std::numeric_limits<double>::infinity());
  EXPECT_EQ(kMax.InSeconds(), std::numeric_limits<int64_t>::max());
  EXPECT_EQ(kMax.InMillisecondsF(), std::numeric_limits<double>::infinity());
  EXPECT_EQ(kMax.InMilliseconds(), std::numeric_limits<int64_t>::max());
  EXPECT_EQ(kMax.InMillisecondsRoundedUp(), std::numeric_limits<int64_t>::max());

  static_assert(TimeDelta::FromDays(std::numeric_limits<int>::max()).is_max(),
                "");

  static_assert(TimeDelta::FromHours(std::numeric_limits<int>::max()).is_max(),
                "");

  static_assert(
      TimeDelta::FromMinutes(std::numeric_limits<int>::max()).is_max(), "");

  constexpr int64_t max_int = std::numeric_limits<int64_t>::max();
  constexpr int64_t min_int = std::numeric_limits<int64_t>::min();

  static_assert(
      TimeDelta::FromSeconds(max_int / Time::kMicrosecondsPerSecond + 1)
          .is_max(),
      "");

  static_assert(
      TimeDelta::FromMilliseconds(max_int / Time::kMillisecondsPerSecond + 1)
          .is_max(),
      "");

  static_assert(TimeDelta::FromMicroseconds(max_int).is_max(), "");

  static_assert(
      TimeDelta::FromSeconds(min_int / Time::kMicrosecondsPerSecond - 1)
          .is_min(),
      "");

  static_assert(
      TimeDelta::FromMilliseconds(min_int / Time::kMillisecondsPerSecond - 1)
          .is_min(),
      "");

  static_assert(TimeDelta::FromMicroseconds(min_int).is_min(), "");

  static_assert(
      TimeDelta::FromMicroseconds(std::numeric_limits<int64_t>::min()).is_min(),
      "");

  // Floating point arithmetic resulting in infinity isn't constexpr in C++14.
  EXPECT_TRUE(TimeDelta::FromSecondsD(std::numeric_limits<double>::infinity())
                  .is_max());

  // Note that max_int/min_int will be rounded when converted to doubles - they
  // can't be exactly represented.
  constexpr double max_d = static_cast<double>(max_int);
  constexpr double min_d = static_cast<double>(min_int);

  static_assert(
      TimeDelta::FromSecondsD(max_d / Time::kMicrosecondsPerSecond + 1)
          .is_max(),
      "");

  static_assert(
      TimeDelta::FromMicrosecondsD(max_d).is_max(),
      "Make sure that 2^63 correctly gets clamped to `max` (crbug.com/612601)");

  // Floating point arithmetic resulting in infinity isn't constexpr in C++14.
  EXPECT_TRUE(
      TimeDelta::FromMillisecondsD(std::numeric_limits<double>::infinity())
          .is_max());

  static_assert(
      TimeDelta::FromMillisecondsD(max_d / Time::kMillisecondsPerSecond * 2)
          .is_max(),
      "");

  static_assert(
      TimeDelta::FromSecondsD(min_d / Time::kMicrosecondsPerSecond - 1)
          .is_min(),
      "");

  static_assert(
      TimeDelta::FromMillisecondsD(min_d / Time::kMillisecondsPerSecond * 2)
          .is_min(),
      "");
}

TEST(TimeDelta, NumericOperators) {
  constexpr double d = 0.5;
  EXPECT_EQ(TimeDelta::FromMilliseconds(500),
            (TimeDelta::FromMilliseconds(1000) * d));
  static_assert(TimeDelta::FromMilliseconds(2000) ==
                    (TimeDelta::FromMilliseconds(1000) / d),
                "");
  EXPECT_EQ(TimeDelta::FromMilliseconds(500),
            (TimeDelta::FromMilliseconds(1000) *= d));
  static_assert(TimeDelta::FromMilliseconds(2000) ==
                    (TimeDelta::FromMilliseconds(1000) /= d),
                "");
  EXPECT_EQ(TimeDelta::FromMilliseconds(500),
            (d * TimeDelta::FromMilliseconds(1000)));

  constexpr float f = 0.5;
  EXPECT_EQ(TimeDelta::FromMilliseconds(500),
            (TimeDelta::FromMilliseconds(1000) * f));
  static_assert(TimeDelta::FromMilliseconds(2000) ==
                    (TimeDelta::FromMilliseconds(1000) / f),
                "");
  EXPECT_EQ(TimeDelta::FromMilliseconds(500),
            (TimeDelta::FromMilliseconds(1000) *= f));
  static_assert(TimeDelta::FromMilliseconds(2000) ==
                    (TimeDelta::FromMilliseconds(1000) /= f),
                "");
  EXPECT_EQ(TimeDelta::FromMilliseconds(500),
            (f * TimeDelta::FromMilliseconds(1000)));

  constexpr int i = 2;
  EXPECT_EQ(TimeDelta::FromMilliseconds(2000),
            (TimeDelta::FromMilliseconds(1000) * i));
  static_assert(TimeDelta::FromMilliseconds(500) ==
                    (TimeDelta::FromMilliseconds(1000) / i),
                "");
  EXPECT_EQ(TimeDelta::FromMilliseconds(2000),
            (TimeDelta::FromMilliseconds(1000) *= i));
  static_assert(TimeDelta::FromMilliseconds(500) ==
                    (TimeDelta::FromMilliseconds(1000) /= i),
                "");
  EXPECT_EQ(TimeDelta::FromMilliseconds(2000),
            (i * TimeDelta::FromMilliseconds(1000)));

  constexpr int64_t i64 = 2;
  EXPECT_EQ(TimeDelta::FromMilliseconds(2000),
            (TimeDelta::FromMilliseconds(1000) * i64));
  static_assert(TimeDelta::FromMilliseconds(500) ==
                    (TimeDelta::FromMilliseconds(1000) / i64),
                "");
  EXPECT_EQ(TimeDelta::FromMilliseconds(2000),
            (TimeDelta::FromMilliseconds(1000) *= i64));
  static_assert(TimeDelta::FromMilliseconds(500) ==
                    (TimeDelta::FromMilliseconds(1000) /= i64),
                "");
  EXPECT_EQ(TimeDelta::FromMilliseconds(2000),
            (i64 * TimeDelta::FromMilliseconds(1000)));

  EXPECT_EQ(TimeDelta::FromMilliseconds(500),
            (TimeDelta::FromMilliseconds(1000) * 0.5));
  static_assert(TimeDelta::FromMilliseconds(2000) ==
                    (TimeDelta::FromMilliseconds(1000) / 0.5),
                "");
  EXPECT_EQ(TimeDelta::FromMilliseconds(500),
            (TimeDelta::FromMilliseconds(1000) *= 0.5));
  static_assert(TimeDelta::FromMilliseconds(2000) ==
                    (TimeDelta::FromMilliseconds(1000) /= 0.5),
                "");
  EXPECT_EQ(TimeDelta::FromMilliseconds(500),
            (0.5 * TimeDelta::FromMilliseconds(1000)));

  EXPECT_EQ(TimeDelta::FromMilliseconds(2000),
            (TimeDelta::FromMilliseconds(1000) * 2));
  static_assert(TimeDelta::FromMilliseconds(500) ==
                    (TimeDelta::FromMilliseconds(1000) / 2),
                "");
  EXPECT_EQ(TimeDelta::FromMilliseconds(2000),
            (TimeDelta::FromMilliseconds(1000) *= 2));
  static_assert(TimeDelta::FromMilliseconds(500) ==
                    (TimeDelta::FromMilliseconds(1000) /= 2),
                "");
  EXPECT_EQ(TimeDelta::FromMilliseconds(2000),
            (2 * TimeDelta::FromMilliseconds(1000)));
}

// Basic test of operators between TimeDeltas (without overflow -- next test
// handles overflow).
TEST(TimeDelta, TimeDeltaOperators) {
  constexpr TimeDelta kElevenSeconds = TimeDelta::FromSeconds(11);
  constexpr TimeDelta kThreeSeconds = TimeDelta::FromSeconds(3);

  EXPECT_EQ(TimeDelta::FromSeconds(14), kElevenSeconds + kThreeSeconds);
  EXPECT_EQ(TimeDelta::FromSeconds(14), kThreeSeconds + kElevenSeconds);
  EXPECT_EQ(TimeDelta::FromSeconds(8), kElevenSeconds - kThreeSeconds);
  EXPECT_EQ(TimeDelta::FromSeconds(-8), kThreeSeconds - kElevenSeconds);
  static_assert(3 == kElevenSeconds / kThreeSeconds, "");
  static_assert(0 == kThreeSeconds / kElevenSeconds, "");
  static_assert(TimeDelta::FromSeconds(2) == kElevenSeconds % kThreeSeconds,
                "");
}

TEST(TimeDelta, Overflows) {
  // Some sanity checks. static_assert's used were possible to verify constexpr
  // evaluation at the same time.
  static_assert(TimeDelta::Max().is_max(), "");
  static_assert(-TimeDelta::Max() < TimeDelta(), "");
  static_assert(-TimeDelta::Max() > TimeDelta::Min(), "");
  static_assert(TimeDelta() > -TimeDelta::Max(), "");

  TimeDelta large_delta = TimeDelta::Max() - TimeDelta::FromMilliseconds(1);
  TimeDelta large_negative = -large_delta;
  EXPECT_GT(TimeDelta(), large_negative);
  EXPECT_FALSE(large_delta.is_max());
  EXPECT_FALSE((-large_negative).is_min());
  constexpr TimeDelta kOneSecond = TimeDelta::FromSeconds(1);

  // Test +, -, * and / operators.
  EXPECT_TRUE((large_delta + kOneSecond).is_max());
  EXPECT_TRUE((large_negative + (-kOneSecond)).is_min());
  EXPECT_TRUE((large_negative - kOneSecond).is_min());
  EXPECT_TRUE((large_delta - (-kOneSecond)).is_max());
  EXPECT_TRUE((large_delta * 2).is_max());
  EXPECT_TRUE((large_delta * -2).is_min());
  EXPECT_TRUE((large_delta / 0.5).is_max());
  EXPECT_TRUE((large_delta / -0.5).is_min());

  // Test that double conversions overflow to infinity.
  EXPECT_EQ((large_delta + kOneSecond).InSecondsF(),
            std::numeric_limits<double>::infinity());
  EXPECT_EQ((large_delta + kOneSecond).InMillisecondsF(),
            std::numeric_limits<double>::infinity());
  EXPECT_EQ((large_delta + kOneSecond).InMicrosecondsF(),
            std::numeric_limits<double>::infinity());

  // Test +=, -=, *= and /= operators.
  TimeDelta delta = large_delta;
  delta += kOneSecond;
  EXPECT_TRUE(delta.is_max());
  delta = large_negative;
  delta += -kOneSecond;
  EXPECT_TRUE((delta).is_min());

  delta = large_negative;
  delta -= kOneSecond;
  EXPECT_TRUE((delta).is_min());
  delta = large_delta;
  delta -= -kOneSecond;
  EXPECT_TRUE(delta.is_max());

  delta = large_delta;
  delta *= 2;
  EXPECT_TRUE(delta.is_max());
  delta = large_negative;
  delta *= 1.5;
  EXPECT_TRUE((delta).is_min());

  delta = large_delta;
  delta /= 0.5;
  EXPECT_TRUE(delta.is_max());
  delta = large_negative;
  delta /= 0.5;
  EXPECT_TRUE((delta).is_min());

  // Test operations with Time and TimeTicks.
  EXPECT_TRUE((large_delta + Time::Now()).is_max());
  EXPECT_TRUE((large_delta + TimeTicks::Now()).is_max());
  EXPECT_TRUE((Time::Now() + large_delta).is_max());
  EXPECT_TRUE((TimeTicks::Now() + large_delta).is_max());

  Time time_now = Time::Now();
  EXPECT_EQ(kOneSecond, (time_now + kOneSecond) - time_now);
  EXPECT_EQ(-kOneSecond, (time_now - kOneSecond) - time_now);

  TimeTicks ticks_now = TimeTicks::Now();
  EXPECT_EQ(-kOneSecond, (ticks_now - kOneSecond) - ticks_now);
  EXPECT_EQ(kOneSecond, (ticks_now + kOneSecond) - ticks_now);
}

TEST(TimeBase, AddSubDeltaSaturates) {
  constexpr TimeTicks kLargeTimeTicks =
      TimeTicks::FromInternalValue(std::numeric_limits<int64_t>::max() - 1);

  constexpr TimeTicks kLargeNegativeTimeTicks =
      TimeTicks::FromInternalValue(std::numeric_limits<int64_t>::min() + 1);

  EXPECT_TRUE((kLargeTimeTicks + TimeDelta::Max()).is_max())
      << (kLargeTimeTicks + TimeDelta::Max());
  EXPECT_TRUE((kLargeNegativeTimeTicks + TimeDelta::Max()).is_max())
      << (kLargeNegativeTimeTicks + TimeDelta::Max());
  EXPECT_TRUE((kLargeTimeTicks - TimeDelta::Max()).is_min())
      << (kLargeTimeTicks - TimeDelta::Max());
  EXPECT_TRUE((kLargeNegativeTimeTicks - TimeDelta::Max()).is_min())
      << (kLargeNegativeTimeTicks - TimeDelta::Max());
  EXPECT_TRUE((TimeTicks() + TimeDelta::Max()).is_max())
      << (TimeTicks() + TimeDelta::Max());
  EXPECT_TRUE((TimeTicks() - TimeDelta::Max()).is_min())
      << (TimeTicks() - TimeDelta::Max());
  EXPECT_TRUE((TimeTicks::Now() + TimeDelta::Max()).is_max())
      << (TimeTicks::Now() + TimeDelta::Max());
  EXPECT_TRUE((TimeTicks::Now() - TimeDelta::Max()).is_min())
      << (TimeTicks::Now() - TimeDelta::Max());

  EXPECT_TRUE((kLargeTimeTicks + TimeDelta::Min()).is_min())
      << (kLargeTimeTicks + TimeDelta::Min());
  EXPECT_TRUE((kLargeNegativeTimeTicks + TimeDelta::Min()).is_min())
      << (kLargeNegativeTimeTicks + TimeDelta::Min());
  EXPECT_TRUE((kLargeTimeTicks - TimeDelta::Min()).is_max())
      << (kLargeTimeTicks - TimeDelta::Min());
  EXPECT_TRUE((kLargeNegativeTimeTicks - TimeDelta::Min()).is_max())
      << (kLargeNegativeTimeTicks - TimeDelta::Min());
  EXPECT_TRUE((TimeTicks() + TimeDelta::Min()).is_min())
      << (TimeTicks() + TimeDelta::Min());
  EXPECT_TRUE((TimeTicks() - TimeDelta::Min()).is_max())
      << (TimeTicks() - TimeDelta::Min());
  EXPECT_TRUE((TimeTicks::Now() + TimeDelta::Min()).is_min())
      << (TimeTicks::Now() + TimeDelta::Min());
  EXPECT_TRUE((TimeTicks::Now() - TimeDelta::Min()).is_max())
      << (TimeTicks::Now() - TimeDelta::Min());
}

TEST(TimeBase, AddSubInfinities) {
  // CHECK when adding opposite signs or subtracting same sign.
  EXPECT_CHECK_DEATH({ TimeTicks::Min() + TimeDelta::Max(); });
  EXPECT_CHECK_DEATH({ TimeTicks::Max() + TimeDelta::Min(); });
  EXPECT_CHECK_DEATH({ TimeTicks::Min() - TimeDelta::Min(); });
  EXPECT_CHECK_DEATH({ TimeTicks::Max() - TimeDelta::Max(); });

  // Saturates when adding same sign or subtracting opposite signs.
  EXPECT_TRUE((TimeTicks::Max() + TimeDelta::Max()).is_max());
  EXPECT_TRUE((TimeTicks::Min() + TimeDelta::Min()).is_min());
  EXPECT_TRUE((TimeTicks::Max() - TimeDelta::Min()).is_max());
  EXPECT_TRUE((TimeTicks::Min() - TimeDelta::Max()).is_min());
}

constexpr TimeTicks TestTimeTicksConstexprCopyAssignment() {
  TimeTicks a = TimeTicks::FromInternalValue(12345);
  TimeTicks b;
  b = a;
  return b;
}

TEST(TimeTicks, ConstexprAndTriviallyCopiable) {
  // "Trivially copyable" is necessary for use in std::atomic<TimeTicks>.
  static_assert(std::is_trivially_copyable<TimeTicks>(), "");

  // Copy ctor.
  constexpr TimeTicks a = TimeTicks::FromInternalValue(12345);
  constexpr TimeTicks b{a};
  static_assert(a.ToInternalValue() == b.ToInternalValue(), "");

  // Copy assignment.
  static_assert(a.ToInternalValue() ==
                    TestTimeTicksConstexprCopyAssignment().ToInternalValue(),
                "");
}

constexpr ThreadTicks TestThreadTicksConstexprCopyAssignment() {
  ThreadTicks a = ThreadTicks::FromInternalValue(12345);
  ThreadTicks b;
  b = a;
  return b;
}

TEST(ThreadTicks, ConstexprAndTriviallyCopiable) {
  // "Trivially copyable" is necessary for use in std::atomic<ThreadTicks>.
  static_assert(std::is_trivially_copyable<ThreadTicks>(), "");

  // Copy ctor.
  constexpr ThreadTicks a = ThreadTicks::FromInternalValue(12345);
  constexpr ThreadTicks b{a};
  static_assert(a.ToInternalValue() == b.ToInternalValue(), "");

  // Copy assignment.
  static_assert(a.ToInternalValue() ==
                    TestThreadTicksConstexprCopyAssignment().ToInternalValue(),
                "");
}

constexpr TimeDelta TestTimeDeltaConstexprCopyAssignment() {
  TimeDelta a = TimeDelta::FromSeconds(1);
  TimeDelta b;
  b = a;
  return b;
}

TEST(TimeDelta, ConstexprAndTriviallyCopiable) {
  // "Trivially copyable" is necessary for use in std::atomic<TimeDelta>.
  static_assert(std::is_trivially_copyable<TimeDelta>(), "");

  // Copy ctor.
  constexpr TimeDelta a = TimeDelta::FromSeconds(1);
  constexpr TimeDelta b{a};
  static_assert(a == b, "");

  // Copy assignment.
  static_assert(a == TestTimeDeltaConstexprCopyAssignment(), "");
}

TEST(TimeDeltaLogging, DCheckEqCompiles) {
  DCHECK_EQ(TimeDelta(), TimeDelta());
}

TEST(TimeDeltaLogging, EmptyIsZero) {
  constexpr TimeDelta kZero;
  EXPECT_EQ("0 s", AnyToString(kZero));
}

TEST(TimeDeltaLogging, FiveHundredMs) {
  constexpr TimeDelta kFiveHundredMs = TimeDelta::FromMilliseconds(500);
  EXPECT_EQ("0.5 s", AnyToString(kFiveHundredMs));
}

TEST(TimeDeltaLogging, MinusTenSeconds) {
  constexpr TimeDelta kMinusTenSeconds = TimeDelta::FromSeconds(-10);
  EXPECT_EQ("-10 s", AnyToString(kMinusTenSeconds));
}

TEST(TimeDeltaLogging, DoesNotMessUpFormattingFlags) {
  std::ostringstream oss;
  std::ios_base::fmtflags flags_before = oss.flags();
  oss << TimeDelta();
  EXPECT_EQ(flags_before, oss.flags());
}

TEST(TimeDeltaLogging, DoesNotMakeStreamBad) {
  std::ostringstream oss;
  oss << TimeDelta();
  EXPECT_TRUE(oss.good());
}

TEST(TimeLogging, DCheckEqCompiles) {
  DCHECK_EQ(Time(), Time());
}

TEST(TimeLogging, ChromeBirthdate) {
  Time birthdate;
  ASSERT_TRUE(Time::FromString("Tue, 02 Sep 2008 09:42:18 GMT", &birthdate));
  EXPECT_EQ("2008-09-02 09:42:18.000 UTC", AnyToString(birthdate));
}

TEST(TimeLogging, DoesNotMessUpFormattingFlags) {
  std::ostringstream oss;
  std::ios_base::fmtflags flags_before = oss.flags();
  oss << Time();
  EXPECT_EQ(flags_before, oss.flags());
}

TEST(TimeLogging, DoesNotMakeStreamBad) {
  std::ostringstream oss;
  oss << Time();
  EXPECT_TRUE(oss.good());
}

TEST(TimeTicksLogging, DCheckEqCompiles) {
  DCHECK_EQ(TimeTicks(), TimeTicks());
}

TEST(TimeTicksLogging, ZeroTime) {
  TimeTicks zero;
  EXPECT_EQ("0 bogo-microseconds", AnyToString(zero));
}

TEST(TimeTicksLogging, FortyYearsLater) {
  TimeTicks forty_years_later =
      TimeTicks() + TimeDelta::FromDays(365.25 * 40);
  EXPECT_EQ("1262304000000000 bogo-microseconds",
            AnyToString(forty_years_later));
}

TEST(TimeTicksLogging, DoesNotMessUpFormattingFlags) {
  std::ostringstream oss;
  std::ios_base::fmtflags flags_before = oss.flags();
  oss << TimeTicks();
  EXPECT_EQ(flags_before, oss.flags());
}

TEST(TimeTicksLogging, DoesNotMakeStreamBad) {
  std::ostringstream oss;
  oss << TimeTicks();
  EXPECT_TRUE(oss.good());
}

}  // namespace

}  // namespace base
