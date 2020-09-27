// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_CONSTANTS_H_
#define ASH_PUBLIC_CPP_ASH_CONSTANTS_H_

#include "ash/public/cpp/accessibility_controller_enums.h"
#include "base/time/time.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"

typedef unsigned int SkColor;

namespace ash {

// Radius of the header's top corners when the window is restored.
constexpr int kTopCornerRadiusWhenRestored = 2;

// Background color used for the Chrome OS boot splash screen.
constexpr SkColor kChromeOsBootColor = SkColorSetRGB(0xfe, 0xfe, 0xfe);

// The border thickness of keyboard focus for launcher items and system tray.
constexpr int kFocusBorderThickness = 2;

constexpr int kDefaultLargeCursorSize = 64;

constexpr SkColor kDefaultCursorColor = SK_ColorBLACK;

// The option in the Switch Access settings for no switch assigned.
constexpr int kSwitchAccessAssignmentNone = 0;

// The default delay before Switch Access automatically moves to the next
// element on the page that is interesting, based on the Switch Access
// predicates.
constexpr base::TimeDelta kDefaultSwitchAccessAutoScanSpeed =
    base::TimeDelta::FromMilliseconds(1200);

// The default wait time between last mouse movement and sending autoclick.
constexpr int kDefaultAutoclickDelayMs = 1000;

constexpr AutoclickEventType kDefaultAutoclickEventType =
    AutoclickEventType::kLeftClick;

// The default threshold of mouse movement, measured in DIP, that will initiate
// a new autoclick.
constexpr int kDefaultAutoclickMovementThreshold = 20;

// The default automatic click menu position.
constexpr FloatingMenuPosition kDefaultAutoclickMenuPosition =
    FloatingMenuPosition::kSystemDefault;

// The default floating accessibility menu position.
constexpr FloatingMenuPosition kDefaultFloatingMenuPosition =
    FloatingMenuPosition::kSystemDefault;

// Whether keyboard auto repeat is enabled by default.
constexpr bool kDefaultKeyAutoRepeatEnabled = true;

// Whether dark mode is enabled by default.
constexpr bool kDefaultDarkModeEnabled = true;

// Whether color mode is themed by default.
constexpr bool kDefaultColorModeThemed = true;

// The default delay before a held keypress will start to auto repeat.
constexpr base::TimeDelta kDefaultKeyAutoRepeatDelay =
    base::TimeDelta::FromMilliseconds(500);

// The default interval between auto-repeated key events.
constexpr base::TimeDelta kDefaultKeyAutoRepeatInterval =
    base::TimeDelta::FromMilliseconds(50);

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_CONSTANTS_H_
