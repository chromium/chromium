// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_ONLINE_WALLPAPER_PARAMS_H_
#define ASH_PUBLIC_CPP_WALLPAPER_ONLINE_WALLPAPER_PARAMS_H_

#include <cstdint>
#include <string>

#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "components/account_id/account_id.h"
#include "url/gurl.h"

namespace ash {

struct ASH_PUBLIC_EXPORT OnlineWallpaperParams {
  // The user's account id.
  AccountId account_id;
  // The wallpaper collection id .e.g. city_for_chromebook.
  std::string collection_id;
  // The layout of the wallpaper, used for wallpaper resizing.
  WallpaperLayout layout;
  // If true, show the wallpaper immediately but doesn't change the user
  // wallpaper info until |ConfirmPreviewWallpaper| is called.
  bool preview_mode;
  // Indicate the params is a result of a user's request. i.e clicking on an
  // image.
  bool from_user = false;
  // If the `WallpaperInfo` generated from these params should have type
  // `WallpaperType::kDaily`.
  bool daily_refresh_enabled = false;
  // The unique identifier for a unit of wallpapers e.g. D/L wallpaper variants.
  uint64_t unit_id;
  // The variants related to the wallpaper. This vector also contains the
  // wallpaper itself.
  std::vector<OnlineWallpaperVariant> variants;

  OnlineWallpaperParams(const AccountId& account_id,
                        const std::string& collection_id,
                        WallpaperLayout layout,
                        bool preview_mode,
                        bool from_user,
                        bool daily_refresh_enabled,
                        uint64_t unit_id,
                        const std::vector<OnlineWallpaperVariant>& variants);

  OnlineWallpaperParams(const OnlineWallpaperParams& other);

  OnlineWallpaperParams(OnlineWallpaperParams&& other);

  OnlineWallpaperParams& operator=(const OnlineWallpaperParams& other);

  ~OnlineWallpaperParams();
};

// For logging use only. Prints out text representation of the
// `OnlineWallpaperParams`.
ASH_PUBLIC_EXPORT std::ostream& operator<<(std::ostream& os,
                                           const OnlineWallpaperParams& params);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_ONLINE_WALLPAPER_PARAMS_H_
