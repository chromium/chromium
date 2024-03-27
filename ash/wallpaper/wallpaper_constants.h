// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_CONSTANTS_H_
#define ASH_WALLPAPER_WALLPAPER_CONSTANTS_H_

#include "ash/public/cpp/style/color_provider.h"

namespace ash::wallpaper_constants {

// Blur sigma used for normal wallpaper.
inline constexpr float kClear = 0.f;
// Blur sigma in lock/login screen.
inline constexpr float kLockLoginBlur = 30.0f;
// Blur sigma used in oobe.
inline constexpr float kOobeBlur = ColorProvider::kBackgroundBlurSigma;

// File path suffix of resized small wallpapers.
inline constexpr char kSmallWallpaperSuffix[] = "_small";

// The ID of the time of day wallpaper collection served by backdrop server.
inline constexpr char kTimeOfDayWallpaperCollectionId[] =
    "_time_of_day_chromebook_collection";
// The ID of the default time of day wallpaper.
inline constexpr uint64_t kDefaultTimeOfDayWallpaperUnitId = 18;

// The subdirectory name for storing SeaPen wallpaper. There is a SeaPen
// subdirectory in the global wallpaper directory, and in
// <user_profile_directory>/wallpaper/sea_pen.
inline constexpr char kSeaPenWallpaperDirName[] = "sea_pen";

}  // namespace ash::wallpaper_constants

#endif  // ASH_WALLPAPER_WALLPAPER_CONSTANTS_H_
