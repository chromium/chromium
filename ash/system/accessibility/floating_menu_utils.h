// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_FLOATING_MENU_UTILS_H_
#define ASH_SYSTEM_ACCESSIBILITY_FLOATING_MENU_UTILS_H_

#include "ui/views/bubble/bubble_border.h"

namespace gfx {
class Size;
}

namespace ash {

enum class FloatingMenuPosition;

// Helper functions that are used by floating menus.

// Default position for the floating menu. This depends on whether the user's
// language is LTR or RTL.
FloatingMenuPosition DefaultSystemFloatingMenuPosition();

// Determines bounds for the floating menu depending on the desired menu
// position.
gfx::Rect GetOnScreenBoundsForFloatingMenuPosition(
    const gfx::Size& menu_size,
    FloatingMenuPosition position);

// Determines the position for the view anchored to the floating menu.
views::BubbleBorder::Arrow GetAnchorAlignmentForFloatingMenuPosition(
    FloatingMenuPosition position);

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_FLOATING_MENU_UTILS_H_
