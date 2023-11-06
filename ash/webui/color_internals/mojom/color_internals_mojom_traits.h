// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_COLOR_INTERNALS_MOJOM_COLOR_INTERNALS_MOJOM_TRAITS_H_
#define ASH_WEBUI_COLOR_INTERNALS_MOJOM_COLOR_INTERNALS_MOJOM_TRAITS_H_

#include "ash/wallpaper/wallpaper_utils/wallpaper_calculated_colors.h"
#include "ash/webui/color_internals/mojom/color_internals.mojom-forward.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/skia/include/core/SkColor.h"

namespace mojo {

template <>
struct StructTraits<
    ash::color_internals::mojom::WallpaperCalculatedColorsDataView,
    ash::WallpaperCalculatedColors> {
  static SkColor k_mean_color(
      const ash::WallpaperCalculatedColors& wallpaper_calculated_colors) {
    return wallpaper_calculated_colors.k_mean_color;
  }

  static SkColor celebi_color(
      const ash::WallpaperCalculatedColors& wallpaper_calculated_colors) {
    return wallpaper_calculated_colors.celebi_color;
  }

  static bool Read(
      ash::color_internals::mojom::WallpaperCalculatedColorsDataView data,
      ash::WallpaperCalculatedColors* out);
};

}  // namespace mojo

#endif  // ASH_WEBUI_COLOR_INTERNALS_MOJOM_COLOR_INTERNALS_MOJOM_TRAITS_H_
