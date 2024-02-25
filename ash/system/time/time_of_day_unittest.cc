// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/time_of_day.h"

#include "ash/test/failing_local_time_converter.h"
#include "base/i18n/rtl.h"
#include "base/test/icu_test_util.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

TEST(TimeOfDayTest, TestEquality) {
  // Test created TimeOfDay objects with equal offsets are equal.
  TimeOfDay time1(18 * 60 + 32);  // 6:32 PM.
  TimeOfDay time2(18 * 60 + 32);  // 6:32 PM.
  EXPECT_EQ(time1, time2);
  TimeOfDay time3(time1);
  EXPECT_EQ(time1, time3);
  EXPECT_EQ(time2, time3);
  TimeOfDay time4(9 * 60 + 59);  // 9:59 AM.
  EXPECT_FALSE(time1 == time4);
  time1 = time4;
  EXPECT_EQ(time1, time4);
}

TEST(TimeOfDayTest, TestSeveralOffsets) {
  // Ensure US locale to make sure time format is expected.
  base::test::ScopedRestoreICUDefaultLocale restore_locale;
  base::i18n::SetICUDefaultLocale("en_US");

  // 6:32 PM ==> 18:32.
  TimeOfDay time1(18 * 60 + 32);
  EXPECT_EQ("6:32\xE2\x80\xAFPM", time1.ToString());

  // 9:59 AM.
  TimeOfDay time2(9 * 60 + 59);
  EXPECT_EQ(
      "9:59\xE2\x80\xAF"
      "AM",
      time2.ToString());

  // Border times: 00:00 and 24:00.
  TimeOfDay time3(0);
  TimeOfDay time4(24 * 60);
  EXPECT_EQ(
      "12:00\xE2\x80\xAF"
      "AM",
      time3.ToString());
  EXPECT_EQ(
      "12:00\xE2\x80\xAF"
      "AM",
      time4.ToString());
}

TEST(TimeOfDayTest, TestFromTime) {
  // "Now" today and "now" tomorrow should have the same minutes offset from
  // 00:00.
  // Assume that "now" is Tuesday May 23, 2017 at 10:30 AM.
  static constexpr base::Time::Exploded kNow = {.year = 2017,
                                                .month = 5,
                                                .day_of_week = 2,
                                                .day_of_month = 23,
                                                .hour = 10,
                                                .minute = 30};
  base::Time now_today = base::Time::Now();
  ASSERT_TRUE(base::Time::FromLocalExploded(kNow, &now_today));
  base::Time now_tomorrow = now_today + base::Days(1);
  EXPECT_EQ(TimeOfDay::FromTime(now_today), TimeOfDay::FromTime(now_tomorrow));
}

// Tests that if the clock is set the date today should follow the clock date.
TEST(TimeOfDayTest, SetClock) {
  base::SimpleTestClock test_clock;
  base::Time time_now;
  EXPECT_TRUE(base::Time::FromString("23 Dec 2021 12:00:00", &time_now));
  test_clock.SetNow(time_now);

  TimeOfDay test_time = TimeOfDay(2 * 60).SetClock(&test_clock);
  base::Time expected_time;
  EXPECT_TRUE(base::Time::FromString("23 Dec 2021 2:00:00", &expected_time));
  EXPECT_EQ(expected_time, test_time.ToTimeToday());
}

TEST(TimeOfDayTest, ReturnsNullTimeWhenLocalTimeFails) {
  FailingLocalTimeConverter failing_local_time_converter;
  TimeOfDay time_of_day =
      TimeOfDay(12 * 60).SetLocalTimeConverter(&failing_local_time_converter);
  EXPECT_FALSE(time_of_day.ToTimeToday());
}

}  // namespace

}  // namespace ash
