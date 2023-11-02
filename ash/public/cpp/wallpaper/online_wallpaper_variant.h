// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_WALLPAPER_ONLINE_WALLPAPER_VARIANT_H_
#define ASH_PUBLIC_CPP_WALLPAPER_ONLINE_WALLPAPER_VARIANT_H_

#include <cstdint>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/webui/personalization_app/proto/backdrop_wallpaper.pb.h"
#include "url/gurl.h"

namespace ash {

struct ASH_PUBLIC_EXPORT OnlineWallpaperVariant {
  OnlineWallpaperVariant(uint64_t asset_id,
                         const GURL& raw_url,
                         backdrop::Image::ImageType type);

  OnlineWallpaperVariant(const OnlineWallpaperVariant& other);

  OnlineWallpaperVariant(OnlineWallpaperVariant&& other);

  OnlineWallpaperVariant& operator=(const OnlineWallpaperVariant& other);

  bool operator==(const OnlineWallpaperVariant& other) const;

  bool operator!=(const OnlineWallpaperVariant& other) const;

  ~OnlineWallpaperVariant();

  // The unique identifier of the wallpaper.
  uint64_t asset_id;
  // The wallpaper url.
  GURL raw_url;
  // The image type.
  backdrop::Image::ImageType type;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_WALLPAPER_ONLINE_WALLPAPER_VARIANT_H_
