// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_CONSTANTS_H_
#define ASH_WALLPAPER_WALLPAPER_CONSTANTS_H_

#include "ash/public/cpp/style/color_provider.h"

#include <string_view>

namespace ash::wallpaper_constants {

// Blur sigma used for normal wallpaper.
constexpr float kClear = 0.f;
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

// Keys for fields stored in SeaPen metadata json.
// TODO(b/321275173): move those consts into another file as they are shared
// with VC background.
inline constexpr std::string_view kSeaPenCreationTimeKey = "creation_time";
inline constexpr std::string_view kSeaPenFreeformQueryKey = "freeform_query";
inline constexpr std::string_view kSeaPenTemplateIdKey = "template_id";
inline constexpr std::string_view kSeaPenTemplateOptionsKey = "options";
inline constexpr std::string_view kSeaPenUserVisibleQueryTextKey =
    "user_visible_query_text";
inline constexpr std::string_view kSeaPenUserVisibleQueryTemplateKey =
    "user_visible_query_template";

}  // namespace ash::wallpaper_constants

#endif  // ASH_WALLPAPER_WALLPAPER_CONSTANTS_H_
