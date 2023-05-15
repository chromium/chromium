// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_resolution.h"

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

gfx::Size GetMaxDisplaySizeInNative() {
  // Return an empty size for test environments where the screen is null.
  if (!display::Screen::GetScreen()) {
    return gfx::Size();
  }

  gfx::Size max;
  for (const auto& display : display::Screen::GetScreen()->GetAllDisplays()) {
    max.SetToMax(display.GetSizeInPixel());
  }

  return max;
}

WallpaperResolution GetAppropriateResolution() {
  gfx::Size size = GetMaxDisplaySizeInNative();
  return (size.width() > kSmallWallpaperMaxWidth ||
          size.height() > kSmallWallpaperMaxHeight)
             ? WallpaperResolution::kLarge
             : WallpaperResolution::kSmall;
}

}  // namespace ash
