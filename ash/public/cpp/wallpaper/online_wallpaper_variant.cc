// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/wallpaper/online_wallpaper_variant.h"

namespace ash {

OnlineWallpaperVariant::OnlineWallpaperVariant(
    uint64_t in_asset_id,
    const GURL& in_url,
    backdrop::Image::ImageType in_type)
    : asset_id(in_asset_id), url(in_url), type(in_type) {
  DCHECK(!url.is_empty());
}

OnlineWallpaperVariant::OnlineWallpaperVariant(
    const OnlineWallpaperVariant& other) = default;

OnlineWallpaperVariant::OnlineWallpaperVariant(OnlineWallpaperVariant&& other) =
    default;

OnlineWallpaperVariant& OnlineWallpaperVariant::operator=(
    const OnlineWallpaperVariant& other) = default;

OnlineWallpaperVariant::~OnlineWallpaperVariant() = default;

}  // namespace ash
