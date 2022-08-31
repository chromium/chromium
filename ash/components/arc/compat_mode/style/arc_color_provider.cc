// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/style/arc_color_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"
#include "ash/style/dark_light_mode_controller_impl.h"

namespace arc {

// Dialog background base color
constexpr SkColor kDialogBackgroundBaseColorLight = SK_ColorWHITE;
constexpr SkColor kDialogBackgroundBaseColorDark =
    SkColorSetRGB(0x32, 0x33, 0x36);

SkColor GetDialogBackgroundBaseColor() {
  return IsDarkModeEnabled() ? kDialogBackgroundBaseColorDark
                             : kDialogBackgroundBaseColorLight;
}

SkColor GetCrOSColor(cros_styles::ColorName color_name) {
  return cros_styles::ResolveColor(color_name, IsDarkModeEnabled(),
                                   /*use_debug_colors=*/false);
}

bool IsDarkModeEnabled() {
  auto* dark_light_mode_controller = ash::DarkLightModeControllerImpl::Get();
  // |dark_light_mode_controller| may return null in unit testing.
  if (!dark_light_mode_controller)
    return false;
  return ash::features::IsDarkLightModeEnabled() &&
         dark_light_mode_controller->IsDarkModeEnabled();
}

}  // namespace arc
