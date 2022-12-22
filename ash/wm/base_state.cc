// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/base_state.h"

#include "ash/public/cpp/window_animation_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"

namespace ash {

using ::chromeos::WindowStateType;

BaseState::BaseState(WindowStateType initial_state_type)
    : state_type_(initial_state_type) {}
BaseState::~BaseState() = default;

void BaseState::OnWMEvent(WindowState* window_state, const WMEvent* event) {
  if (event->IsWorkspaceEvent()) {
    HandleWorkspaceEvents(window_state, event);
    if (window_state->IsPip())
      window_state->UpdatePipBounds();
    if (window_state->IsSnapped() && !window_state->CanSnap())
      window_state->Restore();
    return;
  }
  if ((window_state->IsTrustedPinned() || window_state->IsPinned()) &&
      (event->type() != WM_EVENT_NORMAL && event->type() != WM_EVENT_RESTORE &&
       event->IsTransitionEvent())) {
    // PIN state can be exited only by normal event or restore event.
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
WindowStateType BaseState::GetStateForTransitionEvent(WindowState* window_state,
                                                      const WMEvent* event) {
  switch (event->type()) {
    case WM_EVENT_NORMAL:
      if (window_state->window()->GetProperty(aura::client::kIsRestoringKey))
        return window_state->GetRestoreWindowState();
      return WindowStateType::kNormal;
    case WM_EVENT_MAXIMIZE:
      return WindowStateType::kMaximized;
    case WM_EVENT_MINIMIZE:
      return WindowStateType::kMinimized;
    case WM_EVENT_FULLSCREEN:
      return WindowStateType::kFullscreen;
    case WM_EVENT_SNAP_PRIMARY:
      return WindowStateType::kPrimarySnapped;
    case WM_EVENT_SNAP_SECONDARY:
      return WindowStateType::kSecondarySnapped;
    case WM_EVENT_RESTORE:
      return window_state->GetRestoreWindowState();
    case WM_EVENT_SHOW_INACTIVE:
      return WindowStateType::kInactive;
    case WM_EVENT_PIN:
      return WindowStateType::kPinned;
    case WM_EVENT_PIP:
      return WindowStateType::kPip;
    case WM_EVENT_FLOAT:
      return WindowStateType::kFloated;
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
  auto* shell = Shell::Get();
  // For tablet mode, use |TabletModeWindowState::CycleTabletSnap|.
  DCHECK(!shell->tablet_mode_controller()->InTabletMode());

  WindowStateType desired_snap_state = event == WM_EVENT_CYCLE_SNAP_PRIMARY
                                           ? WindowStateType::kPrimarySnapped
                                           : WindowStateType::kSecondarySnapped;
  aura::Window* window = window_state->window();
  // If |window| can be snapped but is not currently in |desired_snap_state|,
  // then snap |window| to the side that corresponds to |desired_snap_state|.
  if (window_state->CanSnap() &&
      window_state->GetStateType() != desired_snap_state) {
    const bool is_desired_primary_snapped =
        desired_snap_state == WindowStateType::kPrimarySnapped;
    if (shell->overview_controller()->InOverviewSession()) {
      // |window| must already be in split view, and so we do not need to check
      // |SplitViewController::CanSnapWindow|, although in general it is more
      // restrictive than |WindowState::CanSnap|.
      DCHECK(SplitViewController::Get(window)->IsWindowInSplitView(window));
      SplitViewController::Get(window)->SnapWindow(
          window, is_desired_primary_snapped
                      ? SplitViewController::SnapPosition::kPrimary
                      : SplitViewController::SnapPosition::kSecondary);
    } else {
      const WMEvent wm_event(is_desired_primary_snapped
                                 ? WM_EVENT_SNAP_PRIMARY
                                 : WM_EVENT_SNAP_SECONDARY);
      window_state->OnWMEvent(&wm_event);
    }
    window_state->ReadOutWindowCycleSnapAction(
        is_desired_primary_snapped ? IDS_WM_SNAP_WINDOW_TO_LEFT_ON_SHORTCUT
                                   : IDS_WM_SNAP_WINDOW_TO_RIGHT_ON_SHORTCUT);
    return;
  }
  // If |window| is already in |desired_snap_state|, then unsnap |window|.
  if (window_state->IsSnapped()) {
    window_state->Restore();
    window_state->ReadOutWindowCycleSnapAction(
        IDS_WM_RESTORE_SNAPPED_WINDOW_ON_SHORTCUT);
    return;
  }
  // If |window| cannot be snapped, then do a window bounce animation.
  DCHECK(!window_state->CanSnap());
  ::wm::AnimateWindow(window, ::wm::WINDOW_ANIMATION_TYPE_BOUNCE);
}

void BaseState::UpdateMinimizedState(WindowState* window_state,
                                     WindowStateType previous_state_type) {
  aura::Window* window = window_state->window();
  if (window_state->IsMinimized()) {
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
  return BaseState::GetSnappedWindowBoundsInParent(window, state_type,
                                                   kDefaultSnapRatio);
}

gfx::Rect BaseState::GetSnappedWindowBoundsInParent(
    aura::Window* window,
    const WindowStateType state_type,
    float snap_ratio) {
  DCHECK(state_type == WindowStateType::kPrimarySnapped ||
         state_type == WindowStateType::kSecondarySnapped);
  gfx::Rect bounds_in_parent;
  if (ShouldAllowSplitView()) {
    bounds_in_parent =
        SplitViewController::Get(window)->GetSnappedWindowBoundsInParent(
            (state_type == WindowStateType::kPrimarySnapped)
                ? SplitViewController::SnapPosition::kPrimary
                : SplitViewController::SnapPosition::kSecondary,
            window, snap_ratio);
  } else {
    // Use `window_positioning_utils` to calculate the snapped window bounds.
    bounds_in_parent = ash::GetSnappedWindowBoundsInParent(
        window,
        state_type == WindowStateType::kPrimarySnapped
            ? SnapViewType::kPrimary
            : SnapViewType::kSecondary,
        snap_ratio);
  }
  return bounds_in_parent;
}

void BaseState::HandleWindowSnapping(WindowState* window_state,
                                     WMEventType event_type) {
  DCHECK(event_type == WM_EVENT_SNAP_PRIMARY ||
         event_type == WM_EVENT_SNAP_SECONDARY);
  DCHECK(window_state->CanSnap());

  window_state->set_bounds_changed_by_user(true);
  aura::Window* window = window_state->window();
  // SplitViewController will decide if the window needs to be snapped in split
  // view.
  SplitViewController::Get(window)->OnWMEvent(window, event_type);
}

}  // namespace ash
