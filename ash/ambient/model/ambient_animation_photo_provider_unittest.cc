// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_animation_photo_provider.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/model/ambient_animation_photo_config.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/resources/ambient_animation_resource_constants.h"
#include "ash/ambient/resources/ambient_animation_static_resources.h"
#include "ash/ambient/test/ambient_test_util.h"
#include "ash/ambient/test/fake_ambient_animation_static_resources.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "cc/paint/skottie_frame_data.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/image/image_unittest_util.h"

namespace ash {

using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::FieldsAre;
using ::testing::Invoke;
using ::testing::IsSubsetOf;
using ::testing::Key;
using ::testing::Mock;
using ::testing::NotNull;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;
using ImageAsset = ::cc::SkottieFrameDataProvider::ImageAsset;

namespace {

constexpr float kTestScaleFactor = 1;

// Test argument is cc::SkottieFrameData.
MATCHER_P2(HasImageDimensions, width, height, "") {
  return arg.image.GetSkImageInfo().width() == width &&
         arg.image.GetSkImageInfo().height() == height;
}

MATCHER_P(MatchesDimensionsFrom, other, "") {
  return arg.image.GetSkImageInfo().width() ==
             other.image.GetSkImageInfo().width() &&
         arg.image.GetSkImageInfo().height() ==
             other.image.GetSkImageInfo().height();
}

MATCHER_P(TopicHasDetails, expected_details, "") {
  return arg.get().details == expected_details;
}

class MockObserver : public AmbientAnimationPhotoProvider::Observer {
 public:
  explicit MockObserver(AmbientAnimationPhotoProvider* provider) {
    observation_.Observe(provider);
  }
  MockObserver(const MockObserver&) = delete;
  MockObserver& operator=(const MockObserver&) = delete;
  ~MockObserver() override = default;

  // AmbientAnimationPhotoProvider::Observer implementation:
  MOCK_METHOD(
      void,
      OnDynamicImageAssetsRefreshed,
      ((const base::flat_map<ambient::util::ParsedDynamicAssetId,
                             std::reference_wrapper<const PhotoWithDetails>>&)),
      (override));

 private:
  base::ScopedObservation<AmbientAnimationPhotoProvider,
                          AmbientAnimationPhotoProvider::Observer>
      observation_{this};
};

}  // namespace

// Example has 4 dynamic assets in the animation.
class AmbientAnimationPhotoProviderTest : public ::testing::Test {
 protected:
  static constexpr int kNumDynamicAssets = 4;

  explicit AmbientAnimationPhotoProviderTest(
      std::array<std::string, kNumDynamicAssets> dynamic_asset_ids =
          GetDefaultDynamicAssetIds())
      : dynamic_asset_ids_(dynamic_asset_ids),
        model_(
            CreateAmbientAnimationPhotoConfig(BuildSkottieResourceMetadata())),
        provider_(&static_resources_, &model_) {}

  cc::SkottieResourceMetadataMap BuildSkottieResourceMetadata() const {
    cc::SkottieResourceMetadataMap resource_metadata;
    for (int i = 0; i < kNumDynamicAssets; ++i) {
      CHECK(resource_metadata.RegisterAsset(
          "dummy-resource-path", "dummy-resource-name", dynamic_asset_ids_[i],
          /*size=*/std::nullopt));
    }
    return resource_metadata;
  }

  void AddImageToModel(gfx::ImageSkia image,
                       std::string details = std::string()) {
    PhotoWithDetails decoded_topic;
    decoded_topic.photo = std::move(image);
    decoded_topic.details = std::move(details);
    model_.AddNextImage(decoded_topic);
  }

  scoped_refptr<ImageAsset> LoadAsset(
      std::string_view asset_id,
      std::optional<gfx::Size> size = std::nullopt) {
    scoped_refptr<ImageAsset> asset = provider_.LoadImageAsset(
        asset_id, base::FilePath("dummy-resource-path/dummy-resource-name"),
        std::move(size));
    CHECK(asset) << asset_id;
    return asset;
  }

  std::vector<scoped_refptr<ImageAsset>> LoadAllDynamicAssets(
      std::array<std::optional<gfx::Size>, kNumDynamicAssets> asset_sizes =
          std::array<std::optional<gfx::Size>, kNumDynamicAssets>()) {
    std::vector<scoped_refptr<ImageAsset>> all_assets;
    char position_id = 'A';
    for (int asset_idx = 0; asset_idx < kNumDynamicAssets;
         ++asset_idx, ++position_id) {
      all_assets.push_back(
          LoadAsset(dynamic_asset_ids_[asset_idx], asset_sizes[asset_idx]));
    }
    return all_assets;
  }

  std::vector<cc::SkottieFrameData> GetFrameDataForAssets(
      const std::vector<scoped_refptr<ImageAsset>>& assets,
      float timestamp,
      float scale = kTestScaleFactor) {
    // The timestamp for a given frame is not guaranteed to be the same for each
    // asset per Skottie's API. Apply jitter to ensure the provider handles this
    // correctly.
    static constexpr float kTimestampJitter = 0.01f;
    bool add_jitter = false;
    std::vector<cc::SkottieFrameData> all_frame_data;
    for (const scoped_refptr<ImageAsset>& asset : assets) {
      float jitter = add_jitter ? kTimestampJitter : 0.f;
      all_frame_data.push_back(asset->GetFrameData(timestamp + jitter, scale));
      add_jitter = !add_jitter;
    }
    return all_frame_data;
  }

  static std::array<std::string, kNumDynamicAssets>
  GetDefaultDynamicAssetIds() {
    return {
        GenerateLottieDynamicAssetIdForTesting(
            /*position=*/"A", /*idx=*/1),
        GenerateLottieDynamicAssetIdForTesting(
            /*position=*/"B", /*idx=*/1),
        GenerateLottieDynamicAssetIdForTesting(
            /*position=*/"C", /*idx=*/1),
        GenerateLottieDynamicAssetIdForTesting(
            /*position=*/"D", /*idx=*/1),
    };
  }

  const std::array<std::string, kNumDynamicAssets> dynamic_asset_ids_;
  base::test::TaskEnvironment task_environment_;
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
  std::vector<cc::SkottieFrameData> frame_data =
      GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  EXPECT_THAT(frame_data, UnorderedElementsAre(HasImageDimensions(10, 10),
                                               HasImageDimensions(11, 11),
                                               HasImageDimensions(12, 12),
                                               HasImageDimensions(13, 13)));

  // Cycle 0 Frame 1
  frame_data = GetFrameDataForAssets(all_assets, /*timestamp=*/1);
  EXPECT_THAT(frame_data, UnorderedElementsAre(HasImageDimensions(10, 10),
                                               HasImageDimensions(11, 11),
                                               HasImageDimensions(12, 12),
                                               HasImageDimensions(13, 13)));

  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/20, /*height=*/20));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/21, /*height=*/21));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/22, /*height=*/22));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/23, /*height=*/23));

  // Cycle 1 Frame 0
  frame_data = GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  EXPECT_THAT(frame_data, UnorderedElementsAre(HasImageDimensions(20, 20),
                                               HasImageDimensions(21, 21),
                                               HasImageDimensions(22, 22),
                                               HasImageDimensions(23, 23)));

  // Cycle 1 Frame 1
  frame_data = GetFrameDataForAssets(all_assets, /*timestamp=*/1);
  EXPECT_THAT(frame_data, UnorderedElementsAre(HasImageDimensions(20, 20),
                                               HasImageDimensions(21, 21),
                                               HasImageDimensions(22, 22),
                                               HasImageDimensions(23, 23)));
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
  std::vector<cc::SkottieFrameData> frame_data =
      GetFrameDataForAssets(all_assets, /*timestamp=*/0);
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
  std::vector<cc::SkottieFrameData> frame_data =
      GetFrameDataForAssets(all_assets, /*timestamp=*/0);
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
  std::vector<cc::SkottieFrameData> frame_data =
      GetFrameDataForAssets(all_assets, /*timestamp=*/0);
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

  std::vector<cc::SkottieFrameData> frame_data =
      GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  EXPECT_THAT(frame_data, ElementsAre(HasImageDimensions(10, 10),
                                      HasImageDimensions(11, 11)));

  frame_data = GetFrameDataForAssets(all_assets, /*timestamp=*/1);
  EXPECT_THAT(frame_data, ElementsAre(HasImageDimensions(10, 10),
                                      HasImageDimensions(11, 11)));

  // Unlike dynamic assets, static assets only get loaded one time in the
  // animation's lifetime.
  frame_data = GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  EXPECT_THAT(frame_data, ElementsAre(HasImageDimensions(10, 10),
                                      HasImageDimensions(11, 11)));
}

TEST_F(AmbientAnimationPhotoProviderTest, MatchesDynamicAssetOrientation) {
  // 2 landscape 2 portrait
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/10, /*height=*/20));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/20, /*height=*/10));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/20, /*height=*/40));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/40, /*height=*/20));

  std::vector<scoped_refptr<ImageAsset>> all_assets =
      LoadAllDynamicAssets({gfx::Size(100, 50), gfx::Size(50, 100),
                            gfx::Size(100, 50), gfx::Size(50, 100)});

  std::vector<cc::SkottieFrameData> frame_data =
      GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  EXPECT_THAT(std::vector<cc::SkottieFrameData>({frame_data[0], frame_data[2]}),
              UnorderedElementsAre(HasImageDimensions(20, 10),
                                   HasImageDimensions(40, 20)));
  EXPECT_THAT(std::vector<cc::SkottieFrameData>({frame_data[1], frame_data[3]}),
              UnorderedElementsAre(HasImageDimensions(10, 20),
                                   HasImageDimensions(20, 40)));
  GetFrameDataForAssets(all_assets, /*timestamp=*/1);

  // 3 landscape 1 portrait
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/10, /*height=*/20));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/60, /*height=*/30));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/80, /*height=*/40));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/100, /*height=*/50));
  frame_data = GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  // Portrait asset expectations:
  EXPECT_THAT(frame_data[1], HasImageDimensions(10, 20));
  EXPECT_THAT(frame_data[3],
              AnyOf(HasImageDimensions(15, 30), HasImageDimensions(20, 40),
                    HasImageDimensions(25, 50)));
  // Landscape asset expectations:
  EXPECT_THAT(
      std::vector<cc::SkottieFrameData>({frame_data[0], frame_data[2]}),
      IsSubsetOf({HasImageDimensions(60, 30), HasImageDimensions(80, 40),
                  HasImageDimensions(100, 50)}));
  GetFrameDataForAssets(all_assets, /*timestamp=*/1);

  // // 1 landscape 3 portrait
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/30, /*height=*/60));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/20, /*height=*/10));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/40, /*height=*/80));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/50, /*height=*/100));
  frame_data = GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  // Landscape asset expectations:
  EXPECT_THAT(frame_data[0], HasImageDimensions(20, 10));
  EXPECT_THAT(frame_data[2],
              AnyOf(HasImageDimensions(30, 15), HasImageDimensions(40, 20),
                    HasImageDimensions(50, 25)));
  // Portrait asset expectations:
  EXPECT_THAT(
      std::vector<cc::SkottieFrameData>({frame_data[1], frame_data[3]}),
      IsSubsetOf({HasImageDimensions(30, 60), HasImageDimensions(40, 80),
                  HasImageDimensions(50, 100)}));
}

TEST_F(AmbientAnimationPhotoProviderTest, HandlesOnlyPortraitAvailable) {
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/10, /*height=*/20));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/30, /*height=*/60));

  std::vector<scoped_refptr<ImageAsset>> all_assets =
      LoadAllDynamicAssets({gfx::Size(100, 50), gfx::Size(50, 100),
                            gfx::Size(100, 50), gfx::Size(50, 100)});

  std::vector<cc::SkottieFrameData> frame_data =
      GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  EXPECT_THAT(
      std::vector<cc::SkottieFrameData>({frame_data[0], frame_data[1]}),
      AnyOf(
          ElementsAre(HasImageDimensions(10, 5), HasImageDimensions(30, 60)),
          ElementsAre(HasImageDimensions(30, 15), HasImageDimensions(10, 20))));
  EXPECT_THAT(
      std::vector<cc::SkottieFrameData>({frame_data[2], frame_data[3]}),
      AnyOf(
          ElementsAre(HasImageDimensions(10, 5), HasImageDimensions(30, 60)),
          ElementsAre(HasImageDimensions(30, 15), HasImageDimensions(10, 20))));
}

TEST_F(AmbientAnimationPhotoProviderTest, HandlesOnlyLandscapeAvailable) {
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/20, /*height=*/10));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/60, /*height=*/30));

  std::vector<scoped_refptr<ImageAsset>> all_assets =
      LoadAllDynamicAssets({gfx::Size(100, 50), gfx::Size(50, 100),
                            gfx::Size(100, 50), gfx::Size(50, 100)});

  std::vector<cc::SkottieFrameData> frame_data =
      GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  EXPECT_THAT(
      std::vector<cc::SkottieFrameData>({frame_data[0], frame_data[1]}),
      AnyOf(
          ElementsAre(HasImageDimensions(20, 10), HasImageDimensions(15, 30)),
          ElementsAre(HasImageDimensions(60, 30), HasImageDimensions(5, 10))));
  EXPECT_THAT(
      std::vector<cc::SkottieFrameData>({frame_data[2], frame_data[3]}),
      AnyOf(
          ElementsAre(HasImageDimensions(20, 10), HasImageDimensions(15, 30)),
          ElementsAre(HasImageDimensions(60, 30), HasImageDimensions(5, 10))));
}

TEST_F(AmbientAnimationPhotoProviderTest,
       NotifiesObserversWhenDynamicAssetsRefreshed) {
  MockObserver observer(&provider_);

  gfx::ImageSkia test_image =
      gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);
  AddImageToModel(test_image, "attribution-a");
  AddImageToModel(test_image, "attribution-b");
  AddImageToModel(test_image, "attribution-c");
  AddImageToModel(test_image, "attribution-d");

  std::vector<scoped_refptr<ImageAsset>> all_assets = LoadAllDynamicAssets();

  // Cycle 0 Frame 0
  EXPECT_CALL(
      observer,
      OnDynamicImageAssetsRefreshed(AllOf(
          ElementsAre(Key(FieldsAre("A", 1)), Key(FieldsAre("B", 1)),
                      Key(FieldsAre("C", 1)), Key(FieldsAre("D", 1))),
          UnorderedElementsAre(Pair(_, TopicHasDetails("attribution-a")),
                               Pair(_, TopicHasDetails("attribution-b")),
                               Pair(_, TopicHasDetails("attribution-c")),
                               Pair(_, TopicHasDetails("attribution-d"))))));
  GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  Mock::VerifyAndClearExpectations(&observer);

  // Cycle 0 Frame 1
  EXPECT_CALL(observer, OnDynamicImageAssetsRefreshed(_)).Times(0);
  GetFrameDataForAssets(all_assets, /*timestamp=*/1);
  Mock::VerifyAndClearExpectations(&observer);

  AddImageToModel(test_image, "attribution-e");
  AddImageToModel(test_image, "attribution-f");
  AddImageToModel(test_image, "attribution-g");
  AddImageToModel(test_image, "attribution-h");

  // Cycle 1 Frame 0
  EXPECT_CALL(
      observer,
      OnDynamicImageAssetsRefreshed(AllOf(
          ElementsAre(Key(FieldsAre("A", 1)), Key(FieldsAre("B", 1)),
                      Key(FieldsAre("C", 1)), Key(FieldsAre("D", 1))),
          UnorderedElementsAre(Pair(_, TopicHasDetails("attribution-e")),
                               Pair(_, TopicHasDetails("attribution-f")),
                               Pair(_, TopicHasDetails("attribution-g")),
                               Pair(_, TopicHasDetails("attribution-h"))))));
  GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  Mock::VerifyAndClearExpectations(&observer);

  // Cycle 1 Frame 1
  EXPECT_CALL(observer, OnDynamicImageAssetsRefreshed(_)).Times(0);
  GetFrameDataForAssets(all_assets, /*timestamp=*/1);
  Mock::VerifyAndClearExpectations(&observer);
}

TEST_F(AmbientAnimationPhotoProviderTest,
       NotifiesObserversWhenDynamicAssetsDuplicated) {
  MockObserver observer(&provider_);

  // Only 1 image in model, when there are 4 assets.
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10),
                  "attribution-a");

  std::vector<scoped_refptr<ImageAsset>> all_assets = LoadAllDynamicAssets();

  // Cycle 0 Frame 0
  EXPECT_CALL(observer, OnDynamicImageAssetsRefreshed(AllOf(
                            SizeIs(kNumDynamicAssets),
                            Each(Pair(_, TopicHasDetails("attribution-a"))))));
  GetFrameDataForAssets(all_assets, /*timestamp=*/0);
}

TEST_F(AmbientAnimationPhotoProviderTest, LoadsDifferentImageScaleFactor) {
  gfx::ImageSkia test_image =
      gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);
  test_image.AddRepresentation(
      gfx::ImageSkiaRep(gfx::test::CreateBitmap(/*width=*/20, /*height=*/20),
                        /*scale=*/kTestScaleFactor * 2));
  AddImageToModel(test_image);

  std::vector<scoped_refptr<ImageAsset>> all_assets = LoadAllDynamicAssets();

  // Load at 1x.
  std::vector<cc::SkottieFrameData> frame_data =
      GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  ASSERT_THAT(frame_data, SizeIs(kNumDynamicAssets));
  EXPECT_THAT(frame_data, Each(HasImageDimensions(10, 10)));

  // Cycle 0 Frame 1
  frame_data = GetFrameDataForAssets(all_assets, /*timestamp=*/0.5,
                                     /*scale=*/kTestScaleFactor * 2);
  ASSERT_THAT(frame_data, SizeIs(kNumDynamicAssets));
  EXPECT_THAT(frame_data, Each(HasImageDimensions(20, 20)));
}

TEST_F(AmbientAnimationPhotoProviderTest, ToggleStaticImageAsset) {
  static_resources_.SetStaticImageAsset(
      ambient::resources::kTreeShadowAssetId,
      gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10));

  scoped_refptr<ImageAsset> tree_shadow =
      LoadAsset(ambient::resources::kTreeShadowAssetId);
  ASSERT_THAT(tree_shadow, NotNull());

  ASSERT_TRUE(provider_.ToggleStaticImageAsset(
      cc::HashSkottieResourceId(ambient::resources::kTreeShadowAssetId),
      false));
  EXPECT_FALSE(tree_shadow->GetFrameData(/*t=*/0, kTestScaleFactor).image);

  ASSERT_TRUE(provider_.ToggleStaticImageAsset(
      cc::HashSkottieResourceId(ambient::resources::kTreeShadowAssetId), true));
  EXPECT_TRUE(tree_shadow->GetFrameData(/*t=*/0, kTestScaleFactor).image);
}

class AmbientAnimationPhotoProviderTestMultipleAssetsPerPosition
    : public AmbientAnimationPhotoProviderTest {
 protected:
  enum AssetIdx {
    kPositionAIdx1 = 0,
    kPositionAIdx2,
    kPositionBIdx1,
    kPositionBIdx2,
  };

  AmbientAnimationPhotoProviderTestMultipleAssetsPerPosition()
      : AmbientAnimationPhotoProviderTest(GetDynamicAssetIds()) {}

 private:
  static std::array<std::string, kNumDynamicAssets> GetDynamicAssetIds() {
    return {
        GenerateLottieDynamicAssetIdForTesting(
            /*position=*/"A", /*idx=*/1),
        GenerateLottieDynamicAssetIdForTesting(
            /*position=*/"A", /*idx=*/2),
        GenerateLottieDynamicAssetIdForTesting(
            /*position=*/"B", /*idx=*/1),
        GenerateLottieDynamicAssetIdForTesting(
            /*position=*/"B", /*idx=*/2),
    };
  }
};

TEST_F(AmbientAnimationPhotoProviderTestMultipleAssetsPerPosition,
       RefreshesDynamicAssetsAtStartOfCycle) {
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/11, /*height=*/11));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/12, /*height=*/12));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/13, /*height=*/13));

  scoped_refptr<ImageAsset> asset_a_1 =
      LoadAsset(dynamic_asset_ids_[kPositionAIdx1]);
  scoped_refptr<ImageAsset> asset_a_2 =
      LoadAsset(dynamic_asset_ids_[kPositionAIdx2]);
  scoped_refptr<ImageAsset> asset_b_1 =
      LoadAsset(dynamic_asset_ids_[kPositionBIdx1]);
  scoped_refptr<ImageAsset> asset_b_2 =
      LoadAsset(dynamic_asset_ids_[kPositionBIdx2]);
  std::vector<scoped_refptr<ImageAsset>> all_assets(kNumDynamicAssets);
  all_assets[kPositionAIdx1] = asset_a_1;
  all_assets[kPositionAIdx2] = asset_a_2;
  all_assets[kPositionBIdx1] = asset_b_1;
  all_assets[kPositionBIdx2] = asset_b_2;

  // Cycle 0 Frame 0
  std::vector<cc::SkottieFrameData> frame_data =
      GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  EXPECT_THAT(frame_data, UnorderedElementsAre(HasImageDimensions(10, 10),
                                               HasImageDimensions(11, 11),
                                               HasImageDimensions(12, 12),
                                               HasImageDimensions(13, 13)));

  // Cycle 0 Frame 1
  frame_data = GetFrameDataForAssets(all_assets, /*timestamp=*/1);
  EXPECT_THAT(frame_data, UnorderedElementsAre(HasImageDimensions(10, 10),
                                               HasImageDimensions(11, 11),
                                               HasImageDimensions(12, 12),
                                               HasImageDimensions(13, 13)));

  cc::SkottieFrameData asset_a_2_previous = frame_data[kPositionAIdx2];
  cc::SkottieFrameData asset_b_2_previous = frame_data[kPositionBIdx2];

  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/20, /*height=*/20));
  AddImageToModel(gfx::test::CreateImageSkia(/*width=*/21, /*height=*/21));

  // Cycle 1 Frame 0
  frame_data = GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  EXPECT_THAT(frame_data[kPositionAIdx1],
              MatchesDimensionsFrom(asset_a_2_previous));
  EXPECT_THAT(frame_data[kPositionBIdx1],
              MatchesDimensionsFrom(asset_b_2_previous));
  std::vector<cc::SkottieFrameData> idx_2_assets = {frame_data[kPositionAIdx2],
                                                    frame_data[kPositionBIdx2]};
  EXPECT_THAT(idx_2_assets, UnorderedElementsAre(HasImageDimensions(20, 20),
                                                 HasImageDimensions(21, 21)));

  // Cycle 1 Frame 1
  frame_data = GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  EXPECT_THAT(frame_data[kPositionAIdx1],
              MatchesDimensionsFrom(asset_a_2_previous));
  EXPECT_THAT(frame_data[kPositionBIdx1],
              MatchesDimensionsFrom(asset_b_2_previous));
  idx_2_assets = {frame_data[kPositionAIdx2], frame_data[kPositionBIdx2]};
  EXPECT_THAT(idx_2_assets, UnorderedElementsAre(HasImageDimensions(20, 20),
                                                 HasImageDimensions(21, 21)));
}

TEST_F(AmbientAnimationPhotoProviderTestMultipleAssetsPerPosition,
       NotifiesObserversWhenDynamicAssetsRefreshed) {
  MockObserver observer(&provider_);

  gfx::ImageSkia test_image =
      gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);
  AddImageToModel(test_image, "attribution-a");
  AddImageToModel(test_image, "attribution-b");
  AddImageToModel(test_image, "attribution-c");
  AddImageToModel(test_image, "attribution-d");

  std::vector<scoped_refptr<ImageAsset>> all_assets = LoadAllDynamicAssets();

  std::string asset_a_2_prev_attribution;
  std::string asset_b_2_prev_attribution;
  // Cycle 0 Frame 0
  EXPECT_CALL(
      observer,
      OnDynamicImageAssetsRefreshed(AllOf(
          ElementsAre(Key(FieldsAre("A", 1)), Key(FieldsAre("B", 1)),
                      Key(FieldsAre("A", 2)), Key(FieldsAre("B", 2))),
          UnorderedElementsAre(Pair(_, TopicHasDetails("attribution-a")),
                               Pair(_, TopicHasDetails("attribution-b")),
                               Pair(_, TopicHasDetails("attribution-c")),
                               Pair(_, TopicHasDetails("attribution-d"))))))
      .WillOnce(Invoke(
          [&](const base::flat_map<
              ambient::util::ParsedDynamicAssetId,
              std::reference_wrapper<const PhotoWithDetails>>& new_topics) {
            asset_a_2_prev_attribution =
                new_topics.at(ambient::util::ParsedDynamicAssetId({"A", 2}))
                    .get()
                    .details;
            asset_b_2_prev_attribution =
                new_topics.at(ambient::util::ParsedDynamicAssetId({"B", 2}))
                    .get()
                    .details;
          }));
  GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  Mock::VerifyAndClearExpectations(&observer);

  // Cycle 0 Frame 1
  EXPECT_CALL(observer, OnDynamicImageAssetsRefreshed(_)).Times(0);
  GetFrameDataForAssets(all_assets, /*timestamp=*/1);
  Mock::VerifyAndClearExpectations(&observer);

  AddImageToModel(test_image, "attribution-e");
  AddImageToModel(test_image, "attribution-f");

  // Cycle 1 Frame 0
  EXPECT_CALL(
      observer,
      OnDynamicImageAssetsRefreshed(ElementsAre(
          Pair(FieldsAre("A", 1), TopicHasDetails(asset_a_2_prev_attribution)),
          Pair(FieldsAre("B", 1), TopicHasDetails(asset_b_2_prev_attribution)),
          Pair(FieldsAre("A", 2), AnyOf(TopicHasDetails("attribution-e"),
                                        TopicHasDetails("attribution-f"))),
          Pair(FieldsAre("B", 2), AnyOf(TopicHasDetails("attribution-e"),
                                        TopicHasDetails("attribution-f"))))));
  GetFrameDataForAssets(all_assets, /*timestamp=*/0);
  Mock::VerifyAndClearExpectations(&observer);

  // Cycle 1 Frame 1
  EXPECT_CALL(observer, OnDynamicImageAssetsRefreshed(_)).Times(0);
  GetFrameDataForAssets(all_assets, /*timestamp=*/1);
  Mock::VerifyAndClearExpectations(&observer);
}

}  // namespace ash
