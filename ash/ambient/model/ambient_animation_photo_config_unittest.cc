// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_animation_photo_config.h"

#include <optional>

#include "ash/ambient/test/ambient_test_util.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using ::testing::Eq;

TEST(AmbientAnimationPhotoConfigTest, SetsTopicSetFields) {
  cc::SkottieResourceMetadataMap skottie_resource_metadata;
  AmbientPhotoConfig config =
      CreateAmbientAnimationPhotoConfig(skottie_resource_metadata);
  EXPECT_THAT(config.topic_set_size, Eq(0u));
  EXPECT_THAT(config.num_topic_sets_to_buffer, Eq(0u));

  ASSERT_TRUE(skottie_resource_metadata.RegisterAsset(
      "test-resource-path", "test-resource-name-0",
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"A", /*idx=*/1),
      /*size=*/std::nullopt));
  ASSERT_TRUE(skottie_resource_metadata.RegisterAsset(
      "test-resource-path", "test-resource-name-0",
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"B", /*idx=*/1),
      /*size=*/std::nullopt));
  config = CreateAmbientAnimationPhotoConfig(skottie_resource_metadata);
  EXPECT_THAT(config.topic_set_size, Eq(2u));
  EXPECT_THAT(config.num_topic_sets_to_buffer, Eq(1u));

  ASSERT_TRUE(skottie_resource_metadata.RegisterAsset(
      "test-resource-path", "test-resource-name-0",
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"A", /*idx=*/2),
      /*size=*/std::nullopt));
  ASSERT_TRUE(skottie_resource_metadata.RegisterAsset(
      "test-resource-path", "test-resource-name-0",
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"B", /*idx=*/2),
      /*size=*/std::nullopt));
  config = CreateAmbientAnimationPhotoConfig(skottie_resource_metadata);
  EXPECT_THAT(config.topic_set_size, Eq(2u));
  EXPECT_THAT(config.num_topic_sets_to_buffer, Eq(2u));
}

TEST(AmbientAnimationPhotoConfigTest, DoesNotCountStaticAssets) {
  cc::SkottieResourceMetadataMap skottie_resource_metadata;
  ASSERT_TRUE(skottie_resource_metadata.RegisterAsset(
      "test-resource-path", "test-resource-name-0",
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"A", /*idx=*/1),
      /*size=*/std::nullopt));
  ASSERT_TRUE(skottie_resource_metadata.RegisterAsset(
      "test-resource-path", "test-resource-name-0",
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"B", /*idx=*/1),
      /*size=*/std::nullopt));
  ASSERT_TRUE(skottie_resource_metadata.RegisterAsset(
      "test-resource-path", "test-resource-name-0", "StaticAssetId1",
      /*size=*/std::nullopt));
  ASSERT_TRUE(skottie_resource_metadata.RegisterAsset(
      "test-resource-path", "test-resource-name-0", "StaticAssetId2",
      /*size=*/std::nullopt));
  AmbientPhotoConfig config =
      CreateAmbientAnimationPhotoConfig(skottie_resource_metadata);
  EXPECT_THAT(config.topic_set_size, Eq(2u));
  EXPECT_THAT(config.num_topic_sets_to_buffer, Eq(1u));
}

TEST(AmbientAnimationPhotoConfigTest, FatalIfAnimationAssetIdsInvalid) {
  cc::SkottieResourceMetadataMap skottie_resource_metadata;
  // Position A and Position B have a different number of assets assigned to
  // them. This is currently considered invalid.
  ASSERT_TRUE(skottie_resource_metadata.RegisterAsset(
      "test-resource-path", "test-resource-name-0",
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"A", /*idx=*/1),
      /*size=*/std::nullopt));
  ASSERT_TRUE(skottie_resource_metadata.RegisterAsset(
      "test-resource-path", "test-resource-name-0",
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"A", /*idx=*/2),
      /*size=*/std::nullopt));
  ASSERT_TRUE(skottie_resource_metadata.RegisterAsset(
      "test-resource-path", "test-resource-name-0",
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"B", /*idx=*/1),
      /*size=*/std::nullopt));
  EXPECT_DEATH_IF_SUPPORTED(
      CreateAmbientAnimationPhotoConfig(skottie_resource_metadata), "");
}

}  // namespace ash
