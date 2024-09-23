// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/floating_menu_utils.h"

#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/shell.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/work_area_insets.h"
#include "base/i18n/rtl.h"

namespace ash {

FloatingMenuPosition DefaultSystemFloatingMenuPosition() {
  return base::i18n::IsRTL() ? FloatingMenuPosition::kBottomLeft
                             : FloatingMenuPosition::kBottomRight;
}

gfx::Rect GetOnScreenBoundsForFloatingMenuPosition(
    const gfx::Size& menu_bounds,
    FloatingMenuPosition position) {
  // Calculates the ideal bounds.
  aura::Window* window = Shell::GetPrimaryRootWindow();
  gfx::Rect work_area =
      WorkAreaInsets::ForWindow(window)->user_work_area_bounds();

  switch (position) {
    case FloatingMenuPosition::kBottomRight:
      return gfx::Rect(work_area.right() - menu_bounds.width(),
                       work_area.bottom() - menu_bounds.height(),
                       menu_bounds.width(), menu_bounds.height());
    case FloatingMenuPosition::kBottomLeft:
      return gfx::Rect(work_area.x(), work_area.bottom() - menu_bounds.height(),
                       menu_bounds.width(), menu_bounds.height());
    case FloatingMenuPosition::kTopLeft:
      // Because there is no inset at the top of the widget, add
      // 2 * kCollisionWindowWorkAreaInsetsDp to the top of the work area.
      // to ensure correct padding.
      return gfx::Rect(work_area.x(),
                       work_area.y() + 2 * kCollisionWindowWorkAreaInsetsDp,
                       menu_bounds.width(), menu_bounds.height());
    case FloatingMenuPosition::kTopRight:
      // Because there is no inset at the top of the widget, add
      // 2 * kCollisionWindowWorkAreaInsetsDp to the top of the work area.
      // to ensure correct padding.
      return gfx::Rect(work_area.right() - menu_bounds.width(),
                       work_area.y() + 2 * kCollisionWindowWorkAreaInsetsDp,
                       menu_bounds.width(), menu_bounds.height());
    case FloatingMenuPosition::kSystemDefault:
      NOTREACHED();
  }
}

views::BubbleBorder::Arrow GetAnchorAlignmentForFloatingMenuPosition(
    FloatingMenuPosition position) {
  // If this is the default system position, pick the position based on the
  // language direction.
  if (position == FloatingMenuPosition::kSystemDefault) {
    position = DefaultSystemFloatingMenuPosition();
  }
  // Mirror arrow in RTL languages so that it always stays near the screen
  // edge.
  switch (position) {
    case FloatingMenuPosition::kBottomLeft:
      return base::i18n::IsRTL() ? views::BubbleBorder::Arrow::TOP_RIGHT
                                 : views::BubbleBorder::Arrow::TOP_LEFT;
    case FloatingMenuPosition::kTopLeft:
      return base::i18n::IsRTL() ? views::BubbleBorder::Arrow::BOTTOM_RIGHT
                                 : views::BubbleBorder::Arrow::BOTTOM_LEFT;
    case FloatingMenuPosition::kBottomRight:
      return base::i18n::IsRTL() ? views::BubbleBorder::Arrow::TOP_LEFT
                                 : views::BubbleBorder::Arrow::TOP_RIGHT;
    case FloatingMenuPosition::kTopRight:
      return base::i18n::IsRTL() ? views::BubbleBorder::Arrow::BOTTOM_LEFT
                                 : views::BubbleBorder::Arrow::BOTTOM_RIGHT;
    case FloatingMenuPosition::kSystemDefault:
      // It's not possible for position to be kSystemDefault here because we've
      // set it via DefaultSystemPosition() above if it was kSystemDefault.
      NOTREACHED();
  }
}

}  // namespace ash
