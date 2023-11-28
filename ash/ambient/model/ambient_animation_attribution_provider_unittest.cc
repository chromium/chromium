// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_animation_attribution_provider.h"

#include <functional>
#include <optional>
#include <string>
#include <utility>

#include "ash/ambient/model/ambient_animation_photo_config.h"
#include "ash/ambient/model/ambient_animation_photo_provider.h"
#include "ash/ambient/model/ambient_backend_model.h"
#include "ash/ambient/resources/ambient_animation_static_resources.h"
#include "ash/ambient/test/ambient_test_util.h"
#include "ash/ambient/test/fake_ambient_animation_static_resources.h"
#include "ash/public/cpp/ambient/proto/photo_cache_entry.pb.h"
#include "ash/utility/lottie_util.h"
#include "base/check.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "cc/paint/skottie_frame_data.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "cc/paint/skottie_text_property_value.h"
#include "cc/test/lottie_test_data.h"
#include "cc/test/skia_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/lottie/animation.h"

namespace ash {
namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;
using ImageAsset = ::cc::SkottieFrameDataProvider::ImageAsset;

MATCHER_P(HasText, text, "") {
  return arg.text() == text;
}

// Has 2 assets and 2 text nodes.
class AmbientAnimationAttributionProviderTest : public ::testing::Test {
 protected:
  // Default topic type is any arbitrary one for which attribution should
  // be displayed.
  static constexpr ::ambient::TopicType kDefaultTopicType = ::ambient::kCurated;

  AmbientAnimationAttributionProviderTest(std::string attribution_node_0,
                                          std::string attribution_node_1,
                                          std::string asset_id_0,
                                          std::string asset_id_1)
      : attribution_node_0_(std::move(attribution_node_0)),
        attribution_node_1_(std::move(attribution_node_1)),
        asset_id_0_(std::move(asset_id_0)),
        asset_id_1_(std::move(asset_id_1)),
        model_(
            CreateAmbientAnimationPhotoConfig(BuildSkottieResourceMetadata())),
        photo_provider_(&static_resources_, &model_),
        animation_(cc::CreateSkottieFromString(
            cc::CreateCustomLottieDataWith2TextNodes(attribution_node_0_,
                                                     attribution_node_1_))),
        attribution_provider_(&photo_provider_, &animation_) {}

  cc::SkottieResourceMetadataMap BuildSkottieResourceMetadata() const {
    cc::SkottieResourceMetadataMap resource_metadata;
    CHECK(resource_metadata.RegisterAsset("dummy-resource-path",
                                          "dummy-resource-name", asset_id_0_,
                                          /*size=*/std::nullopt));
    CHECK(resource_metadata.RegisterAsset("dummy-resource-path",
                                          "dummy-resource-name", asset_id_1_,
                                          /*size=*/std::nullopt));
    return resource_metadata;
  }

  void RefreshDynamicImageAssets(
      std::optional<std::string> asset_0_attribution,
      std::optional<std::string> asset_1_attribution,
      ::ambient::TopicType topic_type = kDefaultTopicType) {
    gfx::ImageSkia test_image =
        gfx::test::CreateImageSkia(/*width=*/10, /*height=*/10);
    base::flat_map<ambient::util::ParsedDynamicAssetId,
                   std::reference_wrapper<const PhotoWithDetails>>
        new_topics;
    ambient::util::ParsedDynamicAssetId parsed_asset_id;

    PhotoWithDetails topic_0;
    if (asset_0_attribution) {
      CHECK(ambient::util::ParseDynamicLottieAssetId(asset_id_0_,
                                                     parsed_asset_id));
      topic_0.photo = test_image;
      topic_0.details = std::move(*asset_0_attribution);
      topic_0.topic_type = topic_type;
      new_topics.emplace(parsed_asset_id, std::cref(topic_0));
    }

    PhotoWithDetails topic_1;
    if (asset_1_attribution) {
      CHECK(ambient::util::ParseDynamicLottieAssetId(asset_id_1_,
                                                     parsed_asset_id));
      topic_1.photo = test_image;
      topic_1.details = std::move(*asset_1_attribution);
      topic_1.topic_type = topic_type;
      new_topics.emplace(parsed_asset_id, std::cref(topic_1));
    }

    attribution_provider_.OnDynamicImageAssetsRefreshed(new_topics);
  }

  const std::string attribution_node_0_;
  const std::string attribution_node_1_;
  const std::string asset_id_0_;
  const std::string asset_id_1_;
  AmbientBackendModel model_;
  FakeAmbientAnimationStaticResources static_resources_;
  AmbientAnimationPhotoProvider photo_provider_;
  lottie::Animation animation_;
  AmbientAnimationAttributionProvider attribution_provider_;
};

class AmbientAnimationAttributionProviderTest2DynamicAssets
    : public AmbientAnimationAttributionProviderTest {
 protected:
  AmbientAnimationAttributionProviderTest2DynamicAssets()
      : AmbientAnimationAttributionProviderTest(
            std::string(kLottieCustomizableIdPrefix) + "_Attribution_Text0",
            std::string(kLottieCustomizableIdPrefix) + "_Attribution_Text1",
            GenerateLottieDynamicAssetIdForTesting(/*position=*/"A", /*idx=*/1),
            GenerateLottieDynamicAssetIdForTesting(/*position=*/"A",
                                                   /*idx=*/2)) {}
};

class AmbientAnimationAttributionProviderTestMultipleDigitIndex
    : public AmbientAnimationAttributionProviderTest {
 protected:
  AmbientAnimationAttributionProviderTestMultipleDigitIndex()
      : AmbientAnimationAttributionProviderTest(
            std::string(kLottieCustomizableIdPrefix) + "_Attribution_Text10",
            std::string(kLottieCustomizableIdPrefix) + "_Attribution_Text2",
            GenerateLottieDynamicAssetIdForTesting(/*position=*/"A",
                                                   /*idx=*/10),
            GenerateLottieDynamicAssetIdForTesting(/*position=*/"A",
                                                   /*idx=*/2)) {}
};

class AmbientAnimationAttributionProviderTest1DynamicAsset
    : public AmbientAnimationAttributionProviderTest {
 protected:
  AmbientAnimationAttributionProviderTest1DynamicAsset()
      : AmbientAnimationAttributionProviderTest(
            "static-text-node",
            std::string(kLottieCustomizableIdPrefix) + "_Attribution_Text1",
            "static-asset-id",
            GenerateLottieDynamicAssetIdForTesting(/*position=*/"A",
                                                   /*idx=*/1)) {}
};

class AmbientAnimationAttributionProviderTest2DynamicAssets1Attribution
    : public AmbientAnimationAttributionProviderTest {
 protected:
  AmbientAnimationAttributionProviderTest2DynamicAssets1Attribution()
      : AmbientAnimationAttributionProviderTest(
            "static-text-node",
            std::string(kLottieCustomizableIdPrefix) + "_Attribution_Text1",
            GenerateLottieDynamicAssetIdForTesting(/*position=*/"A", /*idx=*/1),
            GenerateLottieDynamicAssetIdForTesting(/*position=*/"A",
                                                   /*idx=*/2)) {}
};

TEST_F(AmbientAnimationAttributionProviderTest2DynamicAssets,
       SetsTextInAnimation) {
  RefreshDynamicImageAssets("attribution_text_0_a", "attribution_text_1_a");
  EXPECT_THAT(
      animation_.text_map(),
      UnorderedElementsAre(Pair(cc::HashSkottieResourceId(attribution_node_0_),
                                HasText("attribution_text_0_a")),
                           Pair(cc::HashSkottieResourceId(attribution_node_1_),
                                HasText("attribution_text_1_a"))));
  RefreshDynamicImageAssets("attribution_text_0_b", "attribution_text_1_b");
  EXPECT_THAT(
      animation_.text_map(),
      UnorderedElementsAre(Pair(cc::HashSkottieResourceId(attribution_node_0_),
                                HasText("attribution_text_0_b")),
                           Pair(cc::HashSkottieResourceId(attribution_node_1_),
                                HasText("attribution_text_1_b"))));
}

TEST_F(AmbientAnimationAttributionProviderTest2DynamicAssets,
       HandlesEmptyAttribution) {
  RefreshDynamicImageAssets("", "");
  EXPECT_THAT(
      animation_.text_map(),
      UnorderedElementsAre(
          Pair(cc::HashSkottieResourceId(attribution_node_0_), HasText("")),
          Pair(cc::HashSkottieResourceId(attribution_node_1_), HasText(""))));
  RefreshDynamicImageAssets("", "attribution_text_1_a");
  EXPECT_THAT(
      animation_.text_map(),
      UnorderedElementsAre(
          Pair(cc::HashSkottieResourceId(attribution_node_0_), HasText("")),
          Pair(cc::HashSkottieResourceId(attribution_node_1_),
               HasText("attribution_text_1_a"))));
  RefreshDynamicImageAssets("attribution_text_0_b", "");
  EXPECT_THAT(
      animation_.text_map(),
      UnorderedElementsAre(
          Pair(cc::HashSkottieResourceId(attribution_node_0_),
               HasText("attribution_text_0_b")),
          Pair(cc::HashSkottieResourceId(attribution_node_1_), HasText(""))));
}

TEST_F(AmbientAnimationAttributionProviderTestMultipleDigitIndex,
       SetsTextInAnimation) {
  RefreshDynamicImageAssets("attribution_text_for_asset_10",
                            "attribution_text_for_asset_2");
  EXPECT_THAT(
      animation_.text_map(),
      UnorderedElementsAre(Pair(cc::HashSkottieResourceId(attribution_node_0_),
                                HasText("attribution_text_for_asset_10")),
                           Pair(cc::HashSkottieResourceId(attribution_node_1_),
                                HasText("attribution_text_for_asset_2"))));
}

TEST_F(AmbientAnimationAttributionProviderTest1DynamicAsset,
       HandlesNonAttributionTextNodes) {
  RefreshDynamicImageAssets(std::nullopt, "attribution_text_1");
  // The static text node should the value that's baked into the lottie file
  // (|kLottieDataWith2TextNode1Text|).
  EXPECT_THAT(
      animation_.text_map(),
      UnorderedElementsAre(Pair(cc::HashSkottieResourceId(attribution_node_0_),
                                HasText(cc::kLottieDataWith2TextNode1Text)),
                           Pair(cc::HashSkottieResourceId(attribution_node_1_),
                                HasText("attribution_text_1"))));
}

TEST_F(AmbientAnimationAttributionProviderTest2DynamicAssets1Attribution,
       HandlesFewerAttributionNodesThanAssets) {
  RefreshDynamicImageAssets("attribution_text_0", "attribution_text_1");
  // The static text node should the value that's baked into the lottie file
  // (|kLottieDataWith2TextNode1Text|).
  //
  // The one attribution node should be assigned the very first asset's text,
  // which is "attribution_text_0" in this case. The second asset's text
  // ("attribution_text_1") should be unused because there are fewer text
  // nodes than assets here.
  EXPECT_THAT(
      animation_.text_map(),
      UnorderedElementsAre(Pair(cc::HashSkottieResourceId(attribution_node_0_),
                                HasText(cc::kLottieDataWith2TextNode1Text)),
                           Pair(cc::HashSkottieResourceId(attribution_node_1_),
                                HasText("attribution_text_0"))));
}

TEST_F(AmbientAnimationAttributionProviderTest2DynamicAssets,
       DoesNotSetTextForPersonalPhotos) {
  RefreshDynamicImageAssets("attribution_text_0_a", "attribution_text_1_a");
  RefreshDynamicImageAssets("attribution_text_0_b", "attribution_text_1_b",
                            ::ambient::kPersonal);
  EXPECT_THAT(
      animation_.text_map(),
      UnorderedElementsAre(
          Pair(cc::HashSkottieResourceId(attribution_node_0_), HasText("")),
          Pair(cc::HashSkottieResourceId(attribution_node_1_), HasText(""))));
}

}  // namespace
}  // namespace ash
