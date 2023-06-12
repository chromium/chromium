// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_calculated_colors.h"

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "base/notreached.h"
#include "ui/gfx/color_analysis.h"

namespace ash {

namespace {

using ColorProfile = color_utils::ColorProfile;
using LumaRange = color_utils::LumaRange;
using SaturationRange = color_utils::SaturationRange;

// Gets the corresponding color profile type based on the given |color_profile|.
ColorProfileType GetColorProfileType(ColorProfile color_profile) {
  bool vibrant = color_profile.saturation == SaturationRange::VIBRANT;
  switch (color_profile.luma) {
    case LumaRange::ANY:
      // There should be no color profiles with the ANY luma range.
      NOTREACHED();
      break;
    case LumaRange::DARK:
      return vibrant ? ColorProfileType::DARK_VIBRANT
                     : ColorProfileType::DARK_MUTED;
    case LumaRange::NORMAL:
      return vibrant ? ColorProfileType::NORMAL_VIBRANT
                     : ColorProfileType::NORMAL_MUTED;
    case LumaRange::LIGHT:
      return vibrant ? ColorProfileType::LIGHT_VIBRANT
                     : ColorProfileType::LIGHT_MUTED;
  }
  NOTREACHED();
  return ColorProfileType::DARK_MUTED;
}

}  // namespace

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

SkColor WallpaperCalculatedColors::GetProminentColor(
    ColorProfile color_profile) const {
  ColorProfileType type = GetColorProfileType(color_profile);
  size_t index = static_cast<size_t>(type);
  DCHECK_LT(index, prominent_colors.size());
  return prominent_colors[index];
}

}  // namespace ash
