// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/power_monitor/moving_average.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace test {

// Ported from third_party/webrtc/rtc_base/numerics/moving_average_unittest.cc.

TEST(MovingAverageTest, EmptyAverage) {
  MovingAverage moving_average(1);
  EXPECT_EQ(0u, moving_average.Size());
  EXPECT_EQ(0, moving_average.GetAverageRoundedDown());
}

TEST(MovingAverageTest, OneElement) {
  MovingAverage moving_average(1);
  moving_average.AddSample(3);
  EXPECT_EQ(1u, moving_average.Size());
  EXPECT_EQ(3, moving_average.GetAverageRoundedDown());
}

// Verify that Size() increases monotonically when samples are added up to the
// window size. At that point the filter is full and shall return the window
// size as Size() until Reset() is called.
TEST(MovingAverageTest, Size) {
  constexpr uint8_t kWindowSize = 3;
  MovingAverage moving_average(kWindowSize);
  EXPECT_EQ(0u, moving_average.Size());
  moving_average.AddSample(1);
  EXPECT_EQ(1u, moving_average.Size());
  moving_average.AddSample(2);
  EXPECT_EQ(2u, moving_average.Size());
  moving_average.AddSample(3);
  // Three samples have beend added and the filter is full (all elements in the
  // buffer have been given a valid value).
  EXPECT_EQ(kWindowSize, moving_average.Size());
  // Adding a fourth sample will shift out the first sample (1) and the filter
  // should now contain [4,2,3] => average is 9 / 3 = 3.
  moving_average.AddSample(4);
  EXPECT_EQ(kWindowSize, moving_average.Size());
  EXPECT_EQ(3, moving_average.GetAverageRoundedToClosest());
  moving_average.Reset();
  EXPECT_EQ(0u, moving_average.Size());
  EXPECT_EQ(0, moving_average.GetAverageRoundedToClosest());
}

TEST(MovingAverageTest, GetAverage) {
  MovingAverage moving_average(255);
  moving_average.AddSample(1);
  moving_average.AddSample(1);
  moving_average.AddSample(3);
  moving_average.AddSample(3);
  EXPECT_EQ(moving_average.GetAverageRoundedDown(), 2);
  EXPECT_EQ(moving_average.GetAverageRoundedToClosest(), 2);
}

TEST(MovingAverageTest, GetAverageRoundedDownRounds) {
  MovingAverage moving_average(255);
  moving_average.AddSample(1);
  moving_average.AddSample(2);
  moving_average.AddSample(2);
  moving_average.AddSample(2);
  EXPECT_EQ(moving_average.GetAverageRoundedDown(), 1);
}

TEST(MovingAverageTest, GetAverageRoundedToClosestRounds) {
  MovingAverage moving_average(255);
  moving_average.AddSample(1);
  moving_average.AddSample(2);
  moving_average.AddSample(2);
  moving_average.AddSample(2);
  EXPECT_EQ(moving_average.GetAverageRoundedToClosest(), 2);
}

TEST(MovingAverageTest, Reset) {
  MovingAverage moving_average(5);
  moving_average.AddSample(1);
  EXPECT_EQ(1, moving_average.GetAverageRoundedDown());
  EXPECT_EQ(1, moving_average.GetAverageRoundedToClosest());

  moving_average.Reset();

  EXPECT_EQ(0, moving_average.GetAverageRoundedDown());
  moving_average.AddSample(10);
  EXPECT_EQ(10, moving_average.GetAverageRoundedDown());
  EXPECT_EQ(10, moving_average.GetAverageRoundedToClosest());
}

TEST(MovingAverageTest, ManySamples) {
  MovingAverage moving_average(10);
  for (int i = 1; i < 11; i++) {
    moving_average.AddSample(i);
  }
  EXPECT_EQ(moving_average.GetAverageRoundedDown(), 5);
  EXPECT_EQ(moving_average.GetAverageRoundedToClosest(), 6);
  for (int i = 1; i < 2001; i++) {
    moving_average.AddSample(i);
  }
  EXPECT_EQ(moving_average.GetAverageRoundedDown(), 1995);
  EXPECT_EQ(moving_average.GetAverageRoundedToClosest(), 1996);
}

TEST(MovingAverageTest, VerifyNoOverflow) {
  constexpr int kMaxInt = std::numeric_limits<int>::max();
  MovingAverage moving_average(255);
  for (int i = 0; i < 255; i++) {
    moving_average.AddSample(kMaxInt);
  }
  EXPECT_EQ(moving_average.GetAverageRoundedDown(), kMaxInt);
  EXPECT_EQ(moving_average.GetAverageRoundedToClosest(), kMaxInt);
  EXPECT_EQ(moving_average.GetUnroundedAverage(), kMaxInt);
}

}  // namespace test
}  // namespace base
