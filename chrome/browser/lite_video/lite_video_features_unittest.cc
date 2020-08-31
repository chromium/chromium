// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lite_video/lite_video_features.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(LiteVideoFeaturesTest, LiteVideoDisabled) {
  EXPECT_EQ(0u, lite_video::features::GetLiteVideoPermanentBlocklist().size());
}

TEST(LiteVideoFeaturesTest, PermanentHostBlocklistEmpty) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature({::features::kLiteVideo});
  EXPECT_EQ(0u, lite_video::features::GetLiteVideoPermanentBlocklist().size());
}

TEST(LiteVideoFeaturesTest, PermanentHostBlocklist_WrongType) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      ::features::kLiteVideo,
      {{"permanent_host_blocklist", "{\"litevideo.com\": 123}"}});
  EXPECT_EQ(0u, lite_video::features::GetLiteVideoPermanentBlocklist().size());
}

TEST(LiteVideoFeaturesTest, PermanentHostBlocklistFilled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      ::features::kLiteVideo, {{"permanent_host_blocklist",
                                "[\"litevideo.com\", \"video2.com\", 12]"}});
  auto permanent_host_blocklist =
      lite_video::features::GetLiteVideoPermanentBlocklist();
  EXPECT_TRUE(permanent_host_blocklist.contains("litevideo.com"));
  EXPECT_TRUE(permanent_host_blocklist.contains("video2.com"));
  EXPECT_FALSE(permanent_host_blocklist.contains("allowed_host.com"));
  EXPECT_FALSE(permanent_host_blocklist.contains("12"));
}
