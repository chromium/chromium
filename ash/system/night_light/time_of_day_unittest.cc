// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/night_light/time_of_day.h"

#include "base/i18n/rtl.h"
#include "base/test/icu_test_util.h"
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
  EXPECT_EQ("6:32 PM", time1.ToString());

  // 9:59 AM.
  TimeOfDay time2(9 * 60 + 59);
  EXPECT_EQ("9:59 AM", time2.ToString());

  // Border times: 00:00 and 24:00.
  TimeOfDay time3(0);
  TimeOfDay time4(24 * 60);
  EXPECT_EQ("12:00 AM", time3.ToString());
  EXPECT_EQ("12:00 AM", time4.ToString());
}

TEST(TimeOfDayTest, TestFromTime) {
  // "Now" today and "now" tomorrow should have the same minutes offset from
  // 00:00.
  // Assume that "now" is Tuesday May 23, 2017 at 10:30 AM.
  base::Time::Exploded now;
  now.year = 2017;
  now.month = 5;        // May.
  now.day_of_week = 2;  // Tuesday.
  now.day_of_month = 23;
  now.hour = 10;
  now.minute = 30;
  now.second = 0;
  now.millisecond = 0;

  base::Time now_today = base::Time::Now();
  ASSERT_TRUE(base::Time::FromLocalExploded(now, &now_today));
  base::Time now_tomorrow = now_today + base::TimeDelta::FromDays(1);
  EXPECT_EQ(TimeOfDay::FromTime(now_today), TimeOfDay::FromTime(now_tomorrow));
}

}  // namespace

}  // namespace ash
