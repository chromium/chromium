// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_animation_photo_config.h"

#include "ash/ambient/test/ambient_test_util.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using ::testing::Eq;

TEST(AmbientAnimationPhotoConfigTest, SetsTopicSetSize) {
  cc::SkottieResourceMetadataMap skottie_resource_metadata;
  EXPECT_THAT(CreateAmbientAnimationPhotoConfig(skottie_resource_metadata)
                  .topic_set_size,
              Eq(0u));
  ASSERT_TRUE(skottie_resource_metadata.RegisterAsset(
      "test-resource-path", "test-resource-name-0",
      GenerateTestLottieDynamicAssetId(/*unique_id=*/0)));
  EXPECT_THAT(CreateAmbientAnimationPhotoConfig(skottie_resource_metadata)
                  .topic_set_size,
              Eq(1u));
  ASSERT_TRUE(skottie_resource_metadata.RegisterAsset(
      "test-resource-path", "test-resource-name-1",
      GenerateTestLottieDynamicAssetId(/*unique_id=*/1)));
  EXPECT_THAT(CreateAmbientAnimationPhotoConfig(skottie_resource_metadata)
                  .topic_set_size,
              Eq(2u));
}

}  // namespace ash
