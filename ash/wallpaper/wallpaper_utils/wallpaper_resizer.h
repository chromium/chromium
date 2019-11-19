// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_RESIZER_H_
#define ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_RESIZER_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "ash/public/cpp/wallpaper_info.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_resizer_observer.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace base {
class TaskRunner;
}

namespace ash {

class WallpaperResizerObserver;

// Stores the current wallpaper data and resize it to |target_size| if needed.
class ASH_EXPORT WallpaperResizer {
 public:
  // Returns a unique identifier corresponding to |image|, suitable for
  // comparison against the value returned by original_image_id(). If the image
  // is modified, its ID will change.
  static uint32_t GetImageId(const gfx::ImageSkia& image);

  WallpaperResizer(const gfx::ImageSkia& image,
                   const gfx::Size& target_size,
                   const WallpaperInfo& info,
                   scoped_refptr<base::TaskRunner> task_runner);

  ~WallpaperResizer();

  const gfx::ImageSkia& image() const { return image_; }
  uint32_t original_image_id() const { return original_image_id_; }
  const WallpaperInfo& wallpaper_info() const { return wallpaper_info_; }

  // Called on the UI thread to run Resize() on the task runner and post an
  // OnResizeFinished() task back to the UI thread on completion.
  void StartResize();

  // Add/Remove observers.
  void AddObserver(WallpaperResizerObserver* observer);
  void RemoveObserver(WallpaperResizerObserver* observer);

 private:
  // Copies |resized_bitmap| to |image_| and notifies observers after Resize()
  // has finished running.
  void OnResizeFinished(SkBitmap* resized_bitmap);

  base::ObserverList<WallpaperResizerObserver>::Unchecked observers_;

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

  scoped_refptr<base::TaskRunner> task_runner_;

  base::WeakPtrFactory<WallpaperResizer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WallpaperResizer);
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_RESIZER_H_
