// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/wallpaper/google_photos_wallpaper_params.h"

namespace ash {

GooglePhotosWallpaperParams::GooglePhotosWallpaperParams(
    const AccountId& account_id,
    const std::string& id,
    WallpaperLayout layout)
    : account_id(account_id), id(id), layout(layout) {}

GooglePhotosWallpaperParams::GooglePhotosWallpaperParams(
    const GooglePhotosWallpaperParams& other) = default;

GooglePhotosWallpaperParams& GooglePhotosWallpaperParams::operator=(
    const GooglePhotosWallpaperParams& other) = default;

GooglePhotosWallpaperParams::~GooglePhotosWallpaperParams() = default;

std::ostream& operator<<(std::ostream& os,
                         const GooglePhotosWallpaperParams& params) {
  os << "GooglePhotosWallPaperParams:" << std::endl;
  os << "  Account Id: " << params.account_id << std::endl;
  os << "  Photo Id: " << params.id << std::endl;
  os << "  Layout: " << params.layout << std::endl;
  return os;
}

}  // namespace ash
