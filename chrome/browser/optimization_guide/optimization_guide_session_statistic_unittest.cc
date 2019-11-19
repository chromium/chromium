// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/optimization_guide_session_statistic.h"

#include <cmath>
#include "testing/gtest/include/gtest/gtest.h"

TEST(OptimzationGuideSessionStatisticTest,
     CalculateSessionStatisticsForSamples) {
  OptimizationGuideSessionStatistic stat;

  stat.AddSample(100.0);
  EXPECT_EQ(1u, stat.GetNumberOfSamples());
  EXPECT_EQ(100.0, stat.GetMean());
  EXPECT_EQ(0.0, stat.GetVariance());
  EXPECT_EQ(0.0, stat.GetStdDev());

  stat.AddSample(200.0);
  EXPECT_EQ(2u, stat.GetNumberOfSamples());
  EXPECT_EQ(150.0, stat.GetMean());
  EXPECT_EQ(5000.0, stat.GetVariance());

  stat.AddSample(150.0);
  EXPECT_EQ(3u, stat.GetNumberOfSamples());
  EXPECT_EQ(150.0, stat.GetMean());
  EXPECT_EQ(2500.0, stat.GetVariance());
  EXPECT_EQ(std::sqrt(2500.0), stat.GetStdDev());
}

TEST(OptimzationGuideSessionStatisticTest, VarianceWitthLessThanTwoSamples) {
  OptimizationGuideSessionStatistic stat;

  EXPECT_EQ(0u, stat.GetNumberOfSamples());
  EXPECT_EQ(0.0, stat.GetVariance());
  EXPECT_EQ(0.0, stat.GetStdDev());

  stat.AddSample(10.);
  EXPECT_EQ(1u, stat.GetNumberOfSamples());
  EXPECT_EQ(0.0, stat.GetVariance());
  EXPECT_EQ(0.0, stat.GetStdDev());
}
