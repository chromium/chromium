// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_SEA_PEN_WALLPAPER_MANAGER_H_
#define ASH_WALLPAPER_SEA_PEN_WALLPAPER_MANAGER_H_

#include "ash/ash_export.h"

#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "ash/wallpaper/wallpaper_file_manager.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// Helper class to process wallpapers of type `kSeaPen`.
class ASH_EXPORT SeaPenWallpaperManager {
 public:
  explicit SeaPenWallpaperManager(WallpaperFileManager* wallpaper_file_manager);

  SeaPenWallpaperManager(const SeaPenWallpaperManager&) = delete;
  SeaPenWallpaperManager& operator=(const SeaPenWallpaperManager&) = delete;

  ~SeaPenWallpaperManager();

  using DecodeAndSaveSeaPenImageCallback =
      base::OnceCallback<void(const gfx::ImageSkia& image_skia)>;

  // Decodes Sea Pen image then save the decoded image into disk. Calls
  // `callback` with the image id and the decoded image. Responds with an empty
  // `ImageSkia` on decoding failure.
  void DecodeAndSaveSeaPenImage(const SeaPenImage& sea_pen_image,
                                const base::FilePath& wallpaper_dir,
                                DecodeAndSaveSeaPenImageCallback callback);

 private:
  void SaveSeaPenImage(uint32_t sea_pen_image_id,
                       const base::FilePath& wallpaper_dir,
                       DecodeAndSaveSeaPenImageCallback callback,
                       const gfx::ImageSkia& image_skia);

  void OnSeaPenImageSaved(const gfx::ImageSkia& image_skia,
                          DecodeAndSaveSeaPenImageCallback callback,
                          const base::FilePath& file_path);

  raw_ptr<WallpaperFileManager> wallpaper_file_manager_;

  base::WeakPtrFactory<SeaPenWallpaperManager> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_WALLPAPER_SEA_PEN_WALLPAPER_MANAGER_H_
