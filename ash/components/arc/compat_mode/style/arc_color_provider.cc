// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/style/arc_color_provider.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/style/scoped_light_mode_as_default.h"

namespace arc {

// Default color
constexpr SkColor kDefaultColor = SK_ColorWHITE;
// Dialog background base color
constexpr SkColor kDialogBackgroundBaseColorLight = SK_ColorWHITE;
constexpr SkColor kDialogBackgroundBaseColorDark =
    SkColorSetRGB(0x32, 0x33, 0x36);

SkColor GetShieldLayerColor(ShieldLayerType type) {
  auto* provider = ash::ColorProvider::Get();
  // |provider| may return null in unit testing
  if (!provider)
    return kDefaultColor;
  ash::ScopedLightModeAsDefault scoped_light_mode_as_default;
  return provider->GetShieldLayerColor(type);
}

SkColor GetContentLayerColor(ContentLayerType type) {
  auto* provider = ash::ColorProvider::Get();
  // |provider| may return null in unit testing
  if (!provider)
    return kDefaultColor;
  ash::ScopedLightModeAsDefault scoped_light_mode_as_default;
  return provider->GetContentLayerColor(type);
}

SkColor GetDialogBackgroundBaseColor() {
  return IsDarkModeEnabled() ? kDialogBackgroundBaseColorDark
                             : kDialogBackgroundBaseColorLight;
}

SkColor GetCrOSColor(cros_styles::ColorName color_name) {
  return cros_styles::ResolveColor(color_name, IsDarkModeEnabled(),
                                   /*use_debug_colors=*/false);
}

bool IsDarkModeEnabled() {
  auto* provider = ash::ColorProvider::Get();
  // |provider| may return null in unit testing
  if (!provider)
    return false;
  return ash::features::IsDarkLightModeEnabled() &&
         provider->IsDarkModeEnabled();
}

}  // namespace arc
