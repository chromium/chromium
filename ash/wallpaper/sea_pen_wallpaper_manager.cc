// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/sea_pen_wallpaper_manager.h"

#include <utility>

#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom-shared.h"

namespace ash {

SeaPenWallpaperManager::SeaPenWallpaperManager(
    WallpaperFileManager* wallpaper_file_manager)
    : wallpaper_file_manager_(wallpaper_file_manager) {}

SeaPenWallpaperManager::~SeaPenWallpaperManager() = default;

void SeaPenWallpaperManager::DecodeAndSaveSeaPenImage(
    const SeaPenImage& sea_pen_image,
    const base::FilePath& wallpaper_dir,
    DecodeAndSaveSeaPenImageCallback callback) {
  // TODO(b/307591556) also save metadata to a file.
  image_util::DecodeImageData(
      base::BindOnce(base::BindOnce(
          &SeaPenWallpaperManager::SaveSeaPenImage, weak_factory_.GetWeakPtr(),
          sea_pen_image.id, wallpaper_dir, std::move(callback))),
      data_decoder::mojom::ImageCodec::kDefault, sea_pen_image.jpg_bytes);
}

void SeaPenWallpaperManager::SaveSeaPenImage(
    uint32_t sea_pen_image_id,
    const base::FilePath& wallpaper_dir,
    DecodeAndSaveSeaPenImageCallback callback,
    const gfx::ImageSkia& image_skia) {
  if (image_skia.isNull()) {
    LOG(ERROR) << __func__ << "Failed to decode Sea Pen image";
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }
  std::string file_name = base::NumberToString(sea_pen_image_id) + ".jpg";
  auto on_saved = base::BindOnce(&SeaPenWallpaperManager::OnSeaPenImageSaved,
                                 weak_factory_.GetWeakPtr(), image_skia,
                                 std::move(callback));
  wallpaper_file_manager_->SaveWallpaperToDisk(
      WallpaperType::kSeaPen, wallpaper_dir, file_name,
      WallpaperLayout::WALLPAPER_LAYOUT_CENTER_CROPPED, image_skia,
      std::move(on_saved));
}

void SeaPenWallpaperManager::OnSeaPenImageSaved(
    const gfx::ImageSkia& image_skia,
    DecodeAndSaveSeaPenImageCallback callback,
    const base::FilePath& file_path) {
  if (file_path.empty()) {
    LOG(ERROR) << __func__ << "Failed to save Sea Pen image into disk";
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }
  std::move(callback).Run(image_skia);
}

}  // namespace ash
