// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer.h"

#include <utility>

#include "ash/utility/cropping_util.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer_observer.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/thread_restrictions.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace ash {
namespace {

// Resizes |image| to |target_size| using |layout| and stores the
// resulting bitmap at |resized_bitmap_out|.
//
// NOTE: |image| is intentionally a copy to ensure it exists for the duration of
// the function.
void Resize(const gfx::ImageSkia image,
            const gfx::Size& target_size,
            WallpaperLayout layout,
            SkBitmap* resized_bitmap_out) {
  base::AssertLongCPUWorkAllowed();

  SkBitmap orig_bitmap = *image.bitmap();
  SkBitmap new_bitmap = orig_bitmap;

  const int orig_width = orig_bitmap.width();
  const int orig_height = orig_bitmap.height();
  const int new_width = target_size.width();
  const int new_height = target_size.height();

  if (orig_width > new_width || orig_height > new_height) {
    gfx::Rect wallpaper_rect(0, 0, orig_width, orig_height);
    gfx::Size cropped_size = gfx::Size(std::min(new_width, orig_width),
                                       std::min(new_height, orig_height));
    switch (layout) {
      case WALLPAPER_LAYOUT_CENTER:
        wallpaper_rect.ClampToCenteredSize(cropped_size);
        orig_bitmap.extractSubset(&new_bitmap,
                                  gfx::RectToSkIRect(wallpaper_rect));
        break;
      case WALLPAPER_LAYOUT_TILE:
        wallpaper_rect.set_size(cropped_size);
        orig_bitmap.extractSubset(&new_bitmap,
                                  gfx::RectToSkIRect(wallpaper_rect));
        break;
      case WALLPAPER_LAYOUT_STRETCH:
        new_bitmap = skia::ImageOperations::Resize(
            orig_bitmap, skia::ImageOperations::RESIZE_LANCZOS3, new_width,
            new_height);
        break;
      case WALLPAPER_LAYOUT_CENTER_CROPPED:
        if (orig_width > new_width && orig_height > new_height) {
          new_bitmap = skia::ImageOperations::Resize(
              CenterCropImage(orig_bitmap, target_size),
              skia::ImageOperations::RESIZE_LANCZOS3, new_width, new_height);
        }
        break;
      case NUM_WALLPAPER_LAYOUT:
        NOTREACHED();
        break;
    }
  }

  *resized_bitmap_out = new_bitmap;
  resized_bitmap_out->setImmutable();
}

}  // namespace

// static
uint32_t WallpaperResizer::GetImageId(const gfx::ImageSkia& image) {
  const gfx::ImageSkiaRep& image_rep = image.GetRepresentation(1.0f);
  return image_rep.is_null() ? 0 : image_rep.GetBitmap().getGenerationID();
}

WallpaperResizer::WallpaperResizer(const gfx::ImageSkia& image,
                                   const gfx::Size& target_size,
                                   const WallpaperInfo& wallpaper_info,
                                   scoped_refptr<base::TaskRunner> task_runner)
    : image_(image),
      original_image_id_(GetImageId(image_)),
      target_size_(target_size),
      wallpaper_info_(wallpaper_info),
      task_runner_(std::move(task_runner)) {
  image_.MakeThreadSafe();
}

WallpaperResizer::~WallpaperResizer() {}

void WallpaperResizer::StartResize() {
  start_calculation_time_ = base::TimeTicks::Now();

  SkBitmap* resized_bitmap = new SkBitmap;
  if (!task_runner_->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&Resize, image_, target_size_, wallpaper_info_.layout,
                         resized_bitmap),
          base::BindOnce(&WallpaperResizer::OnResizeFinished,
                         weak_ptr_factory_.GetWeakPtr(),
                         base::Owned(resized_bitmap)))) {
    LOG(WARNING) << "PostSequencedWorkerTask failed. "
                 << "Wallpaper may not be resized.";
  }
}

void WallpaperResizer::AddObserver(WallpaperResizerObserver* observer) {
  observers_.AddObserver(observer);
}

void WallpaperResizer::RemoveObserver(WallpaperResizerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WallpaperResizer::OnResizeFinished(SkBitmap* resized_bitmap) {
  image_ = gfx::ImageSkia::CreateFrom1xBitmap(*resized_bitmap);
  UMA_HISTOGRAM_TIMES("Ash.Wallpaper.TimeSpentResizing",
                      base::TimeTicks::Now() - start_calculation_time_);

  for (auto& observer : observers_)
    observer.OnWallpaperResized();
}

}  // namespace ash
