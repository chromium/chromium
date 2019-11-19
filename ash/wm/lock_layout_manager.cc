// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/lock_layout_manager.h"

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/wm/lock_window_state.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/env.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_recognizer.h"

namespace ash {

LockLayoutManager::LockLayoutManager(aura::Window* window, Shelf* shelf)
    : WmDefaultLayoutManager(),
      window_(window),
      root_window_(window->GetRootWindow()) {
  root_window_->AddObserver(this);
  keyboard::KeyboardUIController::Get()->AddObserver(this);
  shelf_observer_.Add(shelf);
}

LockLayoutManager::~LockLayoutManager() {
  keyboard::KeyboardUIController::Get()->RemoveObserver(this);

  if (root_window_)
    root_window_->RemoveObserver(this);

  for (aura::Window* child : window_->children())
    child->RemoveObserver(this);

}

void LockLayoutManager::OnWindowResized() {
  const WMEvent event(WM_EVENT_WORKAREA_BOUNDS_CHANGED);
  AdjustWindowsForWorkAreaChange(&event);
}

void LockLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  child->AddObserver(this);

  // LockWindowState replaces default WindowState of a child.
  WindowState* window_state = LockWindowState::SetLockWindowState(child);
  WMEvent event(WM_EVENT_ADDED_TO_WORKSPACE);
  window_state->OnWMEvent(&event);

  aura::Env::GetInstance()->gesture_recognizer()->CancelActiveTouchesExcept(
      nullptr);

  // Disable virtual keyboard overscroll because it interferes with scrolling
  // login/lock content. See crbug.com/363635.
  keyboard::KeyboardConfig config =
      keyboard::KeyboardUIController::Get()->keyboard_config();
  config.overscroll_behavior = keyboard::KeyboardOverscrollBehavior::kDisabled;
  keyboard::KeyboardUIController::Get()->UpdateKeyboardConfig(config);
}

void LockLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
  child->RemoveObserver(this);
}

void LockLayoutManager::OnWindowRemovedFromLayout(aura::Window* child) {
  keyboard::KeyboardConfig config =
      keyboard::KeyboardUIController::Get()->keyboard_config();
  config.overscroll_behavior = keyboard::KeyboardOverscrollBehavior::kDefault;
  keyboard::KeyboardUIController::Get()->UpdateKeyboardConfig(config);
}

void LockLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                       bool visible) {}

void LockLayoutManager::SetChildBounds(aura::Window* child,
                                       const gfx::Rect& requested_bounds) {
  WindowState* window_state = WindowState::Get(child);
  SetBoundsWMEvent event(requested_bounds);
  window_state->OnWMEvent(&event);
}

void LockLayoutManager::OnWindowDestroying(aura::Window* window) {
  window->RemoveObserver(this);
  if (root_window_ == window)
    root_window_ = nullptr;
}

void LockLayoutManager::OnWindowBoundsChanged(aura::Window* window,
                                              const gfx::Rect& old_bounds,
                                              const gfx::Rect& new_bounds,
                                              ui::PropertyChangeReason reason) {
  if (root_window_ == window) {
    const DisplayMetricsChangedWMEvent wm_event(
        display::DisplayObserver::DISPLAY_METRIC_BOUNDS);
    AdjustWindowsForWorkAreaChange(&wm_event);
  }
}

void LockLayoutManager::WillChangeVisibilityState(
    ShelfVisibilityState visibility) {
  // This will be called when shelf work area changes.
  //  * LockLayoutManager windows depend on changes to the accessibility panel
  //    height.
  //  * LockActionHandlerLayoutManager windows bounds depend on the work area
  //    bound defined by the shelf layout (see
  //    screen_util::GetDisplayWorkAreaBoundsInParentForLockScreen).
  // In short, when shelf bounds change, the windows in this layout manager
  // should be updated, too.
  const WMEvent event(WM_EVENT_WORKAREA_BOUNDS_CHANGED);
  AdjustWindowsForWorkAreaChange(&event);
}

void LockLayoutManager::OnKeyboardOccludedBoundsChanged(
    const gfx::Rect& new_bounds_in_screen) {
  OnWindowResized();
}

void LockLayoutManager::AdjustWindowsForWorkAreaChange(const WMEvent* event) {
  DCHECK(event->type() == WM_EVENT_DISPLAY_BOUNDS_CHANGED ||
         event->type() == WM_EVENT_WORKAREA_BOUNDS_CHANGED);

  for (aura::Window* child : window_->children())
    WindowState::Get(child)->OnWMEvent(event);
}

}  // namespace ash
