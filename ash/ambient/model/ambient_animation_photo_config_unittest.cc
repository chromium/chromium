// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_animation_photo_config.h"

#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/test/ambient_test_util.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

using ::testing::Eq;

TEST(AmbientAnimationPhotoConfigTest, GetNumAssets) {
  cc::SkottieResourceMetadataMap skottie_resource_metadata;
  {
    AmbientAnimationPhotoConfig photo_config(skottie_resource_metadata);
    EXPECT_THAT(photo_config.GetNumAssets(), Eq(0));
  }
  ASSERT_TRUE(skottie_resource_metadata.RegisterAsset(
      "test-resource-path", "test-resource-name-0",
      GenerateTestLottieDynamicAssetId(/*unique_id=*/0)));
  {
    AmbientAnimationPhotoConfig photo_config(skottie_resource_metadata);
    EXPECT_THAT(photo_config.GetNumAssets(), Eq(1));
  }
  ASSERT_TRUE(skottie_resource_metadata.RegisterAsset(
      "test-resource-path", "test-resource-name-1",
      GenerateTestLottieDynamicAssetId(/*unique_id=*/1)));
  {
    AmbientAnimationPhotoConfig photo_config(skottie_resource_metadata);
    EXPECT_THAT(photo_config.GetNumAssets(), Eq(2));
  }
}

TEST(AmbientAnimationPhotoConfigTest, GetNumAssetsInTopic) {
  cc::SkottieResourceMetadataMap skottie_resource_metadata;
  AmbientAnimationPhotoConfig photo_config(skottie_resource_metadata);
  PhotoWithDetails downloaded_topic;
  EXPECT_THAT(photo_config.GetNumAssetsInTopic(downloaded_topic), Eq(0));
  downloaded_topic.photo =
      gfx::test::CreateImageSkia(/*width=*/100, /*height=*/100);
  EXPECT_THAT(photo_config.GetNumAssetsInTopic(downloaded_topic), Eq(1));
  downloaded_topic.related_photo =
      gfx::test::CreateImageSkia(/*width=*/100, /*height=*/100);
  EXPECT_THAT(photo_config.GetNumAssetsInTopic(downloaded_topic), Eq(2));
}

}  // namespace ash
