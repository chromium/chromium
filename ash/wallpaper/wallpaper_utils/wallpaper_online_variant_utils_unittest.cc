// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_online_variant_utils.h"

#include <vector>

#include "ash/public/cpp/schedule_enums.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

TEST(WallpaperOnlineVariantUtilsTest, FirstValidVariant) {
  const OnlineWallpaperVariant dark_variant =
      OnlineWallpaperVariant(1, GURL("http://example.com/1"),
                             backdrop::Image_ImageType_IMAGE_TYPE_DARK_MODE);
  const OnlineWallpaperVariant light_variant =
      OnlineWallpaperVariant(2, GURL("http://example.com/2"),
                             backdrop::Image_ImageType_IMAGE_TYPE_LIGHT_MODE);
  const OnlineWallpaperVariant morning_variant =
      OnlineWallpaperVariant(3, GURL("http://example.com/3"),
                             backdrop::Image_ImageType_IMAGE_TYPE_MORNING_MODE);
  const OnlineWallpaperVariant late_afternoon_variant = OnlineWallpaperVariant(
      4, GURL("http://example.com/4"),
      backdrop::Image_ImageType_IMAGE_TYPE_LATE_AFTERNOON_MODE);

  std::vector<OnlineWallpaperVariant> variants = {
      light_variant, dark_variant, morning_variant, late_afternoon_variant};
  EXPECT_EQ(morning_variant,
            *FirstValidVariant(variants, ScheduleCheckpoint::kMorning));
  // For time of day wallpapers, morning variant is used for light mode.
  EXPECT_EQ(morning_variant,
            *FirstValidVariant(variants, ScheduleCheckpoint::kDisabled));
  EXPECT_EQ(dark_variant,
            *FirstValidVariant(variants, ScheduleCheckpoint::kEnabled));
}

TEST(WallpaperOnlineVariantUtilsTest, IsSuitableOnlineWallpaperVariant) {
  const std::map<ScheduleCheckpoint, backdrop::Image::ImageType>
      expected_mapping = {
          {ScheduleCheckpoint::kSunrise,
           backdrop::Image_ImageType_IMAGE_TYPE_LIGHT_MODE},
          {ScheduleCheckpoint::kMorning,
           backdrop::Image_ImageType_IMAGE_TYPE_MORNING_MODE},
          {ScheduleCheckpoint::kLateAfternoon,
           backdrop::Image_ImageType_IMAGE_TYPE_LATE_AFTERNOON_MODE},
          {ScheduleCheckpoint::kSunset,
           backdrop::Image_ImageType_IMAGE_TYPE_DARK_MODE}};

  for (const auto& mapping_pair : expected_mapping) {
    const OnlineWallpaperVariant variant = OnlineWallpaperVariant(
        1, GURL("http://example.com"), mapping_pair.second);
    EXPECT_TRUE(IsSuitableOnlineWallpaperVariant(variant, mapping_pair.first));
  }
}

TEST(WallpaperOnlineVariantUtilsTest,
     IsSuitableOnlineWallpaperVariant_PreviewType) {
  const std::map<ScheduleCheckpoint, backdrop::Image::ImageType>
      expected_mapping = {{ScheduleCheckpoint::kSunrise,
                           backdrop::Image_ImageType_IMAGE_TYPE_PREVIEW_MODE},
                          {ScheduleCheckpoint::kMorning,
                           backdrop::Image_ImageType_IMAGE_TYPE_PREVIEW_MODE},
                          {ScheduleCheckpoint::kLateAfternoon,
                           backdrop::Image_ImageType_IMAGE_TYPE_PREVIEW_MODE},
                          {ScheduleCheckpoint::kSunset,
                           backdrop::Image_ImageType_IMAGE_TYPE_PREVIEW_MODE},
                          {ScheduleCheckpoint::kDisabled,
                           backdrop::Image_ImageType_IMAGE_TYPE_PREVIEW_MODE},
                          {ScheduleCheckpoint::kEnabled,
                           backdrop::Image_ImageType_IMAGE_TYPE_PREVIEW_MODE}};

  for (const auto& mapping_pair : expected_mapping) {
    const OnlineWallpaperVariant variant = OnlineWallpaperVariant(
        1, GURL("http://example.com"), mapping_pair.second);
    EXPECT_FALSE(IsSuitableOnlineWallpaperVariant(variant, mapping_pair.first));
  }
}

TEST(WallpaperOnlineVariantUtilsTest,
     IsSuitableOnlineWallpaperVariant_MatchSubType) {
  const std::map<ScheduleCheckpoint, backdrop::Image::ImageType>
      expected_mapping = {{ScheduleCheckpoint::kSunrise,
                           backdrop::Image_ImageType_IMAGE_TYPE_LIGHT_MODE},
                          {ScheduleCheckpoint::kMorning,
                           backdrop::Image_ImageType_IMAGE_TYPE_LIGHT_MODE},
                          {ScheduleCheckpoint::kLateAfternoon,
                           backdrop::Image_ImageType_IMAGE_TYPE_LIGHT_MODE},
                          {ScheduleCheckpoint::kSunset,
                           backdrop::Image_ImageType_IMAGE_TYPE_DARK_MODE},
                          {ScheduleCheckpoint::kDisabled,
                           backdrop::Image_ImageType_IMAGE_TYPE_LIGHT_MODE},
                          {ScheduleCheckpoint::kEnabled,
                           backdrop::Image_ImageType_IMAGE_TYPE_DARK_MODE}};

  for (const auto& mapping_pair : expected_mapping) {
    const OnlineWallpaperVariant variant = OnlineWallpaperVariant(
        1, GURL("http://example.com"), mapping_pair.second);
    EXPECT_TRUE(IsSuitableOnlineWallpaperVariant(variant, mapping_pair.first));
  }
}

}  // namespace

}  // namespace ash
