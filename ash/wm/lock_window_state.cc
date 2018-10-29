// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/lock_window_state.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/window_animation_types.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/wm/lock_layout_manager.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_state_util.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

LockWindowState::LockWindowState(aura::Window* window, bool exclude_shelf)
    : current_state_type_(wm::GetWindowState(window)->GetStateType()),
      exclude_shelf_(exclude_shelf) {}

LockWindowState::~LockWindowState() = default;

void LockWindowState::OnWMEvent(wm::WindowState* window_state,
                                const wm::WMEvent* event) {
  switch (event->type()) {
    case wm::WM_EVENT_TOGGLE_FULLSCREEN:
      ToggleFullScreen(window_state, window_state->delegate());
      break;
    case wm::WM_EVENT_FULLSCREEN:
      UpdateWindow(window_state, mojom::WindowStateType::FULLSCREEN);
      break;
    case wm::WM_EVENT_PIN:
    case wm::WM_EVENT_TRUSTED_PIN:
      NOTREACHED();
      break;
    case wm::WM_EVENT_TOGGLE_MAXIMIZE_CAPTION:
    case wm::WM_EVENT_TOGGLE_VERTICAL_MAXIMIZE:
    case wm::WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE:
    case wm::WM_EVENT_TOGGLE_MAXIMIZE:
    case wm::WM_EVENT_CYCLE_SNAP_LEFT:
    case wm::WM_EVENT_CYCLE_SNAP_RIGHT:
    case wm::WM_EVENT_CENTER:
    case wm::WM_EVENT_SNAP_LEFT:
    case wm::WM_EVENT_SNAP_RIGHT:
    case wm::WM_EVENT_NORMAL:
    case wm::WM_EVENT_MAXIMIZE:
      UpdateWindow(window_state,
                   GetMaximizedOrCenteredWindowType(window_state));
      return;
    case wm::WM_EVENT_MINIMIZE:
      UpdateWindow(window_state, mojom::WindowStateType::MINIMIZED);
      return;
    case wm::WM_EVENT_SHOW_INACTIVE:
      return;
    case wm::WM_EVENT_SET_BOUNDS:
      if (window_state->IsMaximized() || window_state->IsFullscreen()) {
        UpdateBounds(window_state);
      } else {
        const ash::wm::SetBoundsEvent* bounds_event =
            static_cast<const ash::wm::SetBoundsEvent*>(event);
        window_state->SetBoundsConstrained(bounds_event->requested_bounds());
      }
      break;
    case wm::WM_EVENT_ADDED_TO_WORKSPACE:
      if (current_state_type_ != mojom::WindowStateType::MAXIMIZED &&
          current_state_type_ != mojom::WindowStateType::MINIMIZED &&
          current_state_type_ != mojom::WindowStateType::FULLSCREEN) {
        UpdateWindow(window_state,
                     GetMaximizedOrCenteredWindowType(window_state));
      } else {
        UpdateBounds(window_state);
      }
      break;
    case wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED:
    case wm::WM_EVENT_DISPLAY_BOUNDS_CHANGED:
      UpdateBounds(window_state);
      break;
    case wm::WM_EVENT_SYSTEM_UI_AREA_CHANGED:
      return;
  }
}

mojom::WindowStateType LockWindowState::GetType() const {
  return current_state_type_;
}

void LockWindowState::AttachState(wm::WindowState* window_state,
                                  wm::WindowState::State* previous_state) {
  current_state_type_ = previous_state->GetType();

  // Initialize the state to a good preset.
  if (current_state_type_ != mojom::WindowStateType::MAXIMIZED &&
      current_state_type_ != mojom::WindowStateType::MINIMIZED &&
      current_state_type_ != mojom::WindowStateType::FULLSCREEN) {
    UpdateWindow(window_state, GetMaximizedOrCenteredWindowType(window_state));
  }
}

void LockWindowState::DetachState(wm::WindowState* window_state) {}

// static
wm::WindowState* LockWindowState::SetLockWindowState(aura::Window* window) {
  std::unique_ptr<wm::WindowState::State> lock_state =
      std::make_unique<LockWindowState>(window, false);
  wm::WindowState* window_state = wm::GetWindowState(window);
  std::unique_ptr<wm::WindowState::State> old_state(
      window_state->SetStateObject(std::move(lock_state)));
  return window_state;
}

// static
wm::WindowState* LockWindowState::SetLockWindowStateWithShelfExcluded(
    aura::Window* window) {
  std::unique_ptr<wm::WindowState::State> lock_state =
      std::make_unique<LockWindowState>(window, true);
  wm::WindowState* window_state = wm::GetWindowState(window);
  std::unique_ptr<wm::WindowState::State> old_state(
      window_state->SetStateObject(std::move(lock_state)));
  return window_state;
}

void LockWindowState::UpdateWindow(wm::WindowState* window_state,
                                   mojom::WindowStateType target_state) {
  DCHECK(target_state == mojom::WindowStateType::MINIMIZED ||
         target_state == mojom::WindowStateType::MAXIMIZED ||
         (target_state == mojom::WindowStateType::NORMAL &&
          !window_state->CanMaximize()) ||
         target_state == mojom::WindowStateType::FULLSCREEN);

  if (target_state == mojom::WindowStateType::MINIMIZED) {
    if (current_state_type_ == mojom::WindowStateType::MINIMIZED)
      return;

    current_state_type_ = target_state;
    ::wm::SetWindowVisibilityAnimationType(
        window_state->window(), wm::WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE);
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

  const mojom::WindowStateType old_state_type = current_state_type_;
  current_state_type_ = target_state;
  window_state->UpdateWindowPropertiesFromStateType();
  window_state->NotifyPreStateTypeChange(old_state_type);
  UpdateBounds(window_state);
  window_state->NotifyPostStateTypeChange(old_state_type);

  if ((window_state->window()->TargetVisibility() ||
       old_state_type == mojom::WindowStateType::MINIMIZED) &&
      !window_state->window()->layer()->visible()) {
    // The layer may be hidden if the window was previously minimized. Make
    // sure it's visible.
    window_state->window()->Show();
  }
}

mojom::WindowStateType LockWindowState::GetMaximizedOrCenteredWindowType(
    wm::WindowState* window_state) {
  return window_state->CanMaximize() ? mojom::WindowStateType::MAXIMIZED
                                     : mojom::WindowStateType::NORMAL;
}

gfx::Rect LockWindowState::GetWindowBounds(aura::Window* window) {
  if (exclude_shelf_)
    return screen_util::GetDisplayWorkAreaBoundsInParentForLockScreen(window);

  auto* keyboard_controller = keyboard::KeyboardController::Get();
  const int keyboard_height =
      keyboard_controller->IsEnabled()
          ? keyboard_controller->GetKeyboardLockScreenOffsetBounds().height()
          : 0;
  gfx::Rect bounds = screen_util::GetDisplayBoundsWithShelf(window);
  bounds.Inset(0, Shelf::ForWindow(window)->GetAccessibilityPanelHeight(), 0,
               keyboard_height);
  return bounds;
}

void LockWindowState::UpdateBounds(wm::WindowState* window_state) {
  if (!window_state->IsMaximized() && !window_state->IsFullscreen())
    return;

  gfx::Rect bounds = GetWindowBounds(window_state->window());
  VLOG(1) << "Updating window bounds to: " << bounds.ToString();
  window_state->SetBoundsDirect(bounds);
}

}  // namespace ash
