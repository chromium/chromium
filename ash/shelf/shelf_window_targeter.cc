// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_window_targeter.h"

#include "ash/public/cpp/session/session_types.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ui/aura/window.h"

namespace ash {

namespace {

gfx::Insets GetInsetsForAlignment(int distance, ShelfAlignment alignment) {
  if (alignment == ShelfAlignment::kLeft)
    return gfx::Insets(0, 0, 0, distance);
  if (alignment == ShelfAlignment::kRight)
    return gfx::Insets(0, distance, 0, 0);
  return gfx::Insets(distance, 0, 0, 0);
}

}  // namespace

ShelfWindowTargeter::ShelfWindowTargeter(aura::Window* container, Shelf* shelf)
    : ::wm::EasyResizeWindowTargeter(gfx::Insets(), gfx::Insets()),
      shelf_(shelf) {
  WillChangeVisibilityState(shelf_->GetVisibilityState());
  container->AddObserver(this);
  shelf_->AddObserver(this);
}

ShelfWindowTargeter::~ShelfWindowTargeter() {
  // Ensure that the observers were removed and the shelf pointer was cleared.
  DCHECK(!shelf_);
}

bool ShelfWindowTargeter::ShouldUseExtendedBounds(const aura::Window* w) const {
  // Use extended bounds only for direct child of the container.
  return window() == w->parent();
}

bool ShelfWindowTargeter::GetHitTestRects(
    aura::Window* target,
    gfx::Rect* hit_test_rect_mouse,
    gfx::Rect* hit_test_rect_touch) const {
  // We only want to special case a very specific situation where we are not
  // currently in an active session (or unknown session state) and change only
  // the behavior of the login shelf. On secondary displays, the login shelf
  // will not be visible.
  bool target_is_shelf_widget =
      target == shelf_->shelf_widget()->GetNativeWindow();
  if (target_is_shelf_widget &&
      Shell::Get()->session_controller()->GetSessionState() !=
          session_manager::SessionState::ACTIVE &&
      Shell::Get()->session_controller()->GetSessionState() !=
          session_manager::SessionState::UNKNOWN) {
    // When this is the case, let events pass through the "empty" part of
    // the shelf.
    return shelf_->shelf_widget()->GetHitTestRects(target, hit_test_rect_mouse,
                                                   hit_test_rect_touch);
  }
  *hit_test_rect_mouse = *hit_test_rect_touch = target->bounds();

  if (ShouldUseExtendedBounds(target)) {
    hit_test_rect_mouse->Inset(mouse_extend());

    // Whether the touch hit area should be extended beyond the window top when
    // the shelf is in auto-hide state (to make targeting hidden shelf easier).
    // This should be applied for shelf widget  only, to prevent other widgets
    // positioned below display bounds (e.g. hidden hotseat widget) from
    // handling touch events instead of the shelf.
    if (target_is_shelf_widget)
      hit_test_rect_touch->Inset(touch_extend());
  }
  return true;
}

void ShelfWindowTargeter::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  shelf_->RemoveObserver(this);
  shelf_ = nullptr;
}

void ShelfWindowTargeter::WillChangeVisibilityState(
    ShelfVisibilityState new_state) {
  // Do not use |new_state| as it can be a shelf on other displays.
  auto visibility_state = shelf_->GetVisibilityState();

  gfx::Insets mouse_insets;
  gfx::Insets touch_insets;
  if (visibility_state == SHELF_VISIBLE) {
    // Let clicks at the very top of the shelf through so windows can be
    // resized with the bottom-right corner and bottom edge.
    mouse_insets = GetInsetsForAlignment(
        ShelfConfig::Get()->workspace_area_visible_inset(),
        shelf_->alignment());
  } else if (visibility_state == SHELF_AUTO_HIDE) {
    // Extend the touch hit target out a bit to allow users to drag shelf out
    // while hidden.
    touch_insets = GetInsetsForAlignment(
        -ShelfConfig::Get()->workspace_area_auto_hide_inset(),
        shelf_->alignment());
  }

  // Remember the insets. See GetHitTestsRects when they're actually used.
  SetInsets(mouse_insets, touch_insets);
}

}  // namespace ash
