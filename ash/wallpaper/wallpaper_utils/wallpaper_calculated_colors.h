// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_CALCULATED_COLORS_H_
#define ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_CALCULATED_COLORS_H_

#include "ash/ash_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

// Captures the calculated prominent colors and k mean color of a wallpaper. The
// extraction logic can be found at
// |WallpaperColorCalculator::CalculateWallpaperColor|.
struct ASH_EXPORT WallpaperCalculatedColors {
  WallpaperCalculatedColors();

  WallpaperCalculatedColors(SkColor k_mean_color, SkColor celebi_color);

  WallpaperCalculatedColors(const WallpaperCalculatedColors& other);
  WallpaperCalculatedColors& operator=(const WallpaperCalculatedColors& other);

  WallpaperCalculatedColors(WallpaperCalculatedColors&& other);
  WallpaperCalculatedColors& operator=(WallpaperCalculatedColors&& other);

  bool operator==(const WallpaperCalculatedColors& other) const;

  bool operator!=(const WallpaperCalculatedColors& other) const;

  ~WallpaperCalculatedColors();

  SkColor k_mean_color = SK_ColorTRANSPARENT;
  // Result of image sampling algorithm as described in
  // https://arxiv.org/abs/1101.0395.
  SkColor celebi_color = SK_ColorTRANSPARENT;
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_UTILS_WALLPAPER_CALCULATED_COLORS_H_
