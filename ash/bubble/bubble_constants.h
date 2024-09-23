// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BUBBLE_BUBBLE_CONSTANTS_H_
#define ASH_BUBBLE_BUBBLE_CONSTANTS_H_

#include "ash/constants/ash_features.h"
#include "ash/style/system_shadow.h"

namespace ash {

// TODO(http://b/331210989): Merge these 2 constants back to kBubbleCornerRadius
// once `kEnableBubbleCornerRadiusUpdate` is launched. The corner radius of a
// bubble, like the system tray bubble or the productivity launcher bubble.
inline constexpr int kUpdatedBubbleCornerRadius = 24;
inline constexpr int kDeprecatedBubbleCornerRadius = 16;

inline int GetBubbleCornerRadius() {
  return features::IsBubbleCornerRadiusUpdateEnabled()
             ? kUpdatedBubbleCornerRadius
             : kDeprecatedBubbleCornerRadius;
}

// Padding used for bubbles that represent a menu of options, like the system
// tray bubble or the switch access menu.
inline constexpr int kBubbleMenuPadding = 8;

// The elevation used for system tray bubble.
inline constexpr SystemShadow::Type kBubbleShadowType =
    SystemShadow::Type::kElevation12;

}  // namespace ash

#endif  // ASH_BUBBLE_BUBBLE_CONSTANTS_H_
