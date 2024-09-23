// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_RESIZER_H_
#define ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_RESIZER_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// Stores the current wallpaper data and resize it to |target_size| if needed.
class ASH_EXPORT WallpaperResizer {
 public:
  // Returns a unique identifier corresponding to |image|, suitable for
  // comparison against the value returned by original_image_id(). If the image
  // is modified, its ID will change.
  static uint32_t GetImageId(const gfx::ImageSkia& image);

  // Synchronously resizes the image while maintaining the aspect ratio.
  static gfx::ImageSkia GetResizedImage(const gfx::ImageSkia& image,
                                        int max_size_in_dips);

  WallpaperResizer(const gfx::ImageSkia& image,
                   const gfx::Size& target_size,
                   const WallpaperInfo& info);

  WallpaperResizer(const WallpaperResizer&) = delete;
  WallpaperResizer& operator=(const WallpaperResizer&) = delete;

  ~WallpaperResizer();

  const gfx::ImageSkia& image() const { return image_; }
  uint32_t original_image_id() const { return original_image_id_; }
  const WallpaperInfo& wallpaper_info() const { return wallpaper_info_; }

  // Called on the UI thread to run Resize() on the task runner and post an
  // OnResizeFinished() task back to the UI thread on completion.
  void StartResize(base::OnceClosure on_resize_done);

 private:
  // Copies `resized_image` to `image_` and runs callback `on_resize_done`.
  void OnResizeFinished(base::OnceClosure on_resize_done,
                        gfx::ImageSkia resized_image);

  // Image that should currently be used for wallpaper. It initially
  // contains the original image and is updated to contain the resized
  // image by OnResizeFinished().
  gfx::ImageSkia image_;

  // Unique identifier corresponding to the original (i.e. pre-resize) |image_|.
  uint32_t original_image_id_;

  gfx::Size target_size_;

  const WallpaperInfo wallpaper_info_;

  // The time that StartResize() was last called. Used for recording timing
  // metrics.
  base::TimeTicks start_calculation_time_;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  base::WeakPtrFactory<WallpaperResizer> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_RESIZER_H_
