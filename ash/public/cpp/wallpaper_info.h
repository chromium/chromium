// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_INFO_H_
#define ASH_PUBLIC_CPP_WALLPAPER_INFO_H_

#include "ash/public/cpp/wallpaper_types.h"
#include "base/time/time.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

struct WallpaperInfo {
  WallpaperInfo()
      : layout(WALLPAPER_LAYOUT_CENTER), type(WALLPAPER_TYPE_COUNT) {}

  WallpaperInfo(const std::string& in_location,
                WallpaperLayout in_layout,
                WallpaperType in_type,
                const base::Time& in_date)
      : location(in_location),
        layout(in_layout),
        type(in_type),
        date(in_date) {}

  ~WallpaperInfo() {}

  bool operator==(const WallpaperInfo& other) const {
    return (location == other.location) && (layout == other.layout) &&
           (type == other.type);
  }

  bool operator!=(const WallpaperInfo& other) const {
    return !(*this == other);
  }

  // Either file name of migrated wallpaper including first directory level
  // (corresponding to user wallpaper_files_id) or online wallpaper URL.
  std::string location;
  WallpaperLayout layout;
  WallpaperType type;
  base::Time date;

  // Not empty if type == WallpaperType::ONE_SHOT.
  // This field is filled in by ShowWallpaperImage when image is already
  // decoded.
  gfx::ImageSkia one_shot_wallpaper;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_INFO_H_
