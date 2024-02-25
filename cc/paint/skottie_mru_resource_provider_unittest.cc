// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_mru_resource_provider.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/skottie_resource_metadata.h"
#include "cc/test/lottie_test_data.h"
#include "cc/test/skia_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/modules/skresources/include/SkResources.h"
#include "ui/gfx/geometry/size.h"

namespace cc {
namespace {

using ::testing::Contains;
using ::testing::Eq;
using ::testing::FieldsAre;
using ::testing::Key;
using ::testing::Optional;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

class FrameDataStub {
 public:
  using FrameData = skresources::ImageAsset::FrameData;

  void SetAssetFrameData(std::string_view asset_id,
                         FrameData current_frame_data) {
    asset_to_frame_data_[HashSkottieResourceId(asset_id)] =
        std::move(current_frame_data);
    asset_to_result_[HashSkottieResourceId(asset_id)] =
        SkottieWrapper::FrameDataFetchResult::kNewDataAvailable;
  }

  void SetAssetResult(std::string_view asset_id,
                      SkottieWrapper::FrameDataFetchResult current_result) {
    asset_to_result_[HashSkottieResourceId(asset_id)] = current_result;
  }

  SkottieWrapper::FrameDataFetchResult GetFrameDataForAsset(
      SkottieResourceIdHash asset_id,
      float t,
      sk_sp<SkImage>& image_out,
      SkSamplingOptions& sampling_out) const {
    if (asset_to_frame_data_.contains(asset_id)) {
      image_out = asset_to_frame_data_.at(asset_id).image;
      sampling_out = asset_to_frame_data_.at(asset_id).sampling;
    }
    return asset_to_result_.contains(asset_id)
               ? asset_to_result_.at(asset_id)
               : SkottieWrapper::FrameDataFetchResult::kNoUpdate;
  }

 private:
  base::flat_map<SkottieResourceIdHash, FrameData> asset_to_frame_data_;
  base::flat_map<SkottieResourceIdHash, SkottieWrapper::FrameDataFetchResult>
      asset_to_result_;
};

class SkottieMRUResourceProviderTest : public ::testing::Test {
 protected:
  void Init(std::string_view animation_json) {
    provider_ = sk_make_sp<SkottieMRUResourceProvider>(
        base::BindRepeating(&FrameDataStub::GetFrameDataForAsset,
                            base::Unretained(&frame_data_stub_)),
        animation_json);
    provider_base_ = provider_.get();
  }

  FrameDataStub frame_data_stub_;
  sk_sp<SkottieMRUResourceProvider> provider_;
  raw_ptr<skresources::ResourceProvider, DanglingUntriaged> provider_base_;
};

TEST_F(SkottieMRUResourceProviderTest, ProvidesMostRecentFrameDataForAsset) {
  Init(R"({
      "assets": [
        {
          "id": "test-resource-id"
        }
      ]
    })");
  sk_sp<skresources::ImageAsset> asset = provider_base_->loadImageAsset(
      "test-resource-path", "test-resource-name", "test-resource-id");
  PaintImage image_1 = CreateBitmapImage(gfx::Size(10, 10));
  frame_data_stub_.SetAssetFrameData("test-resource-id",
                                     {.image = image_1.GetSwSkImage()});
  EXPECT_THAT(asset->getFrameData(/*t=*/0).image, Eq(image_1.GetSwSkImage()));
  // The same image should be re-used for the next timestamp.
  frame_data_stub_.SetAssetResult(
      "test-resource-id", SkottieWrapper::FrameDataFetchResult::kNoUpdate);
  EXPECT_THAT(asset->getFrameData(/*t=*/0.1).image, Eq(image_1.GetSwSkImage()));
  // Now the new image should be used.
  PaintImage image_2 = CreateBitmapImage(gfx::Size(20, 20));
  frame_data_stub_.SetAssetFrameData("test-resource-id",
                                     {.image = image_2.GetSwSkImage()});
  EXPECT_THAT(asset->getFrameData(/*t=*/0.2).image, Eq(image_2.GetSwSkImage()));
}

TEST_F(SkottieMRUResourceProviderTest, ProvidesFrameDataForMultipleAssets) {
  Init(R"({
      "assets": [
        {
          "id": "test-resource-id-1"
        },
        {
          "id": "test-resource-id-2"
        }
      ]
    })");
  sk_sp<skresources::ImageAsset> asset_1 = provider_base_->loadImageAsset(
      "test-resource-path", "test-resource-name", "test-resource-id-1");
  sk_sp<skresources::ImageAsset> asset_2 = provider_base_->loadImageAsset(
      "test-resource-path", "test-resource-name", "test-resource-id-2");
  PaintImage image_1 = CreateBitmapImage(gfx::Size(10, 10));
  frame_data_stub_.SetAssetFrameData("test-resource-id-1",
                                     {.image = image_1.GetSwSkImage()});
  PaintImage image_2 = CreateBitmapImage(gfx::Size(20, 20));
  frame_data_stub_.SetAssetFrameData("test-resource-id-2",
                                     {.image = image_2.GetSwSkImage()});
  EXPECT_THAT(asset_1->getFrameData(/*t=*/0).image, Eq(image_1.GetSwSkImage()));
  EXPECT_THAT(asset_2->getFrameData(/*t=*/0).image, Eq(image_2.GetSwSkImage()));
}

TEST_F(SkottieMRUResourceProviderTest, ReturnsCorrectImageAssetMetadata) {
  Init(R"({
      "assets": [
        {
          "id": "test-resource-id-1",
          "w": 100,
          "h": 101
        },
        {
          "id": "test-resource-id-2",
          "w": 200,
          "h": 201
        }
      ]
    })");
  sk_sp<skresources::ImageAsset> asset_1 = provider_base_->loadImageAsset(
      "test-resource-path-1", "test-resource-name-1", "test-resource-id-1");
  sk_sp<skresources::ImageAsset> asset_2 = provider_base_->loadImageAsset(
      "test-resource-path-2", "test-resource-name-2", "test-resource-id-2");
  EXPECT_THAT(
      provider_->GetImageAssetMetadata().asset_storage(),
      UnorderedElementsAre(
          Pair("test-resource-id-1",
               FieldsAre(base::FilePath(
                             FILE_PATH_LITERAL(
                                 "test-resource-path-1/test-resource-name-1"))
                             .NormalizePathSeparators(),
                         Optional(gfx::Size(100, 101)))),
          Pair("test-resource-id-2",
               FieldsAre(base::FilePath(
                             FILE_PATH_LITERAL(
                                 "test-resource-path-2/test-resource-name-2"))
                             .NormalizePathSeparators(),
                         Optional(gfx::Size(200, 201))))));
}

TEST_F(SkottieMRUResourceProviderTest, HandlesMissingAssetDimensions) {
  Init(R"({
      "assets": [
        {
          "id": "test-resource-id"
        }
      ]
    })");
  sk_sp<skresources::ImageAsset> asset_1 = provider_base_->loadImageAsset(
      "test-resource-path", "test-resource-name", "test-resource-id");
  EXPECT_THAT(
      provider_->GetImageAssetMetadata().asset_storage(),
      UnorderedElementsAre(Pair(
          "test-resource-id",
          FieldsAre(base::FilePath(FILE_PATH_LITERAL(
                                       "test-resource-path/test-resource-name"))
                        .NormalizePathSeparators(),
                    Eq(std::nullopt)))));
}

TEST_F(SkottieMRUResourceProviderTest, HandlesIncompleteDimensions) {
  Init(R"({
      "assets": [
        {
          "id": "test-resource-id",
          "w": 100
        }
      ]
    })");
  sk_sp<skresources::ImageAsset> asset_1 = provider_base_->loadImageAsset(
      "test-resource-path", "test-resource-name", "test-resource-id");
  EXPECT_THAT(
      provider_->GetImageAssetMetadata().asset_storage(),
      UnorderedElementsAre(Pair(
          "test-resource-id",
          FieldsAre(base::FilePath(FILE_PATH_LITERAL(
                                       "test-resource-path/test-resource-name"))
                        .NormalizePathSeparators(),
                    Eq(std::nullopt)))));

  Init(R"({
      "assets": [
        {
          "id": "test-resource-id",
          "h": 100
        }
      ]
    })");
  asset_1 = provider_base_->loadImageAsset(
      "test-resource-path", "test-resource-name", "test-resource-id");
  EXPECT_THAT(
      provider_->GetImageAssetMetadata().asset_storage(),
      UnorderedElementsAre(Pair(
          "test-resource-id",
          FieldsAre(base::FilePath(FILE_PATH_LITERAL(
                                       "test-resource-path/test-resource-name"))
                        .NormalizePathSeparators(),
                    Eq(std::nullopt)))));
}

TEST_F(SkottieMRUResourceProviderTest, HandlesInvalidDimensions) {
  Init(R"({
      "assets": [
        {
          "id": "test-resource-id",
          "w": -100
        }
      ]
    })");
  sk_sp<skresources::ImageAsset> asset_1 = provider_base_->loadImageAsset(
      "test-resource-path", "test-resource-name", "test-resource-id");
  EXPECT_THAT(
      provider_->GetImageAssetMetadata().asset_storage(),
      UnorderedElementsAre(Pair(
          "test-resource-id",
          FieldsAre(base::FilePath(FILE_PATH_LITERAL(
                                       "test-resource-path/test-resource-name"))
                        .NormalizePathSeparators(),
                    Eq(std::nullopt)))));

  Init(R"({
      "assets": [
        {
          "id": "test-resource-id",
          "h": -100
        }
      ]
    })");
  asset_1 = provider_base_->loadImageAsset(
      "test-resource-path", "test-resource-name", "test-resource-id");
  EXPECT_THAT(
      provider_->GetImageAssetMetadata().asset_storage(),
      UnorderedElementsAre(Pair(
          "test-resource-id",
          FieldsAre(base::FilePath(FILE_PATH_LITERAL(
                                       "test-resource-path/test-resource-name"))
                        .NormalizePathSeparators(),
                    Eq(std::nullopt)))));
}

TEST_F(SkottieMRUResourceProviderTest, GracefullyHandlesInvalidJson) {
  // No expectations needed. Just make sure the code doesn't crash.
  Init("invalid-json");
  // Lottie animation json is expected to be a dictionary.
  Init(R"(["valid", "json", "list"])");
  // Assets are expected to be a list.
  Init(R"({"assets": "invalid-asset-set"})");
  // Each asset is expected to be a dictionary.
  Init(R"({
      "assets": [
        "invalid-asset-value"
      ]
    })");
}

}  // namespace
}  // namespace cc
