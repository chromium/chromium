// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_ONLINE_VARIANT_UTILS_H_
#define ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_ONLINE_VARIANT_UTILS_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"

namespace ash {

// Checks if the given |variant| is suitable for the current system's
// checkpoint.
ASH_EXPORT bool IsSuitableOnlineWallpaperVariant(
    const OnlineWallpaperVariant& variant,
    ScheduleCheckpoint checkpoint);

// Returns a pointer to the first matching variant in |variants| if one
// exists.
ASH_EXPORT const OnlineWallpaperVariant* FirstValidVariant(
    const std::vector<OnlineWallpaperVariant>& variants,
    ScheduleCheckpoint checkpoint);

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_ONLINE_VARIANT_UTILS_H_
