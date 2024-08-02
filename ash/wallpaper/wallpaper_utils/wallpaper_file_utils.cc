// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/wallpaper/wallpaper_utils/wallpaper_file_utils.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/threading/thread_restrictions.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/encode/SkJpegEncoder.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace ash {
namespace {

// Default quality for encoding wallpaper.
constexpr int kDefaultEncodingQuality = 90;

// Encodes `image_skia` to jpg with `image_metadata` encoded in the header.
// `output` will be in an undefined state on failure.
bool EncodeImage(const gfx::ImageSkia& image_skia,
                 const std::string& image_metadata,
                 scoped_refptr<base::RefCountedBytes>* output) {
  base::AssertLongCPUWorkAllowed();
  SkBitmap bitmap = *(image_skia.bitmap());
  DCHECK(!bitmap.drawsNothing());

  *output = base::MakeRefCounted<base::RefCountedBytes>();

  if (image_metadata.empty()) {
    return gfx::JPEGCodec::Encode(bitmap, kDefaultEncodingQuality,
                                  &(*output)->as_vector());
  }

  SkPixmap pixmap;
  if (!bitmap.peekPixels(&pixmap)) {
    LOG(WARNING) << "Failed to read bitmap pixels";
    return false;
  }

  auto xmpMetadata = SkData::MakeWithCString(image_metadata.c_str());

  return gfx::JPEGCodec::Encode(pixmap, kDefaultEncodingQuality,
                                SkJpegEncoder::Downsample::k420,
                                &(*output)->as_vector(), xmpMetadata.get());
}

// Resizes `image` to a resolution which is nearest to `preferred_width` and
// `preferred_height` while respecting the `layout` choice. Returns empty
// `ImageSkia` on failure.
gfx::ImageSkia ResizeImage(const gfx::ImageSkia& image_skia,
                           const WallpaperLayout layout,
                           const gfx::Size preferred_size) {
  const int width = image_skia.width();
  const int height = image_skia.height();
  const int preferred_width = preferred_size.width();
  const int preferred_height = preferred_size.height();
  int resized_width;
  int resized_height;

  if (layout == WALLPAPER_LAYOUT_CENTER_CROPPED) {
    // Do not resize wallpaper if it is smaller than preferred size.
    if (width < preferred_width || height < preferred_height) {
      DVLOG(1) << "Skip resize. Size=" << image_skia.size().ToString()
               << " Preferred_Size="
               << gfx::Size(preferred_width, preferred_height).ToString();
      return gfx::ImageSkia();
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

  return gfx::ImageSkiaOperations::CreateResizedImage(
      image_skia, skia::ImageOperations::RESIZE_LANCZOS3,
      gfx::Size(resized_width, resized_height));
}

bool SaveWallpaper(const gfx::ImageSkia& image,
                   const base::FilePath& path,
                   const std::string& image_metadata) {
  scoped_refptr<base::RefCountedBytes> data;
  if (!EncodeImage(image, image_metadata, &data)) {
    LOG(WARNING) << "Encoding wallpaper image failed";
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

}  // namespace

bool ResizeAndSaveWallpaper(const gfx::ImageSkia& image,
                            const base::FilePath& path,
                            const WallpaperLayout layout,
                            const gfx::Size preferred_size,
                            const std::string& image_metadata) {
  if (layout == WALLPAPER_LAYOUT_CENTER) {
    // TODO(b/325498873) remove this.
    if (base::PathExists(path)) {
      DVLOG(1) << "Deleting path " << path;
      base::DeleteFile(path);
    }
    DVLOG(1) << "Skipping resize and save for WALLPAPER_LAYOUT_CENTER path "
             << path;
    return false;
  }

  gfx::ImageSkia resized_image = ResizeImage(image, layout, preferred_size);
  if (resized_image.isNull()) {
    LOG(WARNING) << "Failed to resize image";
    return false;
  }

  return SaveWallpaper(resized_image, path, image_metadata);
}

void CreateDirectoryAndLogError(const base::FilePath& directory) {
  DCHECK(!directory.empty());
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(directory, &error)) {
    LOG(WARNING) << "Failed to create wallpaper directory: " << error;
  }
}

}  // namespace ash
