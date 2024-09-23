// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/heuristics/opener_heuristic_metrics.h"

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::TimeDelta;

TEST(OpenerHeuristicsMetricsTest, BucketizeHoursSinceLastInteraction) {
  base::TimeDelta maximum = base::Days(30);
  auto cast_time_delta =
      base::BindRepeating(&base::TimeDelta::InHours)
          .Then(base::BindRepeating([](int64_t t) { return t; }));

  // The input value is clamped to be between 0 and 30 days.
  EXPECT_EQ(Bucketize3PCDHeuristicTimeDelta(base::TimeDelta::Min(), maximum,
                                            cast_time_delta),
            0);
  EXPECT_EQ(Bucketize3PCDHeuristicTimeDelta(base::Seconds(0), maximum,
                                            cast_time_delta),
            0);
  EXPECT_EQ(
      Bucketize3PCDHeuristicTimeDelta(base::Days(30), maximum, cast_time_delta),
      base::Days(30).InHours());
  EXPECT_EQ(Bucketize3PCDHeuristicTimeDelta(base::TimeDelta::Max(), maximum,
                                            cast_time_delta),
            base::Days(30).InHours());

  std::set<int32_t> seen_values;
  int32_t last_value = 0;
  for (TimeDelta td = base::Seconds(0); td <= base::Days(30);
       td += base::Hours(1)) {
    int32_t value =
        Bucketize3PCDHeuristicTimeDelta(td, maximum, cast_time_delta);
    // Values get placed in increasing buckets
    ASSERT_LE(last_value, value);
    seen_values.insert(value);
    last_value = value;
  }
  // Exactly 50 buckets
  ASSERT_EQ(seen_values.size(), 50u);
}

// TODO(crbug.com/40281179): The test is flaky across platforms.
TEST(OpenerHeuristicsMetricsTest, DISABLED_BucketizeSecondsSinceCommitted) {
  base::TimeDelta maximum = base::Minutes(3);
  auto cast_time_delta = base::BindRepeating(&base::TimeDelta::InSeconds);

  // The input value is clamped to be between 0 and 3 minutes.
  EXPECT_EQ(Bucketize3PCDHeuristicTimeDelta(base::TimeDelta::Min(), maximum,
                                            cast_time_delta),
            0);
  EXPECT_EQ(Bucketize3PCDHeuristicTimeDelta(base::Seconds(0), maximum,
                                            cast_time_delta),
            0);
  EXPECT_EQ(Bucketize3PCDHeuristicTimeDelta(base::Minutes(3), maximum,
                                            cast_time_delta),
            base::Minutes(3).InSeconds());
  EXPECT_EQ(Bucketize3PCDHeuristicTimeDelta(base::TimeDelta::Max(), maximum,
                                            cast_time_delta),
            base::Minutes(3).InSeconds());

  std::set<int32_t> seen_values;
  int32_t last_value = 0;
  for (TimeDelta td; td <= base::Minutes(3); td += base::Seconds(1)) {
    int32_t value =
        Bucketize3PCDHeuristicTimeDelta(td, maximum, cast_time_delta);
    // Values get placed in increasing buckets
    ASSERT_LE(last_value, value);
    seen_values.insert(value);
    last_value = value;
  }
  // Exactly 50 buckets
  ASSERT_EQ(seen_values.size(), 50u);
}
