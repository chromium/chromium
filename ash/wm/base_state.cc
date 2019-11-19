// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/base_state.h"

#include "ash/public/cpp/window_animation_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace ash {

BaseState::BaseState(WindowStateType initial_state_type)
    : state_type_(initial_state_type) {}
BaseState::~BaseState() = default;

void BaseState::OnWMEvent(WindowState* window_state, const WMEvent* event) {
  if (event->IsWorkspaceEvent()) {
    HandleWorkspaceEvents(window_state, event);
    if (window_state->IsPip())
      window_state->UpdatePipBounds();
    return;
  }
  if ((window_state->IsTrustedPinned() || window_state->IsPinned()) &&
      (event->type() != WM_EVENT_NORMAL && event->IsTransitionEvent())) {
    // PIN state can be exited only by normal event.
    return;
  }

  if (event->IsCompoundEvent()) {
    HandleCompoundEvents(window_state, event);
    return;
  }

  if (event->IsBoundsEvent()) {
    HandleBoundsEvents(window_state, event);
    return;
  }
  DCHECK(event->IsTransitionEvent());
  HandleTransitionEvents(window_state, event);
}

WindowStateType BaseState::GetType() const {
  return state_type_;
}

// static
WindowStateType BaseState::GetStateForTransitionEvent(const WMEvent* event) {
  switch (event->type()) {
    case WM_EVENT_NORMAL:
      return WindowStateType::kNormal;
    case WM_EVENT_MAXIMIZE:
      return WindowStateType::kMaximized;
    case WM_EVENT_MINIMIZE:
      return WindowStateType::kMinimized;
    case WM_EVENT_FULLSCREEN:
      return WindowStateType::kFullscreen;
    case WM_EVENT_SNAP_LEFT:
      return WindowStateType::kLeftSnapped;
    case WM_EVENT_SNAP_RIGHT:
      return WindowStateType::kRightSnapped;
    case WM_EVENT_SHOW_INACTIVE:
      return WindowStateType::kInactive;
    case WM_EVENT_PIN:
      return WindowStateType::kPinned;
    case WM_EVENT_PIP:
      return WindowStateType::kPip;
    case WM_EVENT_TRUSTED_PIN:
      return WindowStateType::kTrustedPinned;
    default:
      break;
  }
#if !defined(NDEBUG)
  if (event->IsWorkspaceEvent())
    NOTREACHED() << "Can't get the state for Workspace event" << event->type();
  if (event->IsCompoundEvent())
    NOTREACHED() << "Can't get the state for Compound event:" << event->type();
  if (event->IsBoundsEvent())
    NOTREACHED() << "Can't get the state for Bounds event:" << event->type();
#endif
  return WindowStateType::kNormal;
}

// static
void BaseState::CenterWindow(WindowState* window_state) {
  if (!window_state->IsNormalOrSnapped())
    return;
  aura::Window* window = window_state->window();
  if (window_state->IsSnapped()) {
    gfx::Rect center_in_screen = display::Screen::GetScreen()
                                     ->GetDisplayNearestWindow(window)
                                     .work_area();
    gfx::Size size = window_state->HasRestoreBounds()
                         ? window_state->GetRestoreBoundsInScreen().size()
                         : window->bounds().size();
    center_in_screen.ClampToCenteredSize(size);
    window_state->SetRestoreBoundsInScreen(center_in_screen);
    window_state->Restore();
  } else {
    gfx::Rect center_in_parent =
        screen_util::GetDisplayWorkAreaBoundsInParent(window);
    center_in_parent.ClampToCenteredSize(window->bounds().size());
    const SetBoundsWMEvent event(center_in_parent,
                                 /*animate=*/true);
    window_state->OnWMEvent(&event);
  }
  // Centering window is treated as if a user moved and resized the window.
  window_state->set_bounds_changed_by_user(true);
}

// static
void BaseState::CycleSnap(WindowState* window_state, WMEventType event) {
  WindowStateType desired_snap_state = event == WM_EVENT_CYCLE_SNAP_LEFT
                                           ? WindowStateType::kLeftSnapped
                                           : WindowStateType::kRightSnapped;

  if (window_state->CanSnap() &&
      window_state->GetStateType() != desired_snap_state) {
    const WMEvent event(desired_snap_state == WindowStateType::kLeftSnapped
                            ? WM_EVENT_SNAP_LEFT
                            : WM_EVENT_SNAP_RIGHT);
    window_state->OnWMEvent(&event);
    return;
  }

  if (window_state->IsSnapped()) {
    window_state->Restore();
    return;
  }
  ::wm::AnimateWindow(window_state->window(),
                      ::wm::WINDOW_ANIMATION_TYPE_BOUNCE);
}

void BaseState::UpdateMinimizedState(WindowState* window_state,
                                     WindowStateType previous_state_type) {
  aura::Window* window = window_state->window();
  if (window_state->IsMinimized()) {
    // Save the previous show state when it is not minimized so that we can
    // correctly restore it after exiting the minimized mode.
    if (!IsMinimizedWindowStateType(previous_state_type)) {
      // We must not save PIP to |kPreMinimizedShowStateKey|.
      if (previous_state_type != WindowStateType::kPip)
        window->SetProperty(aura::client::kPreMinimizedShowStateKey,
                            ToWindowShowState(previous_state_type));
      // We must not save MINIMIZED to |kPreMinimizedShowStateKey|.
      else if (window->GetProperty(ash::kPrePipWindowStateTypeKey) !=
               WindowStateType::kMinimized)
        window->SetProperty(aura::client::kPreMinimizedShowStateKey,
                            ToWindowShowState(window->GetProperty(
                                ash::kPrePipWindowStateTypeKey)));
    }
    // Count minimizing a PIP window as dismissing it. Android apps in PIP mode
    // don't exit when they are dismissed, they just go back to being a regular
    // app, but minimized.
    ::wm::SetWindowVisibilityAnimationType(
        window, previous_state_type == WindowStateType::kPip
                    ? WINDOW_VISIBILITY_ANIMATION_TYPE_FADE_IN_SLIDE_OUT
                    : WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE);

    window->Hide();
    if (window_state->IsActive())
      window_state->Deactivate();
  } else if ((window->layer()->GetTargetVisibility() ||
              IsMinimizedWindowStateType(previous_state_type)) &&
             !window->layer()->visible()) {
    // The layer may be hidden if the window was previously minimized. Make
    // sure it's visible.
    window->Show();
    if (IsMinimizedWindowStateType(previous_state_type) &&
        !window_state->IsMaximizedOrFullscreenOrPinned()) {
      window_state->set_unminimize_to_restore_bounds(false);
    }
  }
}

gfx::Rect BaseState::GetSnappedWindowBoundsInParent(
    aura::Window* window,
    const WindowStateType state_type) {
  gfx::Rect bounds_in_parent;
  if (ShouldAllowSplitView()) {
    bounds_in_parent =
        SplitViewController::Get(window)->GetSnappedWindowBoundsInParent(
            (state_type == WindowStateType::kLeftSnapped)
                ? SplitViewController::LEFT
                : SplitViewController::RIGHT,
            window);
  } else {
    bounds_in_parent = (state_type == WindowStateType::kLeftSnapped)
                           ? GetDefaultLeftSnappedWindowBoundsInParent(window)
                           : GetDefaultRightSnappedWindowBoundsInParent(window);
  }
  return bounds_in_parent;
}

}  // namespace ash
