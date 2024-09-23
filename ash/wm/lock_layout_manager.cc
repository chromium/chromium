// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/lock_layout_manager.h"

#include "ash/wm/lock_window_state.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/env.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event.h"
#include "ui/events/gestures/gesture_recognizer.h"

namespace ash {

LockLayoutManager::LockLayoutManager(aura::Window* window)
    : window_(window), root_window_(window->GetRootWindow()) {
  root_window_->AddObserver(this);
  keyboard::KeyboardUIController::Get()->AddObserver(this);
}

LockLayoutManager::~LockLayoutManager() {
  keyboard::KeyboardUIController::Get()->RemoveObserver(this);

  if (root_window_)
    root_window_->RemoveObserver(this);

  for (aura::Window* child : window_->children())
    child->RemoveObserver(this);
}

void LockLayoutManager::OnWindowResized() {
  const DisplayMetricsChangedWMEvent event(
      display::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
  AdjustWindowsForWorkAreaChange(&event);
}

void LockLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  child->AddObserver(this);

  // LockWindowState replaces default WindowState of a child.
  WindowState* window_state =
      LockWindowState::SetLockWindowState(child, /*shelf_excluded=*/false);
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

void LockLayoutManager::OnDisplayMetricsChanged(const display::Display& display,
                                                uint32_t changed_metrics) {
  if (!root_window_) {
    return;
  }

  if (display::Screen::GetScreen()
          ->GetDisplayNearestWindow(root_window_)
          .id() != display.id()) {
    return;
  }

  if (changed_metrics & display::DisplayObserver::DISPLAY_METRIC_WORK_AREA) {
    const DisplayMetricsChangedWMEvent event(
        display::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
    AdjustWindowsForWorkAreaChange(&event);
  }
}

void LockLayoutManager::OnKeyboardOccludedBoundsChanged(
    const gfx::Rect& new_bounds_in_screen) {
  OnWindowResized();
}

void LockLayoutManager::AdjustWindowsForWorkAreaChange(const WMEvent* event) {
  const DisplayMetricsChangedWMEvent* display_event =
      event->AsDisplayMetricsChangedWMEvent();
  CHECK(display_event->display_bounds_changed() ||
        display_event->work_area_changed());

  for (aura::Window* child : window_->children())
    WindowState::Get(child)->OnWMEvent(event);
}

}  // namespace ash
