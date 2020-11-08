// Copyright 2020 The Chromium Authors. All rights reserved.
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

// Login shelf button color constants
constexpr SkColor kLoginShelfButtonLabelColor = gfx::kGoogleGrey100;
constexpr SkColor kLoginShelfButtonIconColor = SkColorSetRGB(0xEB, 0xEA, 0xED);
constexpr SkColor kLoginBackgroundBaseColor = SK_ColorBLACK;
constexpr SkColor kLoginButtonBackgroundBaseColor =
    SkColorSetA(SK_ColorWHITE, 26);

// Colors for shelf.
constexpr SkColor kDefaultShelfInkDropColor = SK_ColorWHITE;
constexpr float kDefaultShelfInkDropOpacity = 0.2f;
constexpr SkColor kShelfButtonColor = SK_ColorWHITE;
constexpr SkColor kIndicatorColorActive = SK_ColorWHITE;
constexpr SkColor kInicatorColorRunning = SkColorSetA(SK_ColorWHITE, 0x7F);

#endif  // ASH_STYLE_DEFAULT_COLOR_CONSTANTS_H_
