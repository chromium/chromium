// Copyright 2016 The Chromium Authors
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
  if (alignment == ShelfAlignment::kLeft) {
    return gfx::Insets::TLBR(0, 0, 0, distance);
  }

  if (alignment == ShelfAlignment::kRight) {
    return gfx::Insets::TLBR(0, distance, 0, 0);
  }

  return gfx::Insets::TLBR(distance, 0, 0, 0);
}

}  // namespace

ShelfWindowTargeter::ShelfWindowTargeter(aura::Window* container, Shelf* shelf)
    : ::wm::EasyResizeWindowTargeter(gfx::Insets(), gfx::Insets()),
      shelf_(shelf) {
  UpdateInsetsForVisibilityState(shelf_->GetVisibilityState());
  container->AddObserver(this);
  shelf_->AddObserver(this);
  Shell::Get()->AddShellObserver(this);
}

ShelfWindowTargeter::~ShelfWindowTargeter() {
  // Ensure that the observers were removed and the shelf pointer was cleared.
  DCHECK(!shelf_);

  Shell::Get()->RemoveShellObserver(this);
}

bool ShelfWindowTargeter::ShouldUseExtendedBounds(const aura::Window* w) const {
  // Use extended bounds only for direct child of the container.
  return window() == w->parent();
}

bool ShelfWindowTargeter::GetHitTestRects(
    aura::Window* target,
    gfx::Rect* hit_test_rect_mouse,
    gfx::Rect* hit_test_rect_touch) const {
  bool target_is_shelf_widget =
      target == shelf_->shelf_widget()->GetNativeWindow();
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

void ShelfWindowTargeter::OnShelfAlignmentChanged(
    aura::Window* root_window,
    ShelfAlignment old_alignment) {
  if (!shelf_) {
    return;
  }
  if (shelf_->shelf_widget()->GetNativeWindow()->GetRootWindow() !=
      root_window) {
    return;
  }

  UpdateInsets();
}

void ShelfWindowTargeter::OnShelfVisibilityStateChanged(
    ShelfVisibilityState new_state) {
  UpdateInsetsForVisibilityState(new_state);
}

void ShelfWindowTargeter::UpdateInsetsForVisibilityState(
    ShelfVisibilityState state) {
  mouse_inset_size_for_shelf_visibility_ =
      state == SHELF_VISIBLE
          ? ShelfConfig::Get()->workspace_area_visible_inset()
          : 0;
  touch_inset_size_for_shelf_visibility_ =
      state == SHELF_AUTO_HIDE
          ? -ShelfConfig::Get()->workspace_area_auto_hide_inset()
          : 0;

  UpdateInsets();
}

void ShelfWindowTargeter::UpdateInsets() {
  const gfx::Insets mouse_insets = GetInsetsForAlignment(
      mouse_inset_size_for_shelf_visibility_, shelf_->alignment());
  const gfx::Insets touch_insets = GetInsetsForAlignment(
      touch_inset_size_for_shelf_visibility_, shelf_->alignment());

  // Remember the insets. See GetHitTestsRects when they're actually used.
  SetInsets(mouse_insets, touch_insets);
}

}  // namespace ash
