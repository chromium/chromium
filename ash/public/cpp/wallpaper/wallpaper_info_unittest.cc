// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/wallpaper/wallpaper_info.h"

#include "ash/public/cpp/wallpaper/google_photos_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_params.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "base/time/time.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr char kUser1[] = "user1@test.com";
const AccountId kAccountId1 = AccountId::FromUserEmailGaiaId(kUser1, kUser1);

const uint64_t kAssetId = 1;
const uint64_t kUnitId = 1;

using WallpaperInfoTest = ::testing::Test;

TEST_F(WallpaperInfoTest, FromDictReturnsNullOptForInvalidValues) {
  {
    // Invalid type.
    WallpaperInfo actual_info =
        WallpaperInfo(std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
                      WallpaperType::kCount, base::Time::Now(), std::string());
    base::Value::Dict dict = actual_info.ToDict();
    EXPECT_FALSE(WallpaperInfo::FromDict(dict));
  }
  {
    // Invalid layout.
    WallpaperInfo actual_info =
        WallpaperInfo(std::string(), NUM_WALLPAPER_LAYOUT,
                      WallpaperType::kOnline, base::Time::Now(), std::string());
    base::Value::Dict dict = actual_info.ToDict();
    EXPECT_FALSE(WallpaperInfo::FromDict(dict));
  }
}

TEST_F(WallpaperInfoTest, ToAndFromDict) {
  {
    // WallpaperType::kOnline
    OnlineWallpaperParams params = OnlineWallpaperParams(
        kAccountId1,
        /*collection_id=*/std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
        /*preview_mode=*/false, /*from_user=*/false,
        /*daily_refresh_enabled=*/false, kUnitId,
        /*variants=*/
        {{kAssetId, GURL("https://example.com/image.png"),
          backdrop::Image::IMAGE_TYPE_UNKNOWN}});
    WallpaperInfo actual_info = WallpaperInfo(params, params.variants[0]);
    base::Value::Dict dict = actual_info.ToDict();
    std::optional<WallpaperInfo> expected_info = WallpaperInfo::FromDict(dict);
    EXPECT_TRUE(actual_info.MatchesAsset(expected_info.value()));
  }
  {
    // WallpaperType::kOnceGooglePhotos
    GooglePhotosWallpaperParams params = GooglePhotosWallpaperParams(
        kAccountId1, "id", /*daily_refresh_enabled=*/false,
        WALLPAPER_LAYOUT_CENTER_CROPPED, /*preview_mode=*/false, "dedup_key");
    WallpaperInfo actual_info = WallpaperInfo(params);
    base::Value::Dict dict = actual_info.ToDict();
    std::optional<WallpaperInfo> expected_info = WallpaperInfo::FromDict(dict);
    EXPECT_TRUE(actual_info.MatchesAsset(expected_info.value()));
  }
  {
    // WallpaperType::kCustomized
    WallpaperInfo actual_info = WallpaperInfo(
        std::string(), WALLPAPER_LAYOUT_CENTER_CROPPED,
        WallpaperType::kCustomized, base::Time::Now(), std::string());
    base::Value::Dict dict = actual_info.ToDict();
    std::optional<WallpaperInfo> expected_info = WallpaperInfo::FromDict(dict);
    EXPECT_TRUE(actual_info.MatchesAsset(expected_info.value()));
  }
}

}  // namespace
}  // namespace ash
