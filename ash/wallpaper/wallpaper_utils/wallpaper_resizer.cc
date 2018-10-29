// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer.h"

#include <utility>

#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer_observer.h"
#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_runner.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/gfx/skia_util.h"

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
            SkBitmap* resized_bitmap_out,
            base::TaskRunner* task_runner) {
  DCHECK(task_runner->RunsTasksInCurrentSequence());

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
          // The dimension with the smallest ratio must be cropped, the other
          // one is preserved. Both are set in gfx::Size cropped_size.
          double horizontal_ratio =
              static_cast<double>(new_width) / static_cast<double>(orig_width);
          double vertical_ratio = static_cast<double>(new_height) /
                                  static_cast<double>(orig_height);

          if (vertical_ratio > horizontal_ratio) {
            cropped_size = gfx::Size(
                gfx::ToRoundedInt(new_width / vertical_ratio), orig_height);
          } else {
            cropped_size = gfx::Size(
                orig_width, gfx::ToRoundedInt(new_height / horizontal_ratio));
          }
          wallpaper_rect.ClampToCenteredSize(cropped_size);
          SkBitmap sub_image;
          orig_bitmap.extractSubset(&sub_image,
                                    gfx::RectToSkIRect(wallpaper_rect));
          new_bitmap = skia::ImageOperations::Resize(
              sub_image, skia::ImageOperations::RESIZE_LANCZOS3, new_width,
              new_height);
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
      task_runner_(std::move(task_runner)),
      weak_ptr_factory_(this) {
  image_.MakeThreadSafe();
}

WallpaperResizer::~WallpaperResizer() {}

void WallpaperResizer::StartResize() {
  start_calculation_time_ = base::TimeTicks::Now();

  SkBitmap* resized_bitmap = new SkBitmap;
  if (!task_runner_->PostTaskAndReply(
          FROM_HERE,
          base::Bind(&Resize, image_, target_size_, wallpaper_info_.layout,
                     resized_bitmap, base::RetainedRef(task_runner_)),
          base::Bind(&WallpaperResizer::OnResizeFinished,
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
