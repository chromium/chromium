// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/revisit_cdf_container.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {

std::vector<RevisitCdfContainer::Entry> kDefaultCdf{
    {1, 0.1f},
    {2, 0.2f},
    {5, 0.3f},
    {10, 1.0f},
};

class RevisitCdfContainerTest : public testing::Test {};

TEST_F(RevisitCdfContainerTest, ReturnsZeroIfUnderLowestBucket) {
  RevisitCdfContainer cdf(kDefaultCdf);

  EXPECT_EQ(0.0f, cdf.GetProbability(0));
}

TEST_F(RevisitCdfContainerTest, ReturnsOneIfAboveOrEqualToHighestBucket) {
  RevisitCdfContainer cdf(kDefaultCdf);

  EXPECT_EQ(1.0f, cdf.GetProbability(11));
  EXPECT_EQ(1.0f, cdf.GetProbability(10));
}

TEST_F(RevisitCdfContainerTest, ReturnsProbFromBucket) {
  RevisitCdfContainer cdf(kDefaultCdf);

  EXPECT_EQ(0.1f, cdf.GetProbability(1));
  EXPECT_EQ(0.2f, cdf.GetProbability(2));
  EXPECT_EQ(0.2f, cdf.GetProbability(3));
  EXPECT_EQ(0.3f, cdf.GetProbability(5));
}

}  // namespace performance_manager
