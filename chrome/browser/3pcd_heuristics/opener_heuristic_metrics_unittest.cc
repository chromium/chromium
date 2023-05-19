// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/3pcd_heuristics/opener_heuristic_metrics.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

TEST(OpenerHeuristicsMetricsTest, BucketizeHoursSinceLastInteraction) {
  // The input value is clamped to be between 0 and 30 days.
  EXPECT_EQ(BucketizeHoursSinceLastInteraction(base::TimeDelta::Min()), 0);
  EXPECT_EQ(BucketizeHoursSinceLastInteraction(base::Seconds(0)), 0);
  EXPECT_EQ(BucketizeHoursSinceLastInteraction(base::Days(30)),
            base::Days(30).InHours());
  EXPECT_EQ(BucketizeHoursSinceLastInteraction(base::TimeDelta::Max()),
            base::Days(30).InHours());

  std::set<int32_t> seen_values;
  int32_t last_value = 0;
  for (TimeDelta td = base::Seconds(0); td <= base::Days(30);
       td += base::Hours(1)) {
    int32_t value = BucketizeHoursSinceLastInteraction(td);
    // Values get placed in increasing buckets
    ASSERT_LE(last_value, value);
    seen_values.insert(value);
    last_value = value;
  }
  // Exactly 50 buckets
  ASSERT_EQ(seen_values.size(), 50u);
}

TEST(OpenerHeuristicsMetricsTest, BucketizeSecondsSinceCommitted) {
  // The input value is clamped to be between 0 and 3 minutes.
  EXPECT_EQ(BucketizeSecondsSinceCommitted(base::TimeDelta::Min()), 0);
  EXPECT_EQ(BucketizeSecondsSinceCommitted(base::Seconds(0)), 0);
  EXPECT_EQ(BucketizeSecondsSinceCommitted(base::Minutes(3)),
            base::Minutes(3).InSeconds());
  EXPECT_EQ(BucketizeSecondsSinceCommitted(base::TimeDelta::Max()),
            base::Minutes(3).InSeconds());

  std::set<int32_t> seen_values;
  int32_t last_value = 0;
  for (TimeDelta td; td <= base::Minutes(3); td += base::Seconds(1)) {
    int32_t value = BucketizeSecondsSinceCommitted(td);
    // Values get placed in increasing buckets
    ASSERT_LE(last_value, value);
    seen_values.insert(value);
    last_value = value;
  }
  // Exactly 50 buckets
  ASSERT_EQ(seen_values.size(), 50u);
}
