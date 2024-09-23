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

TEST(NTPFeaturesTest, WallpaperSearchButtonAnimationShownThreshold) {
  base::test::ScopedFeatureList scoped_feature_list_;

  // If the param is unset, the default value is used.
  int threshold = GetWallpaperSearchButtonAnimationShownThreshold();
  EXPECT_EQ(15, threshold);

  // Unsigned integers override the default value.
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kNtpWallpaperSearchButtonAnimationShownThreshold,
        {{kNtpWallpaperSearchButtonAnimationShownThresholdParam, "20"}}}},
      {});
  threshold = GetWallpaperSearchButtonAnimationShownThreshold();
  EXPECT_EQ(20, threshold);

  // Signed integers override the default value.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kNtpWallpaperSearchButtonAnimationShownThreshold,
        {{kNtpWallpaperSearchButtonAnimationShownThresholdParam, "-20"}}}},
      {});
  threshold = GetWallpaperSearchButtonAnimationShownThreshold();
  EXPECT_EQ(-20, threshold);

  // If the param is not parsable to an integer, the default value is
  // used.
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      {{kNtpWallpaperSearchButtonAnimationShownThreshold,
        {{kNtpWallpaperSearchButtonAnimationShownThresholdParam, "j"}}}},
      {});
  threshold = GetWallpaperSearchButtonAnimationShownThreshold();
  EXPECT_EQ(15, threshold);
}
}  // namespace ntp_features
