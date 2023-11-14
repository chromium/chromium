// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/sea_pen_wallpaper_manager.h"

#include <utility>

#include "ash/public/cpp/image_util.h"
#include "ash/public/cpp/wallpaper/sea_pen_image.h"
#include "base/functional/bind.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom-shared.h"

namespace ash {

SeaPenWallpaperManager::SeaPenWallpaperManager() = default;

SeaPenWallpaperManager::~SeaPenWallpaperManager() = default;

void SeaPenWallpaperManager::DecodeSeaPenImage(
    const SeaPenImage& sea_pen_image,
    DecodeSeaPenImageCallback callback) {
  // TODO(b/307591556) also save the image and metadata to a file.
  image_util::DecodeImageData(
      base::BindOnce(std::move(callback), sea_pen_image.id),
      data_decoder::mojom::ImageCodec::kDefault, sea_pen_image.jpg_bytes);
}

}  // namespace ash
