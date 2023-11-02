// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/keyboard_layout_manager.h"

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "base/trace_event/trace_event.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace keyboard {

KeyboardLayoutManager::KeyboardLayoutManager(KeyboardUIController* controller)
    : controller_(controller) {}

KeyboardLayoutManager::~KeyboardLayoutManager() = default;

// Overridden from aura::LayoutManager

void KeyboardLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  // Reset the keyboard window bounds when it gets added to the keyboard
  // container to ensure that its bounds are valid.
  SetChildBounds(child, child->GetBoundsInRootWindow());
}

void KeyboardLayoutManager::SetChildBounds(aura::Window* child,
                                           const gfx::Rect& requested_bounds) {
  aura::Window* contents_window = controller_->GetKeyboardWindow();
  if (contents_window != child) {
    // Let the bounds change to go through for windows other than the virtual
    // keyboard contents window. This is needed because IME candidate window is
    // put in VirtualKeyboardContainer managed by this layout manager.
    if (child->bounds() != requested_bounds)
      SetChildBoundsDirect(child, requested_bounds);
    return;
  }

  TRACE_EVENT0("vk", "KeyboardLayoutSetChildBounds");

  // The requested bounds must be adjusted.
  aura::Window* root_window = controller_->GetRootWindow();

  // If the keyboard has been deactivated, this reference will be null.
  if (!root_window)
    return;

  DisplayUtil display_util;
  const display::Display& display =
      display_util.GetNearestDisplayToWindow(root_window);
  const gfx::Vector2d display_offset =
      display.bounds().origin().OffsetFromOrigin();

  const gfx::Rect new_bounds =
      controller_->AdjustSetBoundsRequest(display.bounds(),
                                          requested_bounds + display_offset) -
      display_offset;

  // Keyboard bounds should only be reset when the contents window bounds
  // actually change. Otherwise it interrupts the initial animation of showing
  // the keyboard. Described in crbug.com/356753.
  gfx::Rect old_bounds = contents_window->GetTargetBounds();

  if (new_bounds == old_bounds)
    return;

  SetChildBoundsDirect(contents_window, new_bounds);
}

}  // namespace keyboard
