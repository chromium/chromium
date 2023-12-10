// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_online_variant_utils.h"

#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/wallpaper/wallpaper_constants.h"
#include "base/ranges/algorithm.h"

namespace ash {

namespace {

// Not all wallpapers have variants that map 1:1 to the checkpoints. D/L
// wallpapers are an example. In order to gracefully support these wallpapers,
// this method accepts a boolean |match_subtype| so that if true, variants with
// |backdrop::Image_ImageType_IMAGE_TYPE_LIGHT_MODE| is also considered valid
// for |ScheduleCheckpoint::kMorning| and |ScheduleCheckpoint::kLateAfternoon|.
bool IsSuitableOnlineWallpaperVariantInternal(
    const OnlineWallpaperVariant& variant,
    ScheduleCheckpoint checkpoint,
    bool match_subtype) {
  if (variant.type == backdrop::Image_ImageType_IMAGE_TYPE_UNKNOWN) {
    return true;
  }
  switch (checkpoint) {
    case ScheduleCheckpoint::kSunrise:
      return variant.type == backdrop::Image_ImageType_IMAGE_TYPE_LIGHT_MODE;
    // `kDisabled` is equivalent to Light mode.
    case ScheduleCheckpoint::kDisabled:
    case ScheduleCheckpoint::kMorning:
      return variant.type ==
                 backdrop::Image_ImageType_IMAGE_TYPE_MORNING_MODE ||
             (match_subtype &&
              variant.type == backdrop::Image_ImageType_IMAGE_TYPE_LIGHT_MODE);
    case ScheduleCheckpoint::kLateAfternoon:
      return variant.type ==
                 backdrop::Image_ImageType_IMAGE_TYPE_LATE_AFTERNOON_MODE ||
             (match_subtype &&
              variant.type == backdrop::Image_ImageType_IMAGE_TYPE_LIGHT_MODE);
    case ScheduleCheckpoint::kSunset:
    // `kEnabled` is equivalent to Dark mode.
    case ScheduleCheckpoint::kEnabled:
      return variant.type == backdrop::Image_ImageType_IMAGE_TYPE_DARK_MODE;
  }
}

}  // namespace

bool IsSuitableOnlineWallpaperVariant(const OnlineWallpaperVariant& variant,
                                      ScheduleCheckpoint checkpoint) {
  return IsSuitableOnlineWallpaperVariantInternal(variant, checkpoint,
                                                  /*match_subtype=*/true);
}

const OnlineWallpaperVariant* FirstValidVariant(
    const std::vector<OnlineWallpaperVariant>& variants,
    ScheduleCheckpoint checkpoint) {
  // Attempt to find the exact 1:1 match for |variant| and |checkpoint|.
  auto iter =
      base::ranges::find_if(variants, [checkpoint](const auto& variant) {
        return IsSuitableOnlineWallpaperVariantInternal(
            variant, checkpoint,
            /*match_subtype=*/false);
      });
  if (iter != variants.end()) {
    return &(*iter);
  }
  // Attempt to find a subtype |variant| for |checkpoint|.
  iter = base::ranges::find_if(variants, [checkpoint](const auto& variant) {
    return IsSuitableOnlineWallpaperVariantInternal(variant, checkpoint,
                                                    /*match_subtype=*/true);
  });
  if (iter != variants.end()) {
    return &(*iter);
  }
  return nullptr;
}

bool IsTimeOfDayWallpaper(const std::string& collection_id) {
  return collection_id == wallpaper_constants::kTimeOfDayWallpaperCollectionId;
}

}  // namespace ash
