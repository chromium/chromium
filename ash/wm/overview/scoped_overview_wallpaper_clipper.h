// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_SCOPED_OVERVIEW_WALLPAPER_CLIPPER_H_
#define ASH_WM_OVERVIEW_SCOPED_OVERVIEW_WALLPAPER_CLIPPER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class OverviewGrid;

// Manages wallpaper clipping on overview entry and restoration on overview
// exit.
class ScopedOverviewWallpaperClipper {
 public:
  enum class AnimationType {
    // Initial cropping animation when entering informed restore session.
    kEnterInformedRestore,
    // Initial cropping animation when entering Overview.
    kEnterOverview,
    // Lift the bottom of clipping area to show birch bar in Overview.
    kShowBirchBarInOverview,
    // Lift the bottom of the clipping area when the birch bar is enabled by
    // user.
    kShowBirchBarByUser,
    // Restore the clipping area when the birch bar is disabled by user.
    kHideBirchBarByUser,
    // Update the clipping area when the birch bar is relayout.
    kRelayoutBirchBar,
    // Restore animation when exiting Overview or informed restore session.
    kRestore,
    // No animation needed.
    kNone,
  };

  ScopedOverviewWallpaperClipper(OverviewGrid* overview_grid,
                                 AnimationType animation_type);
  ScopedOverviewWallpaperClipper(const ScopedOverviewWallpaperClipper&) =
      delete;
  ScopedOverviewWallpaperClipper& operator=(
      const ScopedOverviewWallpaperClipper&) = delete;
  ~ScopedOverviewWallpaperClipper();

  // Updates the bounds of wallpaper clip rect with given animation type and end
  // callback.
  void RefreshWallpaperClipBounds(AnimationType animation_type,
                                  base::OnceClosure animation_end_callback);

 private:
  raw_ptr<OverviewGrid> overview_grid_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_SCOPED_OVERVIEW_WALLPAPER_CLIPPER_H_
