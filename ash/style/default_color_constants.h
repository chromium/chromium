// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_DEFAULT_COLOR_CONSTANTS_H_
#define ASH_STYLE_DEFAULT_COLOR_CONSTANTS_H_

#include "ui/gfx/color_palette.h"

// Colors that can't be found in AshColorProvider. This is used to keep the UI
// element's current look before launching dark/light mode. Using this together
// with the Deprecated* functions inside default_colors.h file. Note: This file
// will be removed once enabled dark/light mode.

// Colors for power button menu.
constexpr SkColor kPowerButtonMenuFullscreenShieldColor = SK_ColorBLACK;

// Colors for back gesture.
constexpr SkColor kArrowColorBeforeActivated = gfx::kGoogleBlue600;
constexpr SkColor kArrowColorAfterActivated = gfx::kGoogleGrey100;
const SkColor kBackgroundColorBeforeActivated = SK_ColorWHITE;
const SkColor kBackgroundColorAfterActivated = gfx::kGoogleBlue600;

// Colors for back gesture nudge.
constexpr SkColor kCircleColor = SK_ColorWHITE;
constexpr SkColor kLabelBackgroundColor = SkColorSetA(SK_ColorBLACK, 0xDE);

#endif  // ASH_STYLE_DEFAULT_COLOR_CONSTANTS_H_
