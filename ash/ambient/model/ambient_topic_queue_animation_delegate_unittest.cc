// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_topic_queue_animation_delegate.h"

#include <utility>

#include "base/check.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_piece.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/geometry/size.h"

namespace ash {
namespace {

using ::testing::UnorderedElementsAre;

}  // namespace

class AmbientTopicQueueAnimationDelegateTest : public ::testing::Test {
 protected:
  void RegisterAsset(base::StringPiece resource_id,
                     absl::optional<gfx::Size> size) {
    CHECK(resource_metadata_.RegisterAsset("test-path", "test-name",
                                           resource_id, std::move(size)))
        << "Asset " << resource_id << " already registered";
  }

  cc::SkottieResourceMetadataMap resource_metadata_;
};

TEST_F(AmbientTopicQueueAnimationDelegateTest,
       GetTopicSizesWithPortraitAndLandscape) {
  RegisterAsset("landscape-1", gfx::Size(100, 50));
  RegisterAsset("landscape-2", gfx::Size(120, 40));
  RegisterAsset("portrait-1", gfx::Size(50, 100));
  RegisterAsset("portrait-2", gfx::Size(60, 80));
  AmbientTopicQueueAnimationDelegate delegate(resource_metadata_);
  EXPECT_THAT(
      delegate.GetTopicSizes(),
      UnorderedElementsAre(gfx::Size(125, 50),
                           gfx::Size(base::ClampRound<int>(62.5), 100)));
}

TEST_F(AmbientTopicQueueAnimationDelegateTest, GetTopicSizesWithOnlyPortrait) {
  RegisterAsset("portrait-1", gfx::Size(60, 100));
  RegisterAsset("portrait-2", gfx::Size(100, 125));
  AmbientTopicQueueAnimationDelegate delegate(resource_metadata_);
  EXPECT_THAT(
      delegate.GetTopicSizes(),
      UnorderedElementsAre(gfx::Size(100, base::ClampRound<int>(100 / .7f))));
}

TEST_F(AmbientTopicQueueAnimationDelegateTest, GetTopicSizesWithOnlyLandscape) {
  RegisterAsset("landscape-1", gfx::Size(200, 100));
  RegisterAsset("landscape-2", gfx::Size(120, 40));
  AmbientTopicQueueAnimationDelegate delegate(resource_metadata_);
  EXPECT_THAT(delegate.GetTopicSizes(),
              UnorderedElementsAre(gfx::Size(250, 100)));
}

TEST_F(AmbientTopicQueueAnimationDelegateTest, GetTopicSizesWithSquare) {
  RegisterAsset("landscape-1", gfx::Size(200, 100));
  RegisterAsset("landscape-2", gfx::Size(120, 40));
  // Should be ignored when calculating the average aspect ratio.
  RegisterAsset("landscape-3", gfx::Size(300, 300));
  AmbientTopicQueueAnimationDelegate delegate(resource_metadata_);
  EXPECT_THAT(delegate.GetTopicSizes(),
              UnorderedElementsAre(gfx::Size(750, 300)));
}

TEST_F(AmbientTopicQueueAnimationDelegateTest, GetTopicSizesWithOnlySquare) {
  RegisterAsset("square-1", gfx::Size(200, 200));
  RegisterAsset("square-2", gfx::Size(100, 100));
  AmbientTopicQueueAnimationDelegate delegate(resource_metadata_);
  EXPECT_THAT(delegate.GetTopicSizes(),
              UnorderedElementsAre(gfx::Size(200, 200)));
}

TEST_F(AmbientTopicQueueAnimationDelegateTest, HandlesMissingAssetSize) {
  RegisterAsset("landscape-1", gfx::Size(200, 100));
  RegisterAsset("landscape-2", gfx::Size(120, 40));
  RegisterAsset("landscape-3", absl::nullopt);
  AmbientTopicQueueAnimationDelegate delegate(resource_metadata_);
  EXPECT_THAT(delegate.GetTopicSizes(),
              UnorderedElementsAre(gfx::Size(250, 100)));
}

}  // namespace ash
