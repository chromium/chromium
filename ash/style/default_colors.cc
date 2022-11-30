// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/style/default_colors.h"

#include "ash/constants/ash_features.h"

namespace ash {

SkColor DeprecatedGetBaseLayerColor(AshColorProvider::BaseLayerType type,
                                    SkColor default_color) {
  if (!features::IsDarkLightModeEnabled())
    return default_color;

  return AshColorProvider::Get()->GetBaseLayerColor(type);
}

SkColor DeprecatedGetControlsLayerColor(
    AshColorProvider::ControlsLayerType type,
    SkColor default_color) {
  if (!features::IsDarkLightModeEnabled())
    return default_color;

  return AshColorProvider::Get()->GetControlsLayerColor(type);
}

SkColor DeprecatedGetContentLayerColor(AshColorProvider::ContentLayerType type,
                                       SkColor default_color) {
  if (!features::IsDarkLightModeEnabled())
    return default_color;

  return AshColorProvider::Get()->GetContentLayerColor(type);
}

}  // namespace ash
