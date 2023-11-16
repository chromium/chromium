// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/probability_distribution.h"

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

std::vector<ProbabilityDistribution::Entry> kDefaultCdf{
    {1, 0.1f},
    {2, 0.2f},
    {5, 0.3f},
    {10, 1.0f},
};

class ProbabilityDistributionTest : public testing::Test {};

TEST_F(ProbabilityDistributionTest, ReturnsZeroIfUnderLowestBucket) {
  ProbabilityDistribution cdf =
      ProbabilityDistribution::FromCDFData(kDefaultCdf);

  EXPECT_EQ(0.0f, cdf.GetProbability(0));
}

TEST_F(ProbabilityDistributionTest,
       ReturnsLastBucketIfAboveOrEqualToHighestBucket) {
  ProbabilityDistribution cdf =
      ProbabilityDistribution::FromCDFData(kDefaultCdf);

  EXPECT_EQ(1.0f, cdf.GetProbability(11));
  EXPECT_EQ(1.0f, cdf.GetProbability(10));

  ProbabilityDistribution dist = ProbabilityDistribution::FromOrderedData({
      {1, 0.1f},
      {2, 0.3f},
      {5, 0.2f},
      {10, 0.4f},
  });

  EXPECT_EQ(0.4f, dist.GetProbability(11));
  EXPECT_EQ(0.4f, dist.GetProbability(10));
}

TEST_F(ProbabilityDistributionTest, ReturnsProbFromBucket) {
  ProbabilityDistribution cdf =
      ProbabilityDistribution::FromCDFData(kDefaultCdf);

  EXPECT_EQ(0.1f, cdf.GetProbability(1));
  EXPECT_EQ(0.2f, cdf.GetProbability(2));
  EXPECT_EQ(0.2f, cdf.GetProbability(3));
  EXPECT_EQ(0.3f, cdf.GetProbability(5));
}

TEST_F(ProbabilityDistributionTest, CrashesIfCreatingCdfFromNonCdfData) {
  EXPECT_DCHECK_DEATH(ProbabilityDistribution::FromCDFData({
      {1, 0.1f},
      {2, 0.3f},
      {5, 0.2f},
      {10, 0.4f},
  }));
  EXPECT_DCHECK_DEATH(ProbabilityDistribution::FromCDFData({
      {1, 0.1f},
      {2, 0.2f},
      {5, 0.3f},
  }));
}

TEST_F(ProbabilityDistributionTest, CrashesIfBucketsNotOrdered) {
  EXPECT_DCHECK_DEATH(ProbabilityDistribution::FromCDFData({
      {5, 0.2f},
      {2, 0.3f},
      {10, 1.0f},
  }));
  EXPECT_DCHECK_DEATH(ProbabilityDistribution::FromOrderedData({
      {5, 0.2f},
      {2, 0.3f},
      {10, 1.0f},
  }));
}

TEST_F(ProbabilityDistributionTest, CrashesIfValuesOutSideRange) {
  EXPECT_DCHECK_DEATH(ProbabilityDistribution::FromCDFData({
      {1, 0.1f},
      {2, 0.2f},
      {5, 0.3f},
      {10, 1.2f},
  }));
  EXPECT_DCHECK_DEATH(ProbabilityDistribution::FromOrderedData({
      {1, 0.1f},
      {2, 0.2f},
      {5, 0.3f},
      {10, 1.2f},
  }));

  EXPECT_DCHECK_DEATH(ProbabilityDistribution::FromCDFData({
      {1, -0.1f},
      {2, 0.2f},
      {5, 0.3f},
      {10, 1.0f},
  }));
  EXPECT_DCHECK_DEATH(ProbabilityDistribution::FromOrderedData({
      {1, -0.1f},
      {2, 0.2f},
      {5, 0.3f},
      {10, 1.0f},
  }));
}

}  // namespace performance_manager
