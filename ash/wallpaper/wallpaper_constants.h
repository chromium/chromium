// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_CONSTANTS_H_
#define ASH_WALLPAPER_WALLPAPER_CONSTANTS_H_

#include "ash/public/cpp/style/color_provider.h"

namespace ash {

namespace wallpaper_constants {

// Blur sigma used for normal wallpaper.
constexpr float kClear = 0.f;
// Blur sigma used in overview mode.
constexpr float kOverviewBlur = ColorProvider::kBackgroundBlurSigma;
// Blur sigma in lock/login screen.
constexpr float kLockLoginBlur = 30.0f;
// Blur sigma used in oobe.
constexpr float kOobeBlur = ColorProvider::kBackgroundBlurSigma;

// File path suffix of resized small wallpapers.
constexpr char kSmallWallpaperSuffix[] = "_small";

// The ID of the time of day wallpaper collection served by backdrop server.
constexpr char kTimeOfDayWallpaperCollectionId[] =
    "_time_of_day_chromebook_collection";
// The ID of the default time of day wallpaper.
constexpr uint64_t kDefaultTimeOfDayWallpaperUnitId = 18;

}  // namespace wallpaper_constants

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_CONSTANTS_H_
