// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_topic_queue_animation_delegate.h"

#include <optional>
#include <string_view>
#include <utility>

#include "ash/ambient/test/ambient_test_util.h"
#include "base/check.h"
#include "base/numerics/safe_conversions.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace {

using ::testing::UnorderedElementsAre;

}  // namespace

class AmbientTopicQueueAnimationDelegateTest : public ::testing::Test {
 protected:
  void RegisterAsset(std::string_view resource_id,
                     std::optional<gfx::Size> size) {
    CHECK(resource_metadata_.RegisterAsset("test-path", "test-name",
                                           resource_id, std::move(size)))
        << "Asset " << resource_id << " already registered";
  }

  cc::SkottieResourceMetadataMap resource_metadata_;
};

TEST_F(AmbientTopicQueueAnimationDelegateTest,
       GetTopicSizesWithPortraitAndLandscape) {
  RegisterAsset(
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"A", /*idx=*/1),
      gfx::Size(100, 50));
  RegisterAsset(
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"B", /*idx=*/1),
      gfx::Size(120, 40));
  RegisterAsset(
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"C", /*idx=*/1),
      gfx::Size(50, 100));
  RegisterAsset(
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"D", /*idx=*/1),
      gfx::Size(60, 80));
  AmbientTopicQueueAnimationDelegate delegate(resource_metadata_);
  EXPECT_THAT(
      delegate.GetTopicSizes(),
      UnorderedElementsAre(gfx::Size(125, 50),
                           gfx::Size(base::ClampRound<int>(62.5), 100)));
}

TEST_F(AmbientTopicQueueAnimationDelegateTest, GetTopicSizesWithOnlyPortrait) {
  RegisterAsset(
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"A", /*idx=*/1),
      gfx::Size(60, 100));
  RegisterAsset(
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"B", /*idx=*/1),
      gfx::Size(100, 125));
  AmbientTopicQueueAnimationDelegate delegate(resource_metadata_);
  EXPECT_THAT(
      delegate.GetTopicSizes(),
      UnorderedElementsAre(gfx::Size(100, base::ClampRound<int>(100 / .7f))));
}

TEST_F(AmbientTopicQueueAnimationDelegateTest, GetTopicSizesWithOnlyLandscape) {
  RegisterAsset(
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"A", /*idx=*/1),
      gfx::Size(200, 100));
  RegisterAsset(
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"B", /*idx=*/1),
      gfx::Size(120, 40));
  AmbientTopicQueueAnimationDelegate delegate(resource_metadata_);
  EXPECT_THAT(delegate.GetTopicSizes(),
              UnorderedElementsAre(gfx::Size(250, 100)));
}

TEST_F(AmbientTopicQueueAnimationDelegateTest, GetTopicSizesWithSquare) {
  RegisterAsset(
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"A", /*idx=*/1),
      gfx::Size(200, 100));
  RegisterAsset(
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"B", /*idx=*/1),
      gfx::Size(120, 40));
  // Should be ignored when calculating the average aspect ratio.
  RegisterAsset(
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"C", /*idx=*/1),
      gfx::Size(300, 300));
  AmbientTopicQueueAnimationDelegate delegate(resource_metadata_);
  EXPECT_THAT(delegate.GetTopicSizes(),
              UnorderedElementsAre(gfx::Size(750, 300)));
}

TEST_F(AmbientTopicQueueAnimationDelegateTest, GetTopicSizesWithOnlySquare) {
  RegisterAsset(
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"A", /*idx=*/1),
      gfx::Size(200, 200));
  RegisterAsset(
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"B", /*idx=*/1),
      gfx::Size(100, 100));
  AmbientTopicQueueAnimationDelegate delegate(resource_metadata_);
  EXPECT_THAT(delegate.GetTopicSizes(),
              UnorderedElementsAre(gfx::Size(200, 200)));
}

TEST_F(AmbientTopicQueueAnimationDelegateTest, HandlesMissingAssetSize) {
  RegisterAsset(
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"A", /*idx=*/1),
      gfx::Size(200, 100));
  RegisterAsset(
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"B", /*idx=*/1),
      gfx::Size(120, 40));
  RegisterAsset(
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"C", /*idx=*/1),
      std::nullopt);
  AmbientTopicQueueAnimationDelegate delegate(resource_metadata_);
  EXPECT_THAT(delegate.GetTopicSizes(),
              UnorderedElementsAre(gfx::Size(250, 100)));
}

TEST_F(AmbientTopicQueueAnimationDelegateTest, FiltersOutStaticImageAssets) {
  RegisterAsset(
      GenerateLottieDynamicAssetIdForTesting(/*position=*/"A", /*idx=*/1),
      gfx::Size(200, 100));
  RegisterAsset("static-image-asset-id", gfx::Size(120, 40));
  AmbientTopicQueueAnimationDelegate delegate(resource_metadata_);
  EXPECT_THAT(delegate.GetTopicSizes(),
              UnorderedElementsAre(gfx::Size(200, 100)));
}

}  // namespace ash
