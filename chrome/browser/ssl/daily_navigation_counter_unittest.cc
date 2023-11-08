// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ssl/daily_navigation_counter.h"

#include "base/test/simple_test_clock.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

constexpr char kStartTime[] = "2023-10-15T06:00:00Z";

// Navigation counter should be able to load the counts from a dict.
TEST(DailyNavigationCounterTest, ShouldLoadFromDict) {
  base::SimpleTestClock clock;
  base::Time now;
  EXPECT_TRUE(base::Time::FromUTCString(kStartTime, &now));
  clock.SetNow(now);

  base::Value::Dict dict;
  DailyNavigationCounter counter1(&dict, &clock,
                                  /*rolling_window_duration_in_days=*/1u,
                                  /*save_interval=*/1);

  EXPECT_EQ(0u, counter1.GetTotal());
  EXPECT_EQ(0u, counter1.unsaved_count_for_testing());

  EXPECT_TRUE(counter1.Increment());
  EXPECT_EQ(1u, counter1.GetTotal());
  EXPECT_EQ(0u, counter1.unsaved_count_for_testing());

  EXPECT_TRUE(counter1.Increment());
  EXPECT_EQ(2u, counter1.GetTotal());
  EXPECT_EQ(0u, counter1.unsaved_count_for_testing());

  // Advance the clock and record again. This will keep all entries.
  clock.SetNow(now + base::Hours(13));
  EXPECT_TRUE(counter1.Increment());
  EXPECT_EQ(3u, counter1.GetTotal());
  EXPECT_EQ(0u, counter1.unsaved_count_for_testing());

  DailyNavigationCounter counter2(&dict, &clock,
                                  /*rolling_window_duration_in_days=*/1u,
                                  /*save_interval=*/1);
  EXPECT_EQ(3u, counter1.GetTotal());
  EXPECT_EQ(0u, counter1.unsaved_count_for_testing());

  EXPECT_TRUE(counter2.Increment());
  EXPECT_EQ(4u, counter2.GetTotal());
  EXPECT_EQ(0u, counter2.unsaved_count_for_testing());

  // Advance the clock and record again. This will keep all entries.
  clock.SetNow(now + base::Hours(14));
  EXPECT_TRUE(counter2.Increment());
  EXPECT_EQ(5u, counter2.GetTotal());
  EXPECT_EQ(0u, counter2.unsaved_count_for_testing());

  // Advance the clock a day and record again. This will keep all entries
  // because entries added yesterday are within the rolling window.
  clock.SetNow(now + base::Hours(25));
  EXPECT_TRUE(counter2.Increment());
  EXPECT_EQ(6u, counter2.GetTotal());
  EXPECT_EQ(0u, counter2.unsaved_count_for_testing());

  // Advance the clock further and record again. This will drop 5 old entries
  // from the first day.
  clock.SetNow(now + base::Hours(49));
  EXPECT_TRUE(counter2.Increment());
  EXPECT_EQ(2u, counter2.GetTotal());
  EXPECT_EQ(0u, counter2.unsaved_count_for_testing());
}

// Navigation counter should properly handle counts using a small rolling
// window and small saving interval.
TEST(DailyNavigationCounterTest, SmallRollingWindowSmallInterval) {
  base::SimpleTestClock clock;
  base::Time now;
  EXPECT_TRUE(base::Time::FromUTCString(kStartTime, &now));
  clock.SetNow(now);

  base::Value::Dict dict;
  DailyNavigationCounter counter(&dict, &clock,
                                 /*rolling_window_duration_in_days=*/1u,
                                 /*save_interval=*/1);

  EXPECT_EQ(0u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(1u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(2u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  // Advance the clock and record again. This will keep all entries.
  clock.SetNow(now + base::Hours(13));
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(3u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  // Advance the clock and record again. This will drop old entries and only
  // keep the last entry.
  clock.SetNow(now + base::Hours(26));
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(4u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  // Advance the clock and record again. This will drop 3 old entries from the
  // first day.
  clock.SetNow(now + base::Hours(49));
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(2u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());
}

// Navigation counter should properly handle counts using a large rolling
// window and small saving interval.
TEST(DailyNavigationCounterTest, LargeRollingWindowSmallInterval) {
  base::SimpleTestClock clock;
  base::Time now;
  EXPECT_TRUE(base::Time::FromUTCString(kStartTime, &now));
  clock.SetNow(now);

  base::Value::Dict dict;
  DailyNavigationCounter counter(&dict, &clock,
                                 /*rolling_window_duration_in_days=*/7u,
                                 /*save_interval=*/1);

  EXPECT_EQ(0u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  EXPECT_TRUE(counter.Increment());
  clock.SetNow(now + base::Hours(6));
  EXPECT_TRUE(counter.Increment());
  clock.SetNow(now + base::Hours(12));
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(3u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  clock.SetNow(now + base::Days(1));
  EXPECT_TRUE(counter.Increment());
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(5u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  // Advance the clock and record again. This will keep all entries.
  clock.SetNow(now + base::Days(2));
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(6u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  // Advance the clock and record again. This will keep all entries.
  clock.SetNow(now + base::Days(3));
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(7u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  // Advance the clock and record again. This will keep all entries.
  clock.SetNow(now + base::Days(4));
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(8u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  // Advance the clock and record again. This will keep all entries.
  clock.SetNow(now + base::Days(5));
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(9u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  // Advance the clock and record again. This will keep all entries.
  clock.SetNow(now + base::Days(6));
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(10u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  // Advance the clock and record again. This will keep all entries because they
  // are still inside the rolling window.
  clock.SetNow(now + base::Days(7));
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(11u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  // Advance the clock and record again. This will drop the 3 entries from the
  // second day.
  clock.SetNow(now + base::Days(8));
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(9u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  // Advance the clock very far and record again. This will drop all entries but
  // the last one.
  clock.SetNow(now + base::Days(30));
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(1u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());
}

// Navigation counter should properly handle counts using a small rolling
// window and large saving interval.
TEST(DailyNavigationCounterTest, SmallRollingWindowLargeInterval) {
  base::SimpleTestClock clock;
  base::Time now;
  EXPECT_TRUE(base::Time::FromUTCString(kStartTime, &now));
  clock.SetNow(now);

  base::Value::Dict dict;
  DailyNavigationCounter counter(&dict, &clock,
                                 /*rolling_window_duration_in_days=*/1u,
                                 /*save_interval=*/10);

  EXPECT_EQ(0u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());
  // Recording 9 times won't save the count in the pref.
  for (size_t i = 0; i < 9; i++) {
    EXPECT_FALSE(counter.Increment());
    EXPECT_EQ(i + 1, counter.GetTotal());
    EXPECT_EQ(i + 1, counter.unsaved_count_for_testing());
  }
  // ... but the 10th time will.
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(10u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  // Record another 9 times, this also won't save the count in the pref.
  for (size_t i = 0; i < 9; i++) {
    EXPECT_FALSE(counter.Increment());
    EXPECT_EQ(11u + i, counter.GetTotal());
    EXPECT_EQ(i + 1, counter.unsaved_count_for_testing());
  }
  // ... but the 20th time will.
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(20u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  // Advance the clock and record again. This will keep all entries.
  clock.SetNow(now + base::Hours(13));
  EXPECT_FALSE(counter.Increment());
  EXPECT_FALSE(counter.Increment());
  EXPECT_EQ(22u, counter.GetTotal());
  EXPECT_EQ(2u, counter.unsaved_count_for_testing());

  // Advance the clock to next day and record 8 times. This will keep all
  // entries as they are still inside the rolling window.
  clock.SetNow(now + base::Hours(26));
  for (size_t i = 0; i < 7; i++) {
    EXPECT_FALSE(counter.Increment());
  }
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(30u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  // Advance the clock to another day and record 10 times. The first nine
  // increments won't discard anything, but the last increment will discard 22
  // old entries from the first day and only keep the entries from today and
  // yesterday.
  clock.SetNow(now + base::Hours(49));
  for (size_t i = 0; i < 9; i++) {
    EXPECT_FALSE(counter.Increment());
  }
  EXPECT_EQ(39u, counter.GetTotal());
  EXPECT_EQ(9u, counter.unsaved_count_for_testing());
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(18u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());
}

// Navigation counter should properly handle counts using a large rolling
// window and large saving interval.
TEST(DailyNavigationCounterTest, LargeRollingWindowLargeInterval) {
  base::SimpleTestClock clock;
  base::Time now;
  EXPECT_TRUE(base::Time::FromUTCString(kStartTime, &now));
  clock.SetNow(now);

  base::Value::Dict dict;
  DailyNavigationCounter counter(&dict, &clock,
                                 /*rolling_window_duration_in_days=*/7u,
                                 /*save_interval=*/10);

  EXPECT_EQ(0u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());
  // Recording 9 times won't save the count in the pref.
  for (size_t i = 0; i < 9; i++) {
    EXPECT_FALSE(counter.Increment());
    EXPECT_EQ(i + 1, counter.GetTotal());
    EXPECT_EQ(i + 1, counter.unsaved_count_for_testing());
  }
  // ... but the 10th time will.
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(10u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  // Record another 9 times, this also won't save the count in the pref.
  for (size_t i = 0; i < 9; i++) {
    EXPECT_FALSE(counter.Increment());
    EXPECT_EQ(11u + i, counter.GetTotal());
    EXPECT_EQ(i + 1, counter.unsaved_count_for_testing());
  }
  // ... but the 20th time will.
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(20u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  // Advance the clock and record again. This will keep all entries.
  clock.SetNow(now + base::Hours(13));
  EXPECT_FALSE(counter.Increment());
  EXPECT_EQ(21u, counter.GetTotal());
  EXPECT_EQ(1u, counter.unsaved_count_for_testing());

  // Advance the clock and record twice. This will keep all entries.
  clock.SetNow(now + base::Hours(25));
  EXPECT_FALSE(counter.Increment());
  EXPECT_FALSE(counter.Increment());
  EXPECT_EQ(23u, counter.GetTotal());
  EXPECT_EQ(3u, counter.unsaved_count_for_testing());

  // Advance the clock and record again. This will keep all entries.
  clock.SetNow(now + base::Days(2));
  EXPECT_FALSE(counter.Increment());
  EXPECT_EQ(24u, counter.GetTotal());
  EXPECT_EQ(4u, counter.unsaved_count_for_testing());

  // Advance the clock and record again. This will keep all entries.
  clock.SetNow(now + base::Days(3));
  EXPECT_FALSE(counter.Increment());
  EXPECT_EQ(25u, counter.GetTotal());
  EXPECT_EQ(5u, counter.unsaved_count_for_testing());

  // Advance the clock and record again. This will keep all entries.
  clock.SetNow(now + base::Days(4));
  EXPECT_FALSE(counter.Increment());
  EXPECT_EQ(26u, counter.GetTotal());
  EXPECT_EQ(6u, counter.unsaved_count_for_testing());

  // Advance the clock and record again. This will keep all entries.
  clock.SetNow(now + base::Days(5));
  EXPECT_FALSE(counter.Increment());
  EXPECT_EQ(27u, counter.GetTotal());
  EXPECT_EQ(7u, counter.unsaved_count_for_testing());

  // Advance the clock and record again. This will keep all entries.
  clock.SetNow(now + base::Days(6));
  EXPECT_FALSE(counter.Increment());
  EXPECT_EQ(28u, counter.GetTotal());
  EXPECT_EQ(8u, counter.unsaved_count_for_testing());

  // Advance the clock and record twice. This will keep all entries as they are
  // still inside the rolling window.
  clock.SetNow(now + base::Days(7));
  EXPECT_FALSE(counter.Increment());
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(30u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  // Advance the clock and record ten times. This will drop the 21 entries from
  // the first day.
  clock.SetNow(now + base::Days(8));
  for (size_t i = 0; i < 9; i++) {
    EXPECT_FALSE(counter.Increment());
  }
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(19u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  // Advance the clock and record ten times. This will drop the 2 entries from
  // the second day.
  clock.SetNow(now + base::Days(9));
  for (size_t i = 0; i < 9; i++) {
    EXPECT_FALSE(counter.Increment());
  }
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(27u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());

  // Advance the clock very far and record ten times. This will all entries but
  // the last ten.
  clock.SetNow(now + base::Days(30));
  for (size_t i = 0; i < 9; i++) {
    EXPECT_FALSE(counter.Increment());
  }
  EXPECT_EQ(36u, counter.GetTotal());
  EXPECT_EQ(9u, counter.unsaved_count_for_testing());
  EXPECT_TRUE(counter.Increment());
  EXPECT_EQ(10u, counter.GetTotal());
  EXPECT_EQ(0u, counter.unsaved_count_for_testing());
}
