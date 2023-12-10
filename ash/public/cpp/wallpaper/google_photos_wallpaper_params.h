// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_GOOGLE_PHOTOS_WALLPAPER_PARAMS_H_
#define ASH_PUBLIC_CPP_WALLPAPER_GOOGLE_PHOTOS_WALLPAPER_PARAMS_H_

#include <optional>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "components/account_id/account_id.h"

namespace ash {

struct ASH_PUBLIC_EXPORT GooglePhotosWallpaperParams {
  GooglePhotosWallpaperParams(const AccountId& account_id,
                              const std::string& id,
                              bool daily_refresh_enabled,
                              WallpaperLayout layout,
                              bool preview_mode,
                              std::optional<std::string> dedup_key);

  GooglePhotosWallpaperParams(const GooglePhotosWallpaperParams& other);

  GooglePhotosWallpaperParams& operator=(
      const GooglePhotosWallpaperParams& other);

  ~GooglePhotosWallpaperParams();

  // The user's account id.
  AccountId account_id;

  // The unique identifier for the photo or album.
  std::string id;

  // Whether to start daily refresh, and `id` is an album id.
  bool daily_refresh_enabled = false;

  // The layout of the wallpaper, used for wallpaper resizing.
  WallpaperLayout layout;

  // If true, show the wallpaper immediately, but don't change the user
  // wallpaper info until `ConfirmPreviewWallpaper()` is called.
  bool preview_mode;

  // A string which uniquely identifies a Google Photos photo across albums.
  // Note that the same photo appearing in multiple albums will have a unique
  // `id` for each album in which it appears, but the `dedup_key` is shared
  // across albums.
  std::optional<std::string> dedup_key;
};

ASH_PUBLIC_EXPORT std::ostream& operator<<(
    std::ostream& os,
    const GooglePhotosWallpaperParams& params);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_GOOGLE_PHOTOS_WALLPAPER_PARAMS_H_
