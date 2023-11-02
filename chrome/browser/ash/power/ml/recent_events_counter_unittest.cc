// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/ml/recent_events_counter.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace power {
namespace ml {

TEST(RecentEventsCounterTest, TimeTest) {
  base::TimeDelta minute = base::Minutes(1);
  RecentEventsCounter counter(base::Hours(1), 60);
  ASSERT_EQ(counter.GetTotal(minute), 0);

  counter.Log(5 * minute);
  ASSERT_EQ(counter.GetTotal(10 * minute), 1);

  counter.Log(5 * minute);
  ASSERT_EQ(counter.GetTotal(10 * minute), 2);

  counter.Log(25.4 * minute);

  ASSERT_EQ(counter.GetTotal(30 * minute), 3);
  ASSERT_EQ(counter.GetTotal(70 * minute), 1);
  // Event at 25.4 minutes is counted 59 minutes later.
  ASSERT_EQ(counter.GetTotal(84.4 * minute), 1);
  // Event at 25.4 minutes is not counted 59.7 minutes later at 85.1 minutes. An
  // an event logged at 85.1 minutes would wipe out the event at 25.4 minutes,
  // so the event at 25.4 minutes cannot be counted to ensure consistency.
  ASSERT_EQ(counter.GetTotal(85.1 * minute), 0);

  counter.Log(75 * minute);
  ASSERT_EQ(counter.GetTotal(80 * minute), 2);
  ASSERT_EQ(counter.GetTotal(90 * minute), 1);

  // Overwrite the 25.4 minute logging.
  counter.Log(85.1 * minute);
  ASSERT_EQ(counter.GetTotal(90 * minute), 2);

  counter.Log(200 * minute);
  ASSERT_EQ(counter.GetTotal(210 * minute), 1);

  ASSERT_EQ(counter.GetTotal(300 * minute), 0);
}

TEST(RecentEventsCounterTest, TimeTestConsecutiveMinutes) {
  base::TimeDelta minute = base::Minutes(1);
  RecentEventsCounter counter(base::Hours(1), 60);

  for (int i = 0; i < 59; i++) {
    counter.Log(i * minute);
    EXPECT_EQ(counter.GetTotal(i * minute), i + 1);
    EXPECT_EQ(counter.GetTotal((i + 0.5) * minute), i + 1);
    EXPECT_EQ(counter.GetTotal((i + 1) * minute), i + 1);
  }
  for (int i = 59; i < 122; i++) {
    counter.Log(i * minute);
    EXPECT_EQ(counter.GetTotal(i * minute), 60);
    EXPECT_EQ(counter.GetTotal((i + 0.5) * minute), 60);
    EXPECT_EQ(counter.GetTotal((i + 1) * minute), 59);
  }
}

// Tests that, when logging a slightly-newer event, stale buckets are cleared.
TEST(RecentEventsCounterTest, SomeBucketsStale) {
  base::TimeDelta minute = base::Minutes(1);
  RecentEventsCounter counter(base::Hours(1), 60);

  // Start with 60 buckets covering [0, 60), with 1 event per bucket.
  for (int i = 0; i < 60; i++) {
    counter.Log(i * minute);
  }
  CHECK_EQ(counter.GetTotal(59.5 * minute), 60);

  // Logging an event at 64 should advance this to [5, 65), with:
  // * 55 buckets covering [5, 60) with 1 event each
  // * 4 buckets covering [60, 64) with 0 events each
  // * 1 bucket covering [64, 65) with 1 event each
  // Total: 56
  counter.Log(64 * minute);
  EXPECT_EQ(counter.GetTotal(64.5 * minute), 56);
}

// Tests that, when logging an event more than `duration` newer than any
// previous event, all buckets are cleared (since all will be stale).
TEST(RecentEventsCounterTest, AllBucketsStale) {
  base::TimeDelta minute = base::Minutes(1);
  RecentEventsCounter counter(base::Hours(1), 60);

  // Start with 60 buckets covering [0, 60), with 1 event per bucket.
  for (int i = 0; i < 60; i++) {
    counter.Log(i * minute);
  }
  CHECK_EQ(counter.GetTotal(59.5 * minute), 60);

  // Logging an event at 124 should advance this to [65, 125), with:
  // * 59 buckets covering [65, 124) with 0 events each
  // * 1 bucket covering [124, 125) with 1 event each
  // Total: 1
  counter.Log(124 * minute);
  EXPECT_EQ(counter.GetTotal(124.5 * minute), 1);
}

}  // namespace ml
}  // namespace power
}  // namespace ash
