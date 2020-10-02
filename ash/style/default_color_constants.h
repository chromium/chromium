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

#endif  // ASH_STYLE_DEFAULT_COLOR_CONSTANTS_H_
