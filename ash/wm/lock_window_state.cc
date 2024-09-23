// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/lock_window_state.h"

#include <memory>
#include <utility>

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/window_animation_types.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/lock_layout_manager.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_state_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/work_area_insets.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

using ::chromeos::WindowStateType;

LockWindowState::LockWindowState(aura::Window* window, bool exclude_shelf)
    : current_state_type_(WindowState::Get(window)->GetStateType()),
      exclude_shelf_(exclude_shelf) {}

LockWindowState::~LockWindowState() = default;

void LockWindowState::OnWMEvent(WindowState* window_state,
                                const WMEvent* event) {
  switch (event->type()) {
    case WM_EVENT_TOGGLE_FULLSCREEN:
      ToggleFullScreen(window_state, window_state->delegate());
      break;
    case WM_EVENT_FULLSCREEN:
      UpdateWindow(window_state, WindowStateType::kFullscreen);
      break;
    case WM_EVENT_PIP:
    case WM_EVENT_FLOAT:
    case WM_EVENT_PIN:
    case WM_EVENT_TRUSTED_PIN:
      NOTREACHED();
    case WM_EVENT_TOGGLE_MAXIMIZE_CAPTION:
    case WM_EVENT_TOGGLE_VERTICAL_MAXIMIZE:
    case WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE:
    case WM_EVENT_TOGGLE_MAXIMIZE:
    case WM_EVENT_CYCLE_SNAP_PRIMARY:
    case WM_EVENT_CYCLE_SNAP_SECONDARY:
    case WM_EVENT_SNAP_PRIMARY:
    case WM_EVENT_SNAP_SECONDARY:
    case WM_EVENT_NORMAL:
    case WM_EVENT_RESTORE:
    case WM_EVENT_MAXIMIZE:
      UpdateWindow(window_state, GetWindowTypeOnMaximizable(window_state));
      return;
    case WM_EVENT_MINIMIZE:
      UpdateWindow(window_state, WindowStateType::kMinimized);
      return;
    case WM_EVENT_SHOW_INACTIVE:
      return;
    case WM_EVENT_SET_BOUNDS:
      if (window_state->IsMaximized() || window_state->IsFullscreen()) {
        UpdateBounds(window_state);
      } else {
        window_state->SetBoundsConstrained(
            event->AsSetBoundsWMEvent()->requested_bounds_in_parent());
      }
      break;
    case WM_EVENT_ADDED_TO_WORKSPACE:
      if (current_state_type_ != WindowStateType::kMaximized &&
          current_state_type_ != WindowStateType::kMinimized &&
          current_state_type_ != WindowStateType::kFullscreen) {
        UpdateWindow(window_state, GetWindowTypeOnMaximizable(window_state));
      } else {
        UpdateBounds(window_state);
      }
      break;
    case WM_EVENT_DISPLAY_METRICS_CHANGED:
      UpdateBounds(window_state);
      break;
  }
}

WindowStateType LockWindowState::GetType() const {
  return current_state_type_;
}

void LockWindowState::AttachState(WindowState* window_state,
                                  WindowState::State* previous_state) {
  current_state_type_ = previous_state->GetType();

  // Initialize the state to a good preset.
  if (current_state_type_ != WindowStateType::kMaximized &&
      current_state_type_ != WindowStateType::kMinimized &&
      current_state_type_ != WindowStateType::kFullscreen) {
    UpdateWindow(window_state, GetWindowTypeOnMaximizable(window_state));
  }
}

void LockWindowState::DetachState(WindowState* window_state) {}

// static
WindowState* LockWindowState::SetLockWindowState(aura::Window* window,
                                                 bool shelf_excluded) {
  std::unique_ptr<WindowState::State> lock_state =
      std::make_unique<LockWindowState>(window, shelf_excluded);
  WindowState* window_state = WindowState::Get(window);
  std::unique_ptr<WindowState::State> old_state(
      window_state->SetStateObject(std::move(lock_state)));
  return window_state;
}

void LockWindowState::UpdateWindow(WindowState* window_state,
                                   WindowStateType target_state) {
  DCHECK(target_state == WindowStateType::kMinimized ||
         target_state == WindowStateType::kMaximized ||
         (target_state == WindowStateType::kNormal &&
          !window_state->CanMaximize()) ||
         target_state == WindowStateType::kFullscreen);

  if (target_state == WindowStateType::kMinimized) {
    if (current_state_type_ == WindowStateType::kMinimized)
      return;

    current_state_type_ = target_state;
    ::wm::SetWindowVisibilityAnimationType(
        window_state->window(), WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE);
    window_state->window()->Hide();
    if (window_state->IsActive())
      window_state->Deactivate();
    return;
  }

  if (current_state_type_ == target_state) {
    // If the state type did not change, update it accordingly.
    UpdateBounds(window_state);
    return;
  }

  const WindowStateType old_state_type = current_state_type_;
  current_state_type_ = target_state;
  window_state->UpdateWindowPropertiesFromStateType();
  window_state->NotifyPreStateTypeChange(old_state_type);
  UpdateBounds(window_state);
  window_state->NotifyPostStateTypeChange(old_state_type);

  if ((window_state->window()->TargetVisibility() ||
       old_state_type == WindowStateType::kMinimized) &&
      !window_state->window()->layer()->visible()) {
    // The layer may be hidden if the window was previously minimized. Make
    // sure it's visible.
    window_state->window()->Show();
  }
}

WindowStateType LockWindowState::GetWindowTypeOnMaximizable(
    WindowState* window_state) const {
  return window_state->CanMaximize() ? WindowStateType::kMaximized
                                     : WindowStateType::kNormal;
}

gfx::Rect LockWindowState::GetWindowBounds(aura::Window* window) {
  if (exclude_shelf_)
    return screen_util::GetDisplayWorkAreaBoundsInParentForLockScreen(window);

  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  const int keyboard_height =
      keyboard_controller->IsEnabled()
          ? keyboard_controller->GetKeyboardLockScreenOffsetBounds().height()
          : 0;
  gfx::Rect bounds = screen_util::GetDisplayBoundsWithShelf(window);
  gfx::Insets insets(WorkAreaInsets::ForWindow(window->GetRootWindow())
                         ->GetAccessibilityInsets());

  if (keyboard_height > 0)
    insets.set_bottom(keyboard_height);

  bounds.Inset(insets);
  return bounds;
}

void LockWindowState::UpdateBounds(WindowState* window_state) {
  if (!window_state->IsMaximized() && !window_state->IsFullscreen())
    return;

  gfx::Rect bounds = GetWindowBounds(window_state->window());
  VLOG(1) << "Updating window bounds to: " << bounds.ToString();
  window_state->SetBoundsDirect(bounds);
}

}  // namespace ash
