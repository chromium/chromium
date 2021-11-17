// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_COMPAT_MODE_STYLE_ARC_COLOR_PROVIDER_H_
#define ASH_COMPONENTS_ARC_COMPAT_MODE_STYLE_ARC_COLOR_PROVIDER_H_

#include "ash/public/cpp/style/color_provider.h"
#include "ui/chromeos/styles/cros_styles.h"

namespace arc {

using ShieldLayerType = ash::ColorProvider::ShieldLayerType;
using ContentLayerType = ash::ColorProvider::ContentLayerType;

// Get the shield layer color
SkColor GetShieldLayerColor(ShieldLayerType type);

// Get the content layer color
SkColor GetContentLayerColor(ContentLayerType type);

// Get dialog background base color
SkColor GetDialogBackgroundBaseColor();

// Get color from cros_styles based on the current semantics (dark/light mode).
SkColor GetCrOSColor(cros_styles::ColorName color_name);

// Determine if dark mode is enabled
bool IsDarkModeEnabled();

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_COMPAT_MODE_STYLE_ARC_COLOR_PROVIDER_H_
