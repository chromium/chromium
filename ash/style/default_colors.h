// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_DEFAULT_COLORS_H_
#define ASH_STYLE_DEFAULT_COLORS_H_

#include "ash/ash_export.h"
#include "ash/style/ash_color_provider.h"
#include "third_party/skia/include/core/SkColor.h"

namespace ash {

// APIs to help keeping the UI element's current color before launching
// dark/light mode. |default_color| will be returned if the dark mode feature is
// not enabled. Use these functions if the |default_color| can't be found in
// AshColorProvider. And move |default_color| to default_color_constants.h file
// to benefit future maintenance. Exported for testing.
ASH_EXPORT SkColor
DeprecatedGetBaseLayerColor(AshColorProvider::BaseLayerType type,
                            SkColor default_color);
ASH_EXPORT SkColor
DeprecatedGetControlsLayerColor(AshColorProvider::ControlsLayerType type,
                                SkColor default_color);
ASH_EXPORT SkColor
DeprecatedGetContentLayerColor(AshColorProvider::ContentLayerType type,
                               SkColor default_color);

}  // namespace ash

#endif  // ASH_STYLE_DEFAULT_COLORS_H_
