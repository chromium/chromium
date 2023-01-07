// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_params.h"

#include <vector>

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/preloading/prefetch/prefetch_proxy/prefetch_proxy_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/unified_consent/pref_names.h"
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
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterBooleanPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);

  for (size_t i = 0; i < 100; i++) {
    EXPECT_FALSE(
        PrefetchProxySendDecoyRequestForIneligiblePrefetch(&pref_service));
  }
}

TEST(PrefetchProxyParamsTest, DecoyProbabilityClampedOne) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders,
      {{"ineligible_decoy_request_probability", "2"}});
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterBooleanPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);

  for (size_t i = 0; i < 100; i++) {
    EXPECT_TRUE(
        PrefetchProxySendDecoyRequestForIneligiblePrefetch(&pref_service));
  }
}

TEST(PrefetchProxyParamsTest, NoDecoysForMSBB) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kIsolatePrerenders,
      {{"ineligible_decoy_request_probability", "1"},
       {"disable_decoys_for_msbb", "true"}});
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterBooleanPref(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, true);
  for (size_t i = 0; i < 100; i++) {
    EXPECT_FALSE(
        PrefetchProxySendDecoyRequestForIneligiblePrefetch(&pref_service));
  }

  pref_service.SetBoolean(
      unified_consent::prefs::kUrlKeyedAnonymizedDataCollectionEnabled, false);
  for (size_t i = 0; i < 100; i++) {
    EXPECT_TRUE(
        PrefetchProxySendDecoyRequestForIneligiblePrefetch(&pref_service));
  }
}

TEST(PrefetchProxyParamsTest, BypassProxyForHost) {
  EXPECT_FALSE(PrefetchProxyBypassProxyForHost().has_value());
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      "bypass-prefetch-proxy-for-host", "www.www1.hostname.test");
  EXPECT_TRUE(PrefetchProxyBypassProxyForHost().has_value());
  EXPECT_EQ(PrefetchProxyBypassProxyForHost().value(),
            "www.www1.hostname.test");
}
