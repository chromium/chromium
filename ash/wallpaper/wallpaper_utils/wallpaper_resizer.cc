// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer.h"

#include <utility>

#include "ash/utility/cropping_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace ash {
namespace {

// Resizes `image` to `target_size` using `layout`.
//
// NOTE: `image` is intentionally a copy to ensure it exists for the duration of
// the function.
SkBitmap Resize(const gfx::ImageSkia image,
                const gfx::Size& target_size,
                WallpaperLayout layout) {
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

  new_bitmap.setImmutable();
  return new_bitmap;
}

}  // namespace

// static
uint32_t WallpaperResizer::GetImageId(const gfx::ImageSkia& image) {
  const gfx::ImageSkiaRep& image_rep = image.GetRepresentation(1.0f);
  return image_rep.is_null() ? 0 : image_rep.GetBitmap().getGenerationID();
}

// static
gfx::ImageSkia WallpaperResizer::GetResizedImage(const gfx::ImageSkia& image,
                                                 int max_size_in_dips) {
  float aspect_ratio =
      static_cast<float>(image.width()) / static_cast<float>(image.height());
  int height = max_size_in_dips;
  int width = static_cast<int>(aspect_ratio * height);
  if (width > max_size_in_dips) {
    width = max_size_in_dips;
    height = static_cast<int>(width / aspect_ratio);
  }
  return gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_BEST, gfx::Size(width, height));
}

WallpaperResizer::WallpaperResizer(const gfx::ImageSkia& image,
                                   const gfx::Size& target_size,
                                   const WallpaperInfo& wallpaper_info)
    : image_(image),
      original_image_id_(GetImageId(image_)),
      target_size_(target_size),
      wallpaper_info_(wallpaper_info),
      sequenced_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})) {
  image_.MakeThreadSafe();
}

WallpaperResizer::~WallpaperResizer() = default;

void WallpaperResizer::StartResize(base::OnceClosure on_resize_done) {
  weak_ptr_factory_.InvalidateWeakPtrs();

  start_calculation_time_ = base::TimeTicks::Now();

  if (!sequenced_task_runner_->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&Resize, image_, target_size_, wallpaper_info_.layout),
          base::BindOnce(&WallpaperResizer::OnResizeFinished,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(on_resize_done)))) {
    LOG(WARNING) << "PostSequencedWorkerTask failed. "
                 << "Wallpaper may not be resized.";
  }
}

void WallpaperResizer::OnResizeFinished(base::OnceClosure on_resize_done,
                                        const SkBitmap& resized_bitmap) {
  auto resized_image = gfx::ImageSkia::CreateFrom1xBitmap(resized_bitmap);

  DVLOG(2) << __func__ << " old=" << image_.size().ToString()
           << " new=" << resized_image.size().ToString()
           << " time=" << base::TimeTicks::Now() - start_calculation_time_;

  image_ = resized_image;
  std::move(on_resize_done).Run();
}

}  // namespace ash
