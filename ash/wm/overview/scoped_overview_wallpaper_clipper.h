// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_SCOPED_OVERVIEW_WALLPAPER_CLIPPER_H_
#define ASH_WM_OVERVIEW_SCOPED_OVERVIEW_WALLPAPER_CLIPPER_H_

#include "base/memory/raw_ptr.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

class OverviewGrid;

// Manages wallpaper clipping on overview entry and restoration on overview
// exit.
class ScopedOverviewWallpaperClipper {
 public:
  explicit ScopedOverviewWallpaperClipper(OverviewGrid* overview_grid);
  ScopedOverviewWallpaperClipper(const ScopedOverviewWallpaperClipper&) =
      delete;
  ScopedOverviewWallpaperClipper& operator=(
      const ScopedOverviewWallpaperClipper&) = delete;
  ~ScopedOverviewWallpaperClipper();

  // Updates the bounds of wallpaper clip rect.
  void RefreshWallpaperClipBounds();

 private:
  // Gets the clip bounds in the wallpaper layer's parent coordinates.
  gfx::Rect GetTargetClipBounds() const;

  raw_ptr<OverviewGrid> overview_grid_;
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_SCOPED_OVERVIEW_WALLPAPER_CLIPPER_H_
