// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search/ntp_features.h"

#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"

namespace ntp_features {

using testing::ElementsAre;

TEST(NTPFeaturesTest, ModulesLoadTimeout) {
  base::test::ScopedFeatureList scoped_feature_list_;

  // The default value can be overridden.
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kNtpModulesLoadTimeoutMilliseconds,
        {{kNtpModulesLoadTimeoutMillisecondsParam, "123"}}}},
      {});
  base::TimeDelta timeout = GetModulesLoadTimeout();
  EXPECT_EQ(123, timeout.InMilliseconds());

  // If the timeout is not parsable to an unsigned integer, the default value is
  // used.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kNtpModulesLoadTimeoutMilliseconds,
        {{kNtpModulesLoadTimeoutMillisecondsParam, "j"}}}},
      {});
  timeout = GetModulesLoadTimeout();
  EXPECT_EQ(3, timeout.InSeconds());
}

TEST(NTPFeaturesTest, ModulesOrder) {
  base::test::ScopedFeatureList scoped_feature_list_;

  // Can process list.
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kNtpModulesOrder, {{kNtpModulesOrderParam, "foo,bar"}}}}, {});
  EXPECT_THAT(GetModulesOrder(), ElementsAre("foo", "bar"));

  // Can process empty param.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kNtpModulesOrder, {{kNtpModulesOrderParam, ""}}}}, {});
  EXPECT_TRUE(GetModulesOrder().empty());
}

TEST(NTPFeaturesTest, CustomizeChromeSupportsChromeRefresh2023) {
  {
    // Chrome Refresh 2023 should be off when Customize Chrome is on but
    // Customize Chrome No Refresh is on, too.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {features::kCustomizeChromeSidePanel,
         features::kCustomizeChromeSidePanelNoChromeRefresh2023},
        {});
    EXPECT_FALSE(features::CustomizeChromeSupportsChromeRefresh2023());
  }

  {
    // Chrome Refresh 2023 should be on when Customize Chrome is on and
    // Customize Chrome No Refresh is off.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {features::kCustomizeChromeSidePanel},
        {features::kCustomizeChromeSidePanelNoChromeRefresh2023});
    EXPECT_TRUE(features::CustomizeChromeSupportsChromeRefresh2023());
  }

  {
    // Chrome Refresh 2023 should be off when Customize Chrome is off.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {}, {features::kCustomizeChromeSidePanel,
             features::kCustomizeChromeSidePanelNoChromeRefresh2023});
    EXPECT_FALSE(features::CustomizeChromeSupportsChromeRefresh2023());
  }

  {
    // Chrome Refresh 2023 should be off when Customize Chrome is off.
    base::test::ScopedFeatureList feature_list;
    feature_list.InitWithFeatures(
        {features::kCustomizeChromeSidePanelNoChromeRefresh2023},
        {features::kCustomizeChromeSidePanel});
    EXPECT_FALSE(features::CustomizeChromeSupportsChromeRefresh2023());
  }
}

}  // namespace ntp_features
