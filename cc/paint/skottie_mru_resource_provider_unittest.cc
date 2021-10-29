// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skottie_mru_resource_provider.h"

#include "base/files/file_path.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/skottie_resource_metadata.h"
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
using ::testing::Key;
using ::testing::Pair;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

class SkottieMRUResourceProviderTest : public ::testing::Test {
 protected:
  SkottieMRUResourceProviderTest()
      : provider_(sk_make_sp<SkottieMRUResourceProvider>()),
        provider_base_(provider_.get()) {}

  const sk_sp<SkottieMRUResourceProvider> provider_;
  skresources::ResourceProvider* const provider_base_;
};

TEST_F(SkottieMRUResourceProviderTest, ProvidesMostRecentFrameDataForAsset) {
  sk_sp<skresources::ImageAsset> asset = provider_base_->loadImageAsset(
      "test-resource-path", "test-resource-name", "test-resource-id");
  PaintImage image_1 = CreateBitmapImage(gfx::Size(10, 10));
  SkottieMRUResourceProvider::ImageAssetMap assets =
      provider_->GetImageAssetMap();
  ASSERT_THAT(assets, Contains(Key(HashSkottieResourceId("test-resource-id"))));
  assets[HashSkottieResourceId("test-resource-id")]->SetCurrentFrameData(
      {.image = image_1.GetSwSkImage()});
  EXPECT_THAT(asset->getFrameData(/*t=*/0).image, Eq(image_1.GetSwSkImage()));
  // The same image should be re-used for the next timestamp.
  EXPECT_THAT(asset->getFrameData(/*t=*/0.1).image, Eq(image_1.GetSwSkImage()));
  // Now the new image should be used.
  PaintImage image_2 = CreateBitmapImage(gfx::Size(20, 20));
  assets[HashSkottieResourceId("test-resource-id")]->SetCurrentFrameData(
      {.image = image_2.GetSwSkImage()});
  EXPECT_THAT(asset->getFrameData(/*t=*/0.2).image, Eq(image_2.GetSwSkImage()));
}

TEST_F(SkottieMRUResourceProviderTest, ProvidesFrameDataForMultipleAssets) {
  sk_sp<skresources::ImageAsset> asset_1 = provider_base_->loadImageAsset(
      "test-resource-path", "test-resource-name", "test-resource-id-1");
  sk_sp<skresources::ImageAsset> asset_2 = provider_base_->loadImageAsset(
      "test-resource-path", "test-resource-name", "test-resource-id-2");
  PaintImage image_1 = CreateBitmapImage(gfx::Size(10, 10));
  SkottieMRUResourceProvider::ImageAssetMap assets =
      provider_->GetImageAssetMap();
  ASSERT_THAT(assets, SizeIs(2));
  ASSERT_THAT(assets,
              Contains(Key(HashSkottieResourceId("test-resource-id-1"))));
  assets[HashSkottieResourceId("test-resource-id-1")]->SetCurrentFrameData(
      {.image = image_1.GetSwSkImage()});
  PaintImage image_2 = CreateBitmapImage(gfx::Size(20, 20));
  ASSERT_THAT(assets,
              Contains(Key(HashSkottieResourceId("test-resource-id-2"))));
  assets[HashSkottieResourceId("test-resource-id-2")]->SetCurrentFrameData(
      {.image = image_2.GetSwSkImage()});
  EXPECT_THAT(asset_1->getFrameData(/*t=*/0).image, Eq(image_1.GetSwSkImage()));
  EXPECT_THAT(asset_2->getFrameData(/*t=*/0).image, Eq(image_2.GetSwSkImage()));
}

TEST_F(SkottieMRUResourceProviderTest, ReturnsCorrectImageAssetMetadata) {
  sk_sp<skresources::ImageAsset> asset_1 = provider_base_->loadImageAsset(
      "test-resource-path-1", "test-resource-name-1", "test-resource-id-1");
  sk_sp<skresources::ImageAsset> asset_2 = provider_base_->loadImageAsset(
      "test-resource-path-2", "test-resource-name-2", "test-resource-id-2");
  EXPECT_THAT(
      provider_->GetImageAssetMetadata().asset_storage(),
      UnorderedElementsAre(
          Pair("test-resource-id-1",
               base::FilePath(FILE_PATH_LITERAL(
                                  "test-resource-path-1/test-resource-name-1"))
                   .NormalizePathSeparators()),
          Pair("test-resource-id-2",
               base::FilePath(FILE_PATH_LITERAL(
                                  "test-resource-path-2/test-resource-name-2"))
                   .NormalizePathSeparators())));
}

}  // namespace
}  // namespace cc
