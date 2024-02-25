// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/wallpaper/google_photos_wallpaper_params.h"

#include "base/check.h"

namespace ash {

GooglePhotosWallpaperParams::GooglePhotosWallpaperParams(
    const AccountId& account_id,
    const std::string& id,
    bool daily_refresh_enabled,
    WallpaperLayout layout,
    bool preview_mode,
    std::optional<std::string> dedup_key)
    : account_id(account_id),
      id(id),
      daily_refresh_enabled(daily_refresh_enabled),
      layout(layout),
      preview_mode(preview_mode),
      dedup_key(std::move(dedup_key)) {
  // The `dedup_key` is only used to differentiate between photos (not albums)
  // and so is not applicable when daily refresh is enabled.
  if (daily_refresh_enabled)
    DCHECK(!this->dedup_key.has_value());
}

GooglePhotosWallpaperParams::GooglePhotosWallpaperParams(
    const GooglePhotosWallpaperParams& other) = default;

GooglePhotosWallpaperParams& GooglePhotosWallpaperParams::operator=(
    const GooglePhotosWallpaperParams& other) = default;

GooglePhotosWallpaperParams::~GooglePhotosWallpaperParams() = default;

std::ostream& operator<<(std::ostream& os,
                         const GooglePhotosWallpaperParams& params) {
  os << "GooglePhotosWallPaperParams:" << std::endl;
  os << "  Account Id: " << params.account_id << std::endl;
  os << "  Id: " << params.id << std::endl;
  os << "  Daily Refresh: " << params.daily_refresh_enabled << std::endl;
  os << "  Layout: " << params.layout << std::endl;
  os << "  Preview Mode: " << params.preview_mode << std::endl;
  os << "  Dedup Key: " << params.dedup_key.value_or("") << std::endl;
  return os;
}

}  // namespace ash
