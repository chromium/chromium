// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/color_internals/mojom/color_internals_mojom_traits.h"

#include <vector>

#include "ash/wallpaper/wallpaper_utils/wallpaper_calculated_colors.h"
#include "ash/webui/color_internals/mojom/color_internals.mojom-shared.h"

namespace mojo {

bool StructTraits<
    ash::color_internals::mojom::WallpaperCalculatedColorsDataView,
    ash::WallpaperCalculatedColors>::
    Read(ash::color_internals::mojom::WallpaperCalculatedColorsDataView data,
         ash::WallpaperCalculatedColors* out) {
  // `WallpaperCalculatedColors` are only sent from C++ to WebUI, so
  // deserialization should never happen.
  return false;
}

}  // namespace mojo