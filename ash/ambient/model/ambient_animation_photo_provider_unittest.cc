// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_animation_photo_provider.h"

#include <utility>
#include <vector>

#include "ash/ambient/model/ambient_animation_photo_config.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/resources/ambient_animation_static_resources.h"
#include "ash/ambient/test/ambient_test_util.h"
#include "ash/ambient/test/fake_ambient_animation_static_resources.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "cc/paint/skottie_frame_data.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::NotNull;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;
using ImageAsset = ::cc::SkottieFrameDataProvider::ImageAsset;

namespace {

constexpr float kTestScaleFactor = 1;

// Test argument is absl::optional<cc::SkottieFrameData>.
MATCHER_P2(HasImageDimensions, width, height, "") {
  if (!arg.has_value())
    return false;

  return arg->image.GetSkImageInfo().width() == width &&
         arg->image.GetSkImageInfo().height() == height;
}

}  // namespace

// Example has 4 dynamic assets in the animation.
class AmbientAnimationPhotoProviderTest : public ::testing::Test {
 protected:
  static constexpr int kNumDynamicAssets = 4;

  AmbientAnimationPhotoProviderTest()
      : model_(
            CreateAmbientAnimationPhotoConfig(BuildSkottieResourceMetadata())),
        provider_(&static_resources_, &model_) {}

  cc::SkottieResourceMetadataMap BuildSkottieResourceMetadata() const {
    cc::SkottieResourceMetadataMap resource_metadata;
    for (int i = 0; i < kNumDynamicAssets; ++i) {
      CHECK(resource_metadata.RegisterAsset(
          "dummy-resource-path", "dummy-resource-name",
          GenerateTestLottieDynamicAssetId(i)));
    }
    return resource_metadata;
  }

  void AddImageToModel(gfx::ImageSkia image) {
    PhotoWithDetails decoded_topic;
    decoded_topic.photo = std::move(image);
    model_.AddNextImage(decoded_topic);
  }

  scoped_refptr<ImageAsset> LoadAsset(base::StringPiece asset_id) {
    scoped_refptr<ImageAsset> asset = provider_.LoadImageAsset(
        asset_id, base::FilePath("dummy-resource-path/dummy-resource-name"));
    CHECK(asset) << asset_id;
    return asset;
  }

  std::vector<scoped_refptr<ImageAsset>> LoadAllDynamicAssets() {
    std::vector<scoped_refptr<ImageAsset>> all_assets;
    for (int asset_idx = 0; asset_idx < kNumDynamicAssets; ++asset_idx) {
      all_assets.push_back(
          LoadAsset(GenerateTestLottieDynamicAssetId(asset_idx)));
    }
    return all_assets;
  }

  std::vector<absl::optional<cc::SkottieFrameData>> GetFrameDataForAssets(
      const std::vector<scoped_refptr<ImageAsset>>& assets,
      float timestamp) {
    // The timestamp for a given frame is not guaranteed to be the same for each
    // asset per Skottie's API. Apply jitter to ensure the provider handles this
    // correctly.
    static constexpr float kTimestampJitter = 0.01f;
    bool add_jitter = false;
    std::vector<absl::optional<cc::SkottieFrameData>> all_frame_data;
    for (const scoped_refptr<ImageAsset>& asset : assets) {
      float jitter = add_jitter ? kTimestampJitter : 0.f;
      all_frame_data.push_back(
          asset->GetFrameData(timestamp + jitter, kTestScaleFactor));
      add_jitter = !add_jitter;
    }
    return all_frame_data;
  }

  AmbientBackendModel model_;
  FakeAmbientAnimationStaticResources static_resources_;
  AmbientAnimationPhotoProvider provider_;
};

TEST_F(AmbientAnimationPhotoProviderTest,
       RefreshesDynamicAssetsAtStartOfCycle) {
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/11, /*height=*/11));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/12, /*height=*/12));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/13, /*height=*/13));

  std::vector<scoped_refptr<ImageAsset>> all_assets = LoadAllDynamicAssets();

  // Cycle 0 Frame 0
  std::vector<absl::optional<cc::SkottieFrameData>> frame_data =
      GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  ASSERT_THAT(frame_data, Each(IsTrue()));
  EXPECT_THAT(frame_data, UnorderedElementsAre(HasImageDimensions(10, 10),
                                               HasImageDimensions(11, 11),
                                               HasImageDimensions(12, 12),
                                               HasImageDimensions(13, 13)));

  // Cycle 0 Frame 1
  frame_data = GetFrameDataForAssets(all_assets, /*timestamp=*/1);
  EXPECT_THAT(frame_data, Each(IsFalse()));

  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/20, /*height=*/20));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/21, /*height=*/21));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/22, /*height=*/22));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/23, /*height=*/23));

  // Cycle 1 Frame 0
  frame_data = GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  ASSERT_THAT(frame_data, Each(IsTrue()));
  EXPECT_THAT(frame_data, UnorderedElementsAre(HasImageDimensions(20, 20),
                                               HasImageDimensions(21, 21),
                                               HasImageDimensions(22, 22),
                                               HasImageDimensions(23, 23)));

  // Cycle 1 Frame 1
  frame_data = GetFrameDataForAssets(all_assets, /*timestamp=*/1);
  EXPECT_THAT(frame_data, Each(IsFalse()));
}

TEST_F(AmbientAnimationPhotoProviderTest,
       RefreshesDynamicAssetsWithPartialAssetsAvailable) {
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/11, /*height=*/11));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/12, /*height=*/12));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/13, /*height=*/13));

  std::vector<scoped_refptr<ImageAsset>> all_assets = LoadAllDynamicAssets();

  // Cycle 0 Frame 0
  GetFrameDataForAssets(all_assets, /*timestamp=*/0);

  // Cycle 0 Frame 1
  GetFrameDataForAssets(all_assets, /*timestamp=*/1);

  // Only 2 new images were prepared for the next cycle. The new cycle should
  // incorporate the 2 new images plus the 2 most recent images from the last
  // cycle.
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/20, /*height=*/20));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/21, /*height=*/21));

  // Cycle 1 Frame 0
  std::vector<absl::optional<cc::SkottieFrameData>> frame_data =
      GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  ASSERT_THAT(frame_data, Each(IsTrue()));
  EXPECT_THAT(frame_data, UnorderedElementsAre(HasImageDimensions(12, 12),
                                               HasImageDimensions(13, 13),
                                               HasImageDimensions(20, 20),
                                               HasImageDimensions(21, 21)));
}

TEST_F(AmbientAnimationPhotoProviderTest,
       DistributesTopicsEvenlyWithMoreAssetsThanTopics) {
  // Only 2 images in model, when there are 4 assets.
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/11, /*height=*/11));

  std::vector<scoped_refptr<ImageAsset>> all_assets = LoadAllDynamicAssets();

  // Cycle 0 Frame 0
  std::vector<absl::optional<cc::SkottieFrameData>> frame_data =
      GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  ASSERT_THAT(frame_data, Each(IsTrue()));
  EXPECT_THAT(frame_data, UnorderedElementsAre(HasImageDimensions(10, 10),
                                               HasImageDimensions(10, 10),
                                               HasImageDimensions(11, 11),
                                               HasImageDimensions(11, 11)));
}

TEST_F(AmbientAnimationPhotoProviderTest,
       HandlesMinimumTopicsAvailableInModel) {
  // Only 1 image in model, when there are 4 assets.
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10));

  std::vector<scoped_refptr<ImageAsset>> all_assets = LoadAllDynamicAssets();

  // Cycle 0 Frame 0
  std::vector<absl::optional<cc::SkottieFrameData>> frame_data =
      GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  ASSERT_THAT(frame_data, Each(IsTrue()));
  ASSERT_THAT(frame_data, SizeIs(kNumDynamicAssets));
  EXPECT_THAT(frame_data, Each(HasImageDimensions(10, 10)));
}

TEST_F(AmbientAnimationPhotoProviderTest, LoadsStaticImageAssets) {
  static_resources_.SetStaticImageAsset(
      "static-asset-0",
      gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10));
  static_resources_.SetStaticImageAsset(
      "static-asset-1",
      gfx::test::CreateImageSkia(/*width=*/11, /*height=*/11));

  std::vector<scoped_refptr<ImageAsset>> all_assets = {
      LoadAsset("static-asset-0"), LoadAsset("static-asset-1")};

  std::vector<absl::optional<cc::SkottieFrameData>> frame_data =
      GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  ASSERT_THAT(frame_data, Each(IsTrue()));
  EXPECT_THAT(frame_data, ElementsAre(HasImageDimensions(10, 10),
                                      HasImageDimensions(11, 11)));

  frame_data = GetFrameDataForAssets(all_assets, /*timestamp=*/1);
  EXPECT_THAT(frame_data, Each(IsFalse()));

  // Unlike dynamic assets, static assets only get loaded one time in the
  // animation's lifetime.
  frame_data = GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  EXPECT_THAT(frame_data, Each(IsFalse()));
}

}  // namespace ash
