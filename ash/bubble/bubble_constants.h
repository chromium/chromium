// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BUBBLE_BUBBLE_CONSTANTS_H_
#define ASH_BUBBLE_BUBBLE_CONSTANTS_H_

#include "ash/style/system_shadow.h"

namespace ash {

// The corner radius of a bubble, like the system tray bubble or the
// productivity launcher bubble.
inline constexpr int kBubbleCornerRadius = 16;

// Padding used for bubbles that represent a menu of options, like the system
// tray bubble or the switch access menu.
inline constexpr int kBubbleMenuPadding = 8;

// The elevation used for system tray bubble.
inline constexpr SystemShadow::Type kBubbleShadowType =
    SystemShadow::Type::kElevation12;

}  // namespace ash

#endif  // ASH_BUBBLE_BUBBLE_CONSTANTS_H_
