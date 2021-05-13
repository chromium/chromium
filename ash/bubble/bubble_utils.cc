// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/bubble/bubble_utils.h"

#include "ash/capture_mode/capture_mode_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/check.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/types/event_type.h"

namespace ash {
namespace bubble_utils {

bool ShouldCloseBubbleForEvent(const ui::LocatedEvent& event) {
  // Should only be called for "press" type events.
  DCHECK(event.type() == ui::ET_MOUSE_PRESSED ||
         event.type() == ui::ET_TOUCH_PRESSED ||
         event.type() == ui::ET_GESTURE_LONG_PRESS ||
         event.type() == ui::ET_GESTURE_TAP ||
         event.type() == ui::ET_GESTURE_TWO_FINGER_TAP)
      << event.type();

  // Users in a capture session may be trying to capture the bubble.
  if (capture_mode_util::IsCaptureModeActive())
    return false;

  aura::Window* target = static_cast<aura::Window*>(event.target());
  if (!target)
    return false;

  RootWindowController* root_controller =
      RootWindowController::ForWindow(target);
  if (!root_controller)
    return false;

  // Bubbles can spawn menus, so don't close for clicks inside menus.
  aura::Window* menu_container =
      root_controller->GetContainer(kShellWindowId_MenuContainer);
  if (menu_container->Contains(target))
    return false;

  // Taps on virtual keyboard should not close bubbles.
  aura::Window* keyboard_container =
      root_controller->GetContainer(kShellWindowId_VirtualKeyboardContainer);
  if (keyboard_container->Contains(target))
    return false;

  // Touch text selection controls should not close bubbles.
  // https://crbug.com/1165938
  aura::Window* settings_bubble_container =
      root_controller->GetContainer(kShellWindowId_SettingBubbleContainer);
  if (settings_bubble_container->Contains(target))
    return false;

  return true;
}

}  // namespace bubble_utils
}  // namespace ash
