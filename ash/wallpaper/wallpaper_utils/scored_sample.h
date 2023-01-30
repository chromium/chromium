// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_UTILS_SCORED_SAMPLE_H_
#define ASH_WALLPAPER_WALLPAPER_UTILS_SCORED_SAMPLE_H_

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

// Returns the most prominent color from `image` using color quanization.
// Expected to be slow so it cannot be run on the UI thread.
SkColor ComputeWallpaperSeedColor(gfx::ImageSkia image);

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_UTILS_SCORED_SAMPLE_H_
