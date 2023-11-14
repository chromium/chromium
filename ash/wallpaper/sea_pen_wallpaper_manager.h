// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_SEA_PEN_WALLPAPER_MANAGER_H_
#define ASH_WALLPAPER_SEA_PEN_WALLPAPER_MANAGER_H_

#include "ash/ash_export.h"

#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "base/functional/callback_forward.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// Helper class to process wallpapers of type `kSeaPen`.
class ASH_EXPORT SeaPenWallpaperManager {
 public:
  SeaPenWallpaperManager();

  SeaPenWallpaperManager(const SeaPenWallpaperManager&) = delete;
  SeaPenWallpaperManager& operator=(const SeaPenWallpaperManager&) = delete;

  ~SeaPenWallpaperManager();

  using DecodeSeaPenImageCallback =
      base::OnceCallback<void(uint32_t sea_pen_image_id,
                              const gfx::ImageSkia& image_skia)>;

  // Calls `callback` with the decoded image. Responds with an empty `ImageSkia`
  // on decoding failure.
  void DecodeSeaPenImage(const SeaPenImage& sea_pen_image,
                         DecodeSeaPenImageCallback callback);
};

}  // namespace ash

#endif  // ASH_WALLPAPER_SEA_PEN_WALLPAPER_MANAGER_H_
