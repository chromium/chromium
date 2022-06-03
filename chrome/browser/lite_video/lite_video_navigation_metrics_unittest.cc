// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_navigation_metrics.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/lite_video/lite_video_features.h"
#include "chrome/browser/lite_video/lite_video_user_blocklist.h"
#include "chrome/common/chrome_features.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(LiteVideoNavigationMetrics, ShouldStopOnRebufferForFrame) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      ::features::kLiteVideo, {{"max_rebuffers_per_frame", "2"}});

  auto nav_metrics = lite_video::LiteVideoNavigationMetrics(
      1, lite_video::LiteVideoDecision::kUnknown,
      lite_video::LiteVideoBlocklistReason::kUnknown,
      lite_video::LiteVideoThrottleResult::kUnknown);

  EXPECT_FALSE(nav_metrics.ShouldStopOnRebufferForFrame(1));
  EXPECT_TRUE(nav_metrics.ShouldStopOnRebufferForFrame(1));
  EXPECT_TRUE(nav_metrics.ShouldStopOnRebufferForFrame(1));

  EXPECT_FALSE(nav_metrics.ShouldStopOnRebufferForFrame(2));
  EXPECT_TRUE(nav_metrics.ShouldStopOnRebufferForFrame(2));
}
