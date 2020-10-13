// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_DEFAULT_COLORS_H_
#define ASH_STYLE_DEFAULT_COLORS_H_

#include "ash/style/ash_color_provider.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

// APIs to help keeping the UI element's current color before launching
// dark/light mode. |default_color| will be returned if the dark mode feature is
// not enabled. Use these functions if the |default_color| can't be found in
// AshColorProvider. And move |default_color| to default_color_constants.h file
// to benefit future maintenance.
SkColor DeprecatedGetShieldLayerColor(AshColorProvider::ShieldLayerType type,
                                      SkColor default_color);
SkColor DeprecatedGetBaseLayerColor(AshColorProvider::BaseLayerType type,
                                    SkColor default_color);
SkColor DeprecatedGetControlsLayerColor(
    AshColorProvider::ControlsLayerType type,
    SkColor default_color);
SkColor DeprecatedGetContentLayerColor(AshColorProvider::ContentLayerType type,
                                       SkColor default_color);
SkColor DeprecatedGetLoginBackgroundBaseColor(SkColor default_color);
SkColor DeprecatedGetInkDropBaseColor(SkColor default_color);
SkColor DeprecatedGetInkDropRippleColor(SkColor default_color);
SkColor DeprecatedGetInkDropHighlightColor(SkColor default_color);
float DeprecatedGetInkDropOpacity(float default_opacity);
SkColor DeprecatedGetAppStateIndicatorColor(bool active,
                                            SkColor active_color,
                                            SkColor default_color);
}  // namespace ash

#endif  // ASH_STYLE_DEFAULT_COLORS_H_
