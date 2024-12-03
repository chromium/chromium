// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/online_wallpaper_variant_info_fetcher.h"

#include "ash/public/cpp/schedule_enums.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/wallpaper/test_wallpaper_controller_client.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "ash/wallpaper/wallpaper_metrics_manager.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/account_id/account_id.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;

constexpr char kUser1[] = "user1@test.com";
const AccountId kAccount1 = AccountId::FromUserEmailGaiaId(kUser1, kUser1);
constexpr char kDummyCollectionId[] = "testCollectionId";

// Returns a set of images with the given |type|.
std::vector<backdrop::Image> ImageSet(backdrop::Image_ImageType type,
                                      size_t size) {
  std::vector<backdrop::Image> images;
  for (size_t i = 0; i < size; i++) {
    images.emplace_back();
    images.back().set_image_url(
        base::StringPrintf("https://test_wallpaper/%zu", i));
    // A "unique" asset id.
    images.back().set_asset_id(42 + i);
    // Images should all have a different unit id except in a light/dark pair.
    images.back().set_unit_id(13 + i);
    images.back().set_image_type(type);
  }
  return images;
}

// Returns the time of day wallpapers in order of light, morning, late
// afternoon, and dark.
std::vector<backdrop::Image> TimeOfDayImageSet() {
  const uint64_t kUnitId = 439;
  const std::vector<backdrop::Image_ImageType> image_types = {
      backdrop::Image::IMAGE_TYPE_LIGHT_MODE,
      backdrop::Image::IMAGE_TYPE_MORNING_MODE,
      backdrop::Image::IMAGE_TYPE_LATE_AFTERNOON_MODE,
      backdrop::Image::IMAGE_TYPE_DARK_MODE};

  std::vector<backdrop::Image> images;
  for (size_t i = 0; i < image_types.size(); ++i) {
    const uint64_t asset_id = i + 99;
    const std::string url =
        base::StringPrintf("https://preferred_wallpaper/images/%zu", asset_id);
    backdrop::Image image;
    image.set_asset_id(asset_id);
    image.set_unit_id(kUnitId);
    image.set_image_type(image_types[i]);
    image.set_image_url(url);
    images.push_back(image);
  }
  return images;
}

std::vector<OnlineWallpaperVariant> ConvertToVariants(
    const std::vector<backdrop::Image>& image_set) {
  std::vector<OnlineWallpaperVariant> variants;
  for (const backdrop::Image& image : image_set) {
    variants.emplace_back(image.asset_id(), GURL(image.image_url()),
                          image.image_type());
  }
  return variants;
}

class OnlineWallpaperVariantInfoFetcherTest : public testing::Test {
 public:
  OnlineWallpaperVariantInfoFetcherTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI) {}

  void SetUp() override {
    wallpaper_fetcher_ = std::make_unique<OnlineWallpaperVariantInfoFetcher>();
    wallpaper_fetcher_->SetClient(&client_);
  }

  void TearDown() override {}

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::HistogramTester histogram_tester_;

  TestWallpaperControllerClient client_;
  std::unique_ptr<OnlineWallpaperVariantInfoFetcher> wallpaper_fetcher_;
};

// Verify that variants in params is populated.
TEST_F(OnlineWallpaperVariantInfoFetcherTest,
       FetchDailyWallpaper_VariantsPopulated) {
  base::test::TestFuture<std::optional<OnlineWallpaperParams>> test_future;
  WallpaperInfo info("", WallpaperLayout::WALLPAPER_LAYOUT_CENTER,
                     WallpaperType::kDaily, base::Time::Now());
  info.collection_id = kDummyCollectionId;

  wallpaper_fetcher_->FetchDailyWallpaper(kAccount1, info,
                                          test_future.GetCallback());

  ASSERT_TRUE(test_future.Wait()) << "Fetch Daily never ran callback";
  auto result = test_future.Get();
  ASSERT_TRUE(result);
  EXPECT_FALSE(result->variants.empty());
}

// Verify that repeated requests for daily wallpaper changes the unit_id.
TEST_F(OnlineWallpaperVariantInfoFetcherTest,
       FetchDailyWallpaper_EveryRequestDifferent) {
  // Add some images for a new collection id.
  const std::string kCollectionId = "FetchDaily";
  client_.AddCollection(kCollectionId,
                        ImageSet(backdrop::Image::IMAGE_TYPE_UNKNOWN, 6u));

  // First fetch
  base::test::TestFuture<std::optional<OnlineWallpaperParams>> test_future;
  WallpaperInfo info("", WallpaperLayout::WALLPAPER_LAYOUT_CENTER,
                     WallpaperType::kDaily, base::Time::Now());
  info.collection_id = kCollectionId;

  wallpaper_fetcher_->FetchDailyWallpaper(kAccount1, info,
                                          test_future.GetCallback());
  auto first_result = test_future.Get();
  EXPECT_TRUE(first_result);

  // Calling FetchDaily with the same arguments should yield a different params
  // object if there is more than one wallpaper in the collection.
  base::test::TestFuture<std::optional<OnlineWallpaperParams>> test_future2;
  wallpaper_fetcher_->FetchDailyWallpaper(kAccount1, info,
                                          test_future2.GetCallback());

  auto second_result = test_future2.Get();
  EXPECT_TRUE(second_result);

  EXPECT_NE(first_result->unit_id, second_result->unit_id)
      << "unit_ids for the two calls should be different";
}

// Verify that variants with matching unit id are selected and the asset of the
// appropriate type (dark/light).
TEST_F(OnlineWallpaperVariantInfoFetcherTest,
       FetchOnlineWallpaper_DarkLightVariants) {
  // Add some images for a new collection id.
  const std::string kCollectionId = "FetchOnline";
  const uint64_t kLightAssetId = 99;
  const std::string kLightUrl = "https://preferred_wallpaper/images/99";
  const uint64_t kDarkAssetId = 103;
  const std::string kDarkUrl = "https://preferred_wallpaper/images/103";
  const uint64_t kUnitId = 432;

  // Initially populate the collection with images we won't use.
  std::vector<backdrop::Image> images =
      ImageSet(backdrop::Image::IMAGE_TYPE_UNKNOWN, 6u);

  // Push a dark and light asset that share a unit id.
  backdrop::Image light_image;
  light_image.set_asset_id(kLightAssetId);
  light_image.set_unit_id(kUnitId);
  light_image.set_image_type(backdrop::Image::IMAGE_TYPE_LIGHT_MODE);
  light_image.set_image_url(kLightUrl);
  images.push_back(light_image);

  backdrop::Image dark_image;
  dark_image.set_asset_id(kDarkAssetId);
  dark_image.set_unit_id(kUnitId);
  dark_image.set_image_type(backdrop::Image::IMAGE_TYPE_DARK_MODE);
  dark_image.set_image_url(kDarkUrl);
  images.push_back(dark_image);

  client_.AddCollection(kCollectionId, images);

  WallpaperInfo info(kLightUrl, WallpaperLayout::WALLPAPER_LAYOUT_CENTER,
                     WallpaperType::kOnline, base::Time::Now());
  info.collection_id = kCollectionId;

  base::test::TestFuture<std::optional<OnlineWallpaperParams>> test_future;
  wallpaper_fetcher_->FetchOnlineWallpaper(kAccount1, info,
                                           test_future.GetCallback());
  auto result = test_future.Get();
  ASSERT_TRUE(result);
  EXPECT_THAT(
      result->variants,
      UnorderedElementsAre(
          OnlineWallpaperVariant(kLightAssetId, GURL(kLightUrl),
                                 backdrop::Image::IMAGE_TYPE_LIGHT_MODE),
          OnlineWallpaperVariant(kDarkAssetId, GURL(kDarkUrl),
                                 backdrop::Image::IMAGE_TYPE_DARK_MODE)));
}

// Verify that time of day variants with matching unit id are matched with the
// right checkpoints.
TEST_F(OnlineWallpaperVariantInfoFetcherTest, FetchTimeOfDayWallpaper) {
  auto images = TimeOfDayImageSet();
  client_.AddCollection(wallpaper_constants::kTimeOfDayWallpaperCollectionId,
                        images);
  base::test::TestFuture<std::optional<OnlineWallpaperParams>> test_future;
  wallpaper_fetcher_->FetchTimeOfDayWallpaper(kAccount1, images[0].unit_id(),
                                              test_future.GetCallback());
  auto result = test_future.Get();
  ASSERT_TRUE(result);
  EXPECT_THAT(result->variants,
              UnorderedElementsAreArray(ConvertToVariants(images)));
}

// Verify requests for fetching time of day wallpapers fail with invalid unit
// id.
TEST_F(OnlineWallpaperVariantInfoFetcherTest,
       FetchTimeOfDayWallpaper_InvalidUnitId) {
  auto images = TimeOfDayImageSet();
  client_.AddCollection(wallpaper_constants::kTimeOfDayWallpaperCollectionId,
                        images);

  base::test::TestFuture<std::optional<OnlineWallpaperParams>> test_future;
  // Verifies that checkpoint and the variant matches.
  wallpaper_fetcher_->FetchTimeOfDayWallpaper(kAccount1, 123,
                                              test_future.GetCallback());
  auto result = test_future.Get();
  EXPECT_FALSE(result);
}

// When variants are already populated, params are returned.
TEST_F(OnlineWallpaperVariantInfoFetcherTest, FetchOnlineWallpaper_FromInfo) {
  const uint64_t kAssetId = 14;
  const GURL kUrl("https://populated_url/14");
  const std::string kCollectionId = "PrePopulatedCollection";
  const uint64_t kUnitId = 31;
  const std::vector<OnlineWallpaperVariant> kVariants = {OnlineWallpaperVariant(
      kAssetId, kUrl, backdrop::Image::IMAGE_TYPE_UNKNOWN)};
  OnlineWallpaperParams params(
      kAccount1, kCollectionId, WallpaperLayout::WALLPAPER_LAYOUT_CENTER,
      /*preview_mode=*/false, /*from_user=*/true,
      /*daily_refresh_enabled=*/false, kUnitId, kVariants);
  WallpaperInfo info(params, kVariants.front());

  base::test::TestFuture<std::optional<OnlineWallpaperParams>> test_future;
  wallpaper_fetcher_->FetchOnlineWallpaper(kAccount1, info,
                                           test_future.GetCallback());

  // Callback is called
  auto result = test_future.Get();
  EXPECT_TRUE(result);

  // WallpaperControllerClient is not used if OnlineWallpaperParams is fully
  // populated.
  EXPECT_EQ(0u, client_.fetch_images_for_collection_count());
}

TEST_F(OnlineWallpaperVariantInfoFetcherTest,
       FetchOnlineWallpaper_FromInfo_NoLocation) {
  WallpaperInfo info = WallpaperInfo(/*in_location=*/"",
                                     WallpaperLayout::WALLPAPER_LAYOUT_CENTER,
                                     WallpaperType::kOnline, base::Time::Now());

  base::test::TestFuture<std::optional<OnlineWallpaperParams>> test_future;
  wallpaper_fetcher_->FetchOnlineWallpaper(kAccount1, info,
                                           test_future.GetCallback());

  // Callback is called
  auto result = test_future.Get();
  EXPECT_FALSE(result);

  histogram_tester_.ExpectBucketCount("Ash.Wallpaper.Online.Result2",
                                      SetWallpaperResult::kInvalidState, 1);
}

}  // namespace
}  // namespace ash
