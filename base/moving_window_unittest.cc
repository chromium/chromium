// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/moving_window.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

constexpr int kTestValues[] = {
    33, 1, 2, 7, 5, 2, 4, 45, 1000, 1, 100, 2, 200, 2,  2, 2, 300, 4, 1,
    2,  3, 4, 5, 6, 7, 8, 9,  10,   9, 8,   7, 6,   5,  4, 3, 2,   1, 1,
    2,  1, 4, 2, 1, 8, 1, 2,  1,    4, 1,   2, 1,   16, 1, 2, 1};

}  // namespace

class MovingMaxTest : public testing::TestWithParam<unsigned int> {};

INSTANTIATE_TEST_SUITE_P(All,
                         MovingMaxTest,
                         testing::ValuesIn({1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u,
                                            10u, 17u, 20u, 100u}));

TEST_P(MovingMaxTest, BlanketTest) {
  const size_t window_size = GetParam();
  MovingMax<int> window(window_size);
  for (size_t i = 0; i < std::size(kTestValues); ++i) {
    window.AddSample(kTestValues[i]);
    int slow_max = kTestValues[i];
    for (size_t j = 1; j < window_size && j <= i; ++j) {
      slow_max = std::max(slow_max, kTestValues[i - j]);
    }
    EXPECT_EQ(window.Max(), slow_max);
  }
}

TEST(MovingMax, SingleElementWindow) {
  MovingMax<int> window(1u);
  window.AddSample(100);
  EXPECT_EQ(window.Max(), 100);
  window.AddSample(1000);
  EXPECT_EQ(window.Max(), 1000);
  window.AddSample(1);
  EXPECT_EQ(window.Max(), 1);
  window.AddSample(3);
  EXPECT_EQ(window.Max(), 3);
  window.AddSample(4);
  EXPECT_EQ(window.Max(), 4);
}

TEST(MovingMax, VeryLargeWindow) {
  MovingMax<int> window(100u);
  window.AddSample(100);
  EXPECT_EQ(window.Max(), 100);
  window.AddSample(1000);
  EXPECT_EQ(window.Max(), 1000);
  window.AddSample(1);
  EXPECT_EQ(window.Max(), 1000);
  window.AddSample(3);
  EXPECT_EQ(window.Max(), 1000);
  window.AddSample(4);
  EXPECT_EQ(window.Max(), 1000);
}

TEST(MovingMax, Counts) {
  MovingMax<int> window(3u);
  EXPECT_EQ(window.Count(), 0u);
  window.AddSample(100);
  EXPECT_EQ(window.Count(), 1u);
  window.AddSample(1000);
  EXPECT_EQ(window.Count(), 2u);
  window.AddSample(1);
  EXPECT_EQ(window.Count(), 3u);
  window.AddSample(3);
  EXPECT_EQ(window.Count(), 4u);
  window.AddSample(4);
  EXPECT_EQ(window.Count(), 5u);
}

TEST(MovingAverage, Unrounded) {
  MovingAverage<int, int64_t> window(4u);
  window.AddSample(1);
  EXPECT_EQ(window.Mean<double>(), 1.0);
  window.AddSample(2);
  EXPECT_EQ(window.Mean<double>(), 1.5);
  window.AddSample(3);
  EXPECT_EQ(window.Mean<double>(), 2.0);
  window.AddSample(4);
  EXPECT_EQ(window.Mean<double>(), 2.5);
  window.AddSample(101);
  EXPECT_EQ(window.Mean<double>(), 27.5);
}

class MovingMinTest : public testing::TestWithParam<unsigned int> {};

INSTANTIATE_TEST_SUITE_P(All,
                         MovingMinTest,
                         testing::ValuesIn({1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u,
                                            10u, 17u, 20u, 100u}));

TEST_P(MovingMinTest, BlanketTest) {
  const size_t window_size = GetParam();
  MovingMin<int> window(window_size);
  for (int repeats = 0; repeats < 2; ++repeats) {
    for (size_t i = 0; i < std::size(kTestValues); ++i) {
      window.AddSample(kTestValues[i]);
      int slow_min = kTestValues[i];
      for (size_t j = 1; j < window_size && j <= i; ++j) {
        slow_min = std::min(slow_min, kTestValues[i - j]);
      }
      EXPECT_EQ(window.Min(), slow_min);
    }
    window.Reset();
  }
}

class MovingAverageTest : public testing::TestWithParam<unsigned int> {};

INSTANTIATE_TEST_SUITE_P(All,
                         MovingAverageTest,
                         testing::ValuesIn({1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u,
                                            10u, 17u, 20u, 100u}));

TEST_P(MovingAverageTest, BlanketTest) {
  const size_t window_size = GetParam();
  MovingAverage<int, int64_t> window(window_size);
  for (int repeats = 0; repeats < 2; ++repeats) {
    for (size_t i = 0; i < std::size(kTestValues); ++i) {
      window.AddSample(kTestValues[i]);
      int slow_mean = 0;
      for (size_t j = 0; j < window_size && j <= i; ++j) {
        slow_mean += kTestValues[i - j];
      }
      slow_mean /= std::min(window_size, i + 1);
      EXPECT_EQ(window.Mean(), slow_mean);
    }
    window.Reset();
  }
}

class MovingDeviationTest : public testing::TestWithParam<unsigned int> {};

INSTANTIATE_TEST_SUITE_P(All,
                         MovingDeviationTest,
                         testing::ValuesIn({1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u,
                                            10u, 17u, 20u, 100u}));

TEST_P(MovingDeviationTest, BlanketTest) {
  const size_t window_size = GetParam();
  MovingAverageDeviation<double> window(window_size);
  for (int repeats = 0; repeats < 2; ++repeats) {
    for (size_t i = 0; i < std::size(kTestValues); ++i) {
      window.AddSample(kTestValues[i]);
      double slow_deviation = 0;
      double mean = window.Mean();
      for (size_t j = 0; j < window_size && j <= i; ++j) {
        slow_deviation +=
            (kTestValues[i - j] - mean) * (kTestValues[i - j] - mean);
      }
      slow_deviation /= std::min(window_size, i + 1);
      slow_deviation = sqrt(slow_deviation);
      double fast_deviation = window.Deviation();
      EXPECT_TRUE(std::abs(fast_deviation - slow_deviation) < 1e-9);
    }
    window.Reset();
  }
}

TEST(MovingWindowTest, Iteration) {
  const size_t kWindowSize = 10;
  MovingWindow<int, base::MovingWindowFeatures::Iteration> window(kWindowSize);
  for (int repeats = 0; repeats < 2; ++repeats) {
    for (size_t i = 0; i < std::size(kTestValues); ++i) {
      window.AddSample(kTestValues[i]);
      size_t j = 0;
      const size_t in_window = std::min(i + 1, kWindowSize);
      for (int value : window) {
        ASSERT_LT(j, in_window);
        EXPECT_EQ(value, kTestValues[i + j + 1 - in_window]);
        ++j;
      }
      EXPECT_EQ(j, in_window);
    }
    window.Reset();
  }
}

TEST(MovingMeanDeviation, WorksWithTimeDelta) {
  MovingAverageDeviation<base::TimeDelta> window(2);
  window.AddSample(base::Milliseconds(400));
  window.AddSample(base::Milliseconds(200));
  EXPECT_EQ(window.Mean(), base::Milliseconds(300));
  EXPECT_EQ(window.Deviation(), base::Milliseconds(100));
  window.AddSample(base::Seconds(40));
  window.AddSample(base::Seconds(20));
  EXPECT_EQ(window.Mean(), base::Seconds(30));
  EXPECT_EQ(window.Deviation(), base::Seconds(10));
}

}  // namespace base
