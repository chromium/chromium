// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_calculated_colors.h"

namespace ash {

WallpaperCalculatedColors::WallpaperCalculatedColors() = default;

WallpaperCalculatedColors::WallpaperCalculatedColors(
    const std::vector<SkColor>& in_prominent_colors,
    SkColor in_k_mean_color,
    SkColor in_celebi_color)
    : prominent_colors(in_prominent_colors),
      k_mean_color(in_k_mean_color),
      celebi_color(in_celebi_color) {}

WallpaperCalculatedColors::WallpaperCalculatedColors(
    const WallpaperCalculatedColors& other) = default;
WallpaperCalculatedColors& WallpaperCalculatedColors::operator=(
    const WallpaperCalculatedColors& other) = default;

WallpaperCalculatedColors::WallpaperCalculatedColors(
    WallpaperCalculatedColors&& other) = default;
WallpaperCalculatedColors& WallpaperCalculatedColors::operator=(
    WallpaperCalculatedColors&& other) = default;

bool WallpaperCalculatedColors::operator==(
    const WallpaperCalculatedColors& other) const {
  return prominent_colors == other.prominent_colors &&
         k_mean_color == other.k_mean_color &&
         celebi_color == other.celebi_color;
}

bool WallpaperCalculatedColors::operator!=(
    const WallpaperCalculatedColors& other) const {
  return !(*this == other);
}

WallpaperCalculatedColors::~WallpaperCalculatedColors() = default;

}  // namespace ash
