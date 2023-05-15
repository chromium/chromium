// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_RESOLUTION_H_
#define ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_RESOLUTION_H_

#include "ash/ash_export.h"
#include "ui/gfx/geometry/size.h"

namespace ash {

enum class WallpaperResolution { kLarge, kSmall };

// Returns the maximum size of all displays combined in native
// resolutions.  Note that this isn't the bounds of the display who
// has maximum resolutions. Instead, this returns the size of the
// maximum width of all displays, and the maximum height of all displays.
ASH_EXPORT gfx::Size GetMaxDisplaySizeInNative();

// Returns the appropriate wallpaper resolution for all root windows.
ASH_EXPORT WallpaperResolution GetAppropriateResolution();

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_RESOLUTION_H_
