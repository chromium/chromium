// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_DEFAULT_COLOR_CONSTANTS_H_
#define ASH_STYLE_DEFAULT_COLOR_CONSTANTS_H_

#include "ui/gfx/color_palette.h"

// Colors used by AshColorMode::kDefault mode of system UI. Only old specs needs
// to support |kDefault| mode. New specs will always follow colors for |kLight|
// or |kDark| mode. Note, don't add new colors to this file since this file and
// related logic will be removed after launch dark/light mode.

// Colors for status area trays, includs system tray, IME, stylus and
// notifications.
//----------------------------------Begin--------------------------------------
constexpr SkColor kUnifiedMenuBackgroundColor =
    SkColorSetA(gfx::kGoogleGrey900, 0xf2);  // 95%
constexpr SkColor kUnifiedMenuBackgroundColorWithBlur =
    SkColorSetA(kUnifiedMenuBackgroundColor, 0x99);  // 60%

constexpr SkColor kNotificationBackgroundColor = SK_ColorWHITE;
constexpr SkColor kUnifiedMenuTextColor = gfx::kGoogleGrey100;
constexpr SkColor kUnifiedMenuButtonColor =
    SkColorSetA(gfx::kGoogleGrey200, 0x14);
constexpr SkColor kUnifiedMenuButtonColorActive = gfx::kGoogleBlueDark600;
constexpr SkColor kProminentIconButtonColor = gfx::kGoogleGreen700;
constexpr SkColor kLogoutButtonTrayColor = gfx::kGoogleRed700;

//----------------------------------End----------------------------------------

// Colors for toast.
constexpr SkColor kToastBackgroundColor =
    SkColorSetA(SK_ColorBLACK, 0xCC);  // 80%
constexpr SkColor kToastLabelColor = SK_ColorWHITE;

// colors for power button menu.
constexpr SkColor kPowerButtonMenuItemIconColor = gfx::kGoogleGrey900;
constexpr SkColor kPowerButtonMenuItemTitleColor = gfx::kGoogleGrey700;
constexpr SkColor kPowerButtonMenuItemFocusColor =
    SkColorSetA(gfx::kGoogleBlue600, 0x66);  // 40%
constexpr SkColor kPowerButtonMenuBackgroundColor = SK_ColorWHITE;
constexpr SkColor kPowerButtonMenuFullscreenShieldColor = SK_ColorBLACK;

#endif  // ASH_STYLE_DEFAULT_COLOR_CONSTANTS_H_
