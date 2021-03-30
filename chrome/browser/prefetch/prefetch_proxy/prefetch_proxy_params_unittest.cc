// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_params.h"

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/prefetch/prefetch_proxy/prefetch_proxy_features.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PrefetchProxyParamsTest, PrefetchPosition_DefaultEmpty) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders, {{"prefetch_positions", ""}});

  for (size_t want_position :
       std::vector<size_t>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}) {
    SCOPED_TRACE(want_position);
    EXPECT_TRUE(PrefetchProxyShouldPrefetchPosition(want_position));
  }
}

TEST(PrefetchProxyParamsTest, PrefetchPosition_SingleIndex) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders, {{"prefetch_positions", "0"}});

  EXPECT_TRUE(PrefetchProxyShouldPrefetchPosition(0));

  for (size_t not_want_position :
       std::vector<size_t>{1, 2, 3, 4, 5, 6, 7, 8, 9}) {
    SCOPED_TRACE(not_want_position);
    EXPECT_FALSE(PrefetchProxyShouldPrefetchPosition(not_want_position));
  }
}

TEST(PrefetchProxyParamsTest, PrefetchPosition_Normal) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders, {{"prefetch_positions", "0,1,2"}});

  for (size_t want_position : std::vector<size_t>{0, 1, 2}) {
    SCOPED_TRACE(want_position);
    EXPECT_TRUE(PrefetchProxyShouldPrefetchPosition(want_position));
  }

  for (size_t not_want_position : std::vector<size_t>{3, 4, 5, 6, 7, 8, 9}) {
    SCOPED_TRACE(not_want_position);
    EXPECT_FALSE(PrefetchProxyShouldPrefetchPosition(not_want_position));
  }
}

TEST(PrefetchProxyParamsTest, PrefetchPosition_Messy) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders,
      {{"prefetch_positions", "  2,3, invalid, 5,,7"}});

  for (size_t want_position : std::vector<size_t>{2, 3, 5, 7}) {
    SCOPED_TRACE(want_position);
    EXPECT_TRUE(PrefetchProxyShouldPrefetchPosition(want_position));
  }

  for (size_t not_want_position : std::vector<size_t>{0, 1, 4, 6, 8, 9}) {
    SCOPED_TRACE(not_want_position);
    EXPECT_FALSE(PrefetchProxyShouldPrefetchPosition(not_want_position));
  }
}

TEST(PrefetchProxyParamsTest, DecoyProbabilityClampedZero) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders,
      {{"ineligible_decoy_request_probability", "-1"}});

  for (size_t i = 0; i < 100; i++) {
    EXPECT_FALSE(PrefetchProxySendDecoyRequestForIneligiblePrefetch());
  }
}

TEST(PrefetchProxyParamsTest, DecoyProbabilityClampedOne) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders,
      {{"ineligible_decoy_request_probability", "2"}});

  for (size_t i = 0; i < 100; i++) {
    EXPECT_TRUE(PrefetchProxySendDecoyRequestForIneligiblePrefetch());
  }
}
