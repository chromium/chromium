// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_file_utils.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace ash {
namespace {

// Default quality for encoding wallpaper.
constexpr int kDefaultEncodingQuality = 90;

// Resizes |image| to a resolution which is nearest to |preferred_width| and
// |preferred_height| while respecting the |layout| choice. Encodes the image to
// JPEG and saves to |output|. Returns true on success.
bool ResizeAndEncodeImage(const gfx::ImageSkia& image,
                          WallpaperLayout layout,
                          int preferred_width,
                          int preferred_height,
                          scoped_refptr<base::RefCountedBytes>* output) {
  int width = image.width();
  int height = image.height();
  int resized_width;
  int resized_height;
  *output = base::MakeRefCounted<base::RefCountedBytes>();

  if (layout == WALLPAPER_LAYOUT_CENTER_CROPPED) {
    // Do not resize wallpaper if it is smaller than preferred size.
    if (width < preferred_width || height < preferred_height) {
      return false;
    }

    // TODO(esum): This is the same scaling logic as what's in
    // ash::CenterCropImage(). Remove this code duplication.
    double horizontal_ratio = static_cast<double>(preferred_width) / width;
    double vertical_ratio = static_cast<double>(preferred_height) / height;
    if (vertical_ratio > horizontal_ratio) {
      resized_width =
          base::ClampRound(static_cast<double>(width) * vertical_ratio);
      resized_height = preferred_height;
    } else {
      resized_width = preferred_width;
      resized_height =
          base::ClampRound(static_cast<double>(height) * horizontal_ratio);
    }
  } else if (layout == WALLPAPER_LAYOUT_STRETCH) {
    resized_width = preferred_width;
    resized_height = preferred_height;
  } else {
    resized_width = width;
    resized_height = height;
  }

  gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_LANCZOS3,
      gfx::Size(resized_width, resized_height));

  SkBitmap bitmap = *(resized_image.bitmap());
  gfx::JPEGCodec::Encode(bitmap, kDefaultEncodingQuality, &(*output)->data());
  return true;
}

}  // namespace

bool ResizeAndSaveWallpaper(const gfx::ImageSkia& image,
                            const base::FilePath& path,
                            WallpaperLayout layout,
                            int preferred_width,
                            int preferred_height) {
  DVLOG(3) << __func__ << " path=" << path;
  if (layout == WALLPAPER_LAYOUT_CENTER) {
    if (base::PathExists(path)) {
      base::DeleteFile(path);
    }
    return false;
  }
  scoped_refptr<base::RefCountedBytes> data;
  if (!ResizeAndEncodeImage(image, layout, preferred_width, preferred_height,
                            &data)) {
    return false;
  }

  // Write to `temp_path` to reduce the chance of read/write
  // collisions from different sequences. This avoids issues with policy
  // wallpaper at login: b/280578317.
  base::FilePath temp_path;
  if (!base::CreateTemporaryFileInDir(path.DirName(), &temp_path)) {
    LOG(WARNING) << "Failed to create temporary file";
    return false;
  }

  if (!base::WriteFile(temp_path,
                       base::make_span(data->front(), data->size()))) {
    LOG(WARNING) << "Failed to write wallpaper data to temporary file";
    base::DeleteFile(temp_path);
    return false;
  }

  if (!base::Move(temp_path, path)) {
    LOG(WARNING) << "Failed to copy temporary wallpaper data to " << path;
    base::DeleteFile(temp_path);
    return false;
  }

  return true;
}

}  // namespace ash
