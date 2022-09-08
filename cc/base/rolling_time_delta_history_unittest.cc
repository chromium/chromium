// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/base/rolling_time_delta_history.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

TEST(RollingTimeDeltaHistoryTest, EmptyHistory) {
  RollingTimeDeltaHistory empty_history(0);

  EXPECT_EQ(base::TimeDelta(), empty_history.Percentile(0.0));
  EXPECT_EQ(base::TimeDelta(), empty_history.Percentile(50.0));
  EXPECT_EQ(base::TimeDelta(), empty_history.Percentile(100.0));

  empty_history.InsertSample(base::Milliseconds(10));
  empty_history.InsertSample(base::Milliseconds(15));
  empty_history.InsertSample(base::Milliseconds(20));

  EXPECT_EQ(base::TimeDelta(), empty_history.Percentile(0.0));
  EXPECT_EQ(base::TimeDelta(), empty_history.Percentile(50.0));
  EXPECT_EQ(base::TimeDelta(), empty_history.Percentile(100.0));

  empty_history.Clear();
  EXPECT_EQ(base::TimeDelta(), empty_history.Percentile(0.0));
  EXPECT_EQ(base::TimeDelta(), empty_history.Percentile(50.0));
  EXPECT_EQ(base::TimeDelta(), empty_history.Percentile(100.0));
}

TEST(RollingTimeDeltaHistoryTest, SizeOneHistory) {
  RollingTimeDeltaHistory size_one_history(1);
  base::TimeDelta sample1 = base::Milliseconds(10);
  base::TimeDelta sample2 = base::Milliseconds(20);

  EXPECT_EQ(base::TimeDelta(), size_one_history.Percentile(0.0));
  EXPECT_EQ(base::TimeDelta(), size_one_history.Percentile(50.0));
  EXPECT_EQ(base::TimeDelta(), size_one_history.Percentile(100.0));

  size_one_history.InsertSample(sample1);
  EXPECT_EQ(sample1, size_one_history.Percentile(0.0));
  EXPECT_EQ(sample1, size_one_history.Percentile(50.0));
  EXPECT_EQ(sample1, size_one_history.Percentile(100.0));

  size_one_history.InsertSample(sample2);
  EXPECT_EQ(sample2, size_one_history.Percentile(0.0));
  EXPECT_EQ(sample2, size_one_history.Percentile(50.0));
  EXPECT_EQ(sample2, size_one_history.Percentile(100.0));

  size_one_history.Clear();
  EXPECT_EQ(base::TimeDelta(), size_one_history.Percentile(0.0));
  EXPECT_EQ(base::TimeDelta(), size_one_history.Percentile(50.0));
  EXPECT_EQ(base::TimeDelta(), size_one_history.Percentile(100.0));
}

TEST(RollingTimeDeltaHistoryTest, LargeHistory) {
  RollingTimeDeltaHistory large_history(100);
  base::TimeDelta sample1 = base::Milliseconds(150);
  base::TimeDelta sample2 = base::Milliseconds(250);
  base::TimeDelta sample3 = base::Milliseconds(200);

  large_history.InsertSample(sample1);
  large_history.InsertSample(sample2);

  EXPECT_EQ(sample1, large_history.Percentile(0.0));
  EXPECT_EQ(sample1, large_history.Percentile(25.0));
  EXPECT_EQ(sample2, large_history.Percentile(75.0));
  EXPECT_EQ(sample2, large_history.Percentile(100.0));

  large_history.InsertSample(sample3);
  EXPECT_EQ(sample1, large_history.Percentile(0.0));
  EXPECT_EQ(sample1, large_history.Percentile(25.0));
  EXPECT_EQ(sample3, large_history.Percentile(50.0));
  EXPECT_EQ(sample2, large_history.Percentile(100.0));

  // Fill the history.
  for (int i = 1; i <= 97; i++)
    large_history.InsertSample(base::Milliseconds(i));

  EXPECT_EQ(base::Milliseconds(1), large_history.Percentile(0.0));
  for (int i = 1; i <= 97; i++) {
    EXPECT_EQ(base::Milliseconds(i), large_history.Percentile(i - 0.5));
  }
  EXPECT_EQ(sample1, large_history.Percentile(97.5));
  EXPECT_EQ(sample3, large_history.Percentile(98.5));
  EXPECT_EQ(sample2, large_history.Percentile(99.5));

  // Continue inserting samples, causing the oldest samples to be discarded.
  base::TimeDelta sample4 = base::Milliseconds(100);
  base::TimeDelta sample5 = base::Milliseconds(102);
  base::TimeDelta sample6 = base::Milliseconds(104);
  large_history.InsertSample(sample4);
  large_history.InsertSample(sample5);
  large_history.InsertSample(sample6);
  EXPECT_EQ(sample4, large_history.Percentile(97.5));
  EXPECT_EQ(sample5, large_history.Percentile(98.5));
  EXPECT_EQ(sample6, large_history.Percentile(99.5));

  large_history.Clear();
  EXPECT_EQ(base::TimeDelta(), large_history.Percentile(0.0));
  EXPECT_EQ(base::TimeDelta(), large_history.Percentile(50.0));
  EXPECT_EQ(base::TimeDelta(), large_history.Percentile(100.0));
}

}  // namespace
}  // namespace cc
