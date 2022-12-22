// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/client_controlled_state.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_animation_types.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_state_util.h"
#include "ash/wm/wm_event.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

using ::chromeos::WindowStateType;

// |kMinimumOnScreenArea + 1| is used to avoid adjusting loop.
constexpr int kClientControlledWindowMinimumOnScreenArea =
    kMinimumOnScreenArea + 1;
}  // namespace

// static
void ClientControlledState::AdjustBoundsForMinimumWindowVisibility(
    const gfx::Rect& display_bounds,
    gfx::Rect* bounds) {
  AdjustBoundsToEnsureWindowVisibility(
      display_bounds, kClientControlledWindowMinimumOnScreenArea,
      kClientControlledWindowMinimumOnScreenArea, bounds);
}

ClientControlledState::ClientControlledState(std::unique_ptr<Delegate> delegate)
    : BaseState(WindowStateType::kDefault), delegate_(std::move(delegate)) {}

ClientControlledState::~ClientControlledState() = default;

void ClientControlledState::ResetDelegate() {
  delegate_.reset();
}

void ClientControlledState::HandleTransitionEvents(WindowState* window_state,
                                                   const WMEvent* event) {
  if (!delegate_)
    return;

  const WMEventType event_type = event->type();
  bool pin_transition = window_state->IsTrustedPinned() ||
                        window_state->IsPinned() || event->IsPinEvent();
  // Pinned State transition is handled on server side.
  if (pin_transition) {
    // Only one window can be pinned.
    if (event->IsPinEvent() &&
        Shell::Get()->screen_pinning_controller()->IsPinned()) {
      return;
    }
    WindowStateType next_state_type =
        GetStateForTransitionEvent(window_state, event);
    delegate_->HandleWindowStateRequest(window_state, next_state_type);
    return;
  }

  switch (event_type) {
    case WM_EVENT_NORMAL:
    case WM_EVENT_MAXIMIZE:
    case WM_EVENT_MINIMIZE:
    case WM_EVENT_FULLSCREEN:
    case WM_EVENT_SNAP_PRIMARY:
    case WM_EVENT_SNAP_SECONDARY: {
      WindowStateType next_state =
          GetResolvedNextWindowStateType(window_state, event);
      UpdateWindowForTransitionEvents(window_state, next_state, event);
      break;
    }
    case WM_EVENT_FLOAT:
      // TODO(crbug.com/1346061): Implement this.
      break;
    case WM_EVENT_RESTORE:
      UpdateWindowForTransitionEvents(
          window_state, window_state->GetRestoreWindowState(), event);
      break;
    case WM_EVENT_SHOW_INACTIVE:
      NOTREACHED();
      break;
    default:
      NOTREACHED() << "Unknown event :" << event->type();
  }
}

void ClientControlledState::AttachState(
    WindowState* window_state,
    WindowState::State* state_in_previous_mode) {}

void ClientControlledState::DetachState(WindowState* window_state) {}

void ClientControlledState::HandleWorkspaceEvents(WindowState* window_state,
                                                  const WMEvent* event) {
  if (!delegate_)
    return;
  // Client is responsible for adjusting bounds after workspace bounds change.
  if (window_state->IsSnapped()) {
    gfx::Rect bounds = GetSnappedWindowBoundsInParent(
        window_state->window(), window_state->GetStateType());
    // Then ask delegate to set the desired bounds for the snap state.
    delegate_->HandleBoundsRequest(window_state, window_state->GetStateType(),
                                   bounds, window_state->GetDisplay().id());
  } else if (event->type() == WM_EVENT_DISPLAY_BOUNDS_CHANGED) {
    // Explicitly handle the primary change because it can change the display id
    // with no bounds change.
    if (event->AsDisplayMetricsChangedWMEvent()->primary_changed()) {
      const gfx::Rect bounds = window_state->window()->bounds();
      delegate_->HandleBoundsRequest(window_state, window_state->GetStateType(),
                                     bounds, window_state->GetDisplay().id());
    }
  } else if (event->type() == WM_EVENT_ADDED_TO_WORKSPACE) {
    aura::Window* window = window_state->window();
    gfx::Rect bounds = window->bounds();
    AdjustBoundsForMinimumWindowVisibility(window->GetRootWindow()->bounds(),
                                           &bounds);

    if (window->bounds() != bounds)
      window_state->SetBoundsConstrained(bounds);
  }
}

void ClientControlledState::HandleCompoundEvents(WindowState* window_state,
                                                 const WMEvent* event) {
  if (!delegate_)
    return;
  switch (event->type()) {
    case WM_EVENT_TOGGLE_MAXIMIZE_CAPTION:
      if (window_state->IsFullscreen()) {
        const WMEvent wm_event(WM_EVENT_TOGGLE_FULLSCREEN);
        window_state->OnWMEvent(&wm_event);
      } else if (window_state->IsMaximized()) {
        window_state->Restore();
      } else if (window_state->IsNormalOrSnapped()) {
        if (window_state->CanMaximize())
          window_state->Maximize();
      }
      break;
    case WM_EVENT_TOGGLE_MAXIMIZE:
      if (window_state->IsFullscreen()) {
        const WMEvent wm_event(WM_EVENT_TOGGLE_FULLSCREEN);
        window_state->OnWMEvent(&wm_event);
      } else if (window_state->IsMaximized()) {
        window_state->Restore();
      } else if (window_state->CanMaximize()) {
        window_state->Maximize();
      }
      break;
    case WM_EVENT_TOGGLE_VERTICAL_MAXIMIZE:
      // TODO(oshima): Implement this.
      break;
    case WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE:
      // TODO(oshima): Implement this.
      break;
    case WM_EVENT_TOGGLE_FULLSCREEN:
      ToggleFullScreen(window_state, window_state->delegate());
      break;
    case WM_EVENT_CYCLE_SNAP_PRIMARY:
    case WM_EVENT_CYCLE_SNAP_SECONDARY:
      CycleSnap(window_state, event->type());
      break;
    default:
      NOTREACHED() << "Invalid event :" << event->type();
      break;
  }
}

void ClientControlledState::HandleBoundsEvents(WindowState* window_state,
                                               const WMEvent* event) {
  if (!delegate_)
    return;
  switch (event->type()) {
    case WM_EVENT_SET_BOUNDS: {
      const auto* set_bounds_event =
          static_cast<const SetBoundsWMEvent*>(event);
      const gfx::Rect& bounds = set_bounds_event->requested_bounds();
      if (set_bounds_locally_) {
        switch (next_bounds_change_animation_type_) {
          case WindowState::BoundsChangeAnimationType::kNone:
            window_state->SetBoundsDirect(bounds);
            break;
          case WindowState::BoundsChangeAnimationType::kCrossFade:
            window_state->SetBoundsDirectCrossFade(bounds);
            break;
          case WindowState::BoundsChangeAnimationType::kAnimate:
            window_state->SetBoundsDirectAnimated(
                bounds, bounds_change_animation_duration_);
            break;
          case WindowState::BoundsChangeAnimationType::kAnimateZero:
            NOTREACHED();
            break;
        }
        next_bounds_change_animation_type_ =
            WindowState::BoundsChangeAnimationType::kNone;
      } else if (!window_state->IsPinned()) {
        // TODO(oshima): Define behavior for pinned app.
        bounds_change_animation_duration_ = set_bounds_event->duration();
        int64_t display_id = set_bounds_event->display_id();
        auto* window = window_state->window();
        if (display_id == display::kInvalidDisplayId) {
          display_id = display::Screen::GetScreen()
                           ->GetDisplayNearestWindow(window)
                           .id();
        }
#if DCHECK_IS_ON()
        gfx::Rect bounds_in_display(bounds);
        // The coordinates of the WindowState's parent must be same as display
        // coordinates. The following code is only to verify this condition.
        const aura::Window* root = window->GetRootWindow();
        aura::Window::ConvertRectToTarget(window->parent(), root,
                                          &bounds_in_display);
        DCHECK_EQ(bounds_in_display.x(), bounds.x());
        DCHECK_EQ(bounds_in_display.y(), bounds.y());
#endif
        delegate_->HandleBoundsRequest(
            window_state, window_state->GetStateType(), bounds, display_id);
      }
      break;
    }
    case WM_EVENT_CENTER:
      CenterWindow(window_state);
      break;
    default:
      NOTREACHED() << "Unknown event:" << event->type();
  }
}

void ClientControlledState::OnWindowDestroying(WindowState* window_state) {
  ResetDelegate();
}

bool ClientControlledState::EnterNextState(WindowState* window_state,
                                           WindowStateType next_state_type) {
  // Do nothing if  we're already in the same state, or delegate has already
  // been deleted.
  if (state_type_ == next_state_type || !delegate_)
    return false;
  WindowStateType previous_state_type = state_type_;
  state_type_ = next_state_type;

  window_state->UpdateWindowPropertiesFromStateType();
  window_state->NotifyPreStateTypeChange(previous_state_type);

  // Don't update the window if the window is detached from parent.
  // This can happen during dragging.
  // TODO(oshima): This was added for DOCKED windows. Investigate if
  // we still need this.
  if (window_state->window()->parent())
    UpdateMinimizedState(window_state, previous_state_type);

  window_state->NotifyPostStateTypeChange(previous_state_type);

  if (IsPinnedWindowStateType(next_state_type) ||
      IsPinnedWindowStateType(previous_state_type)) {
    set_next_bounds_change_animation_type(
        WindowState::BoundsChangeAnimationType::kCrossFade);
    Shell::Get()->screen_pinning_controller()->SetPinnedWindow(
        window_state->window());
  }
  return true;
}

WindowStateType ClientControlledState::GetResolvedNextWindowStateType(
    WindowState* window_state,
    const WMEvent* event) {
  DCHECK(event->IsTransitionEvent());

  const WindowStateType next = GetStateForTransitionEvent(window_state, event);

  if (Shell::Get()->tablet_mode_controller()->InTabletMode() &&
      next == WindowStateType::kNormal && window_state->CanMaximize())
    return WindowStateType::kMaximized;

  return next;
}

void ClientControlledState::UpdateWindowForTransitionEvents(
    WindowState* window_state,
    chromeos::WindowStateType next_state_type,
    const WMEvent* event) {
  const WMEventType event_type = event->type();
  aura::Window* window = window_state->window();

  if (next_state_type == WindowStateType::kPrimarySnapped ||
      next_state_type == WindowStateType::kSecondarySnapped) {
    if (window_state->CanSnap()) {
      HandleWindowSnapping(window_state,
                           next_state_type == WindowStateType::kPrimarySnapped
                               ? WM_EVENT_SNAP_PRIMARY
                               : WM_EVENT_SNAP_SECONDARY);

      if (event_type == WM_EVENT_RESTORE) {
        window_state->set_snap_action_source(
            WindowSnapActionSource::kSnapByWindowStateRestore);
      }
      window_state->RecordAndResetWindowSnapActionSource(
          window_state->GetStateType(), next_state_type);

      // Get the desired window bounds for the snap state.
      const bool is_restoring =
          window_state->window()->GetProperty(aura::client::kIsRestoringKey) ||
          event_type == WM_EVENT_RESTORE;
      // TODO(b/246683799): Investigate why window_state->snap_ratio() can be
      // empty.
      // Use the saved `window_state->snap_ratio()` if restoring, otherwise use
      // the event requested snap ratio, which has a default value.
      float next_snap_ratio;
      if (is_restoring) {
        next_snap_ratio =
            window_state->snap_ratio().value_or(chromeos::kDefaultSnapRatio);
      } else {
        DCHECK(event->IsSnapEvent());
        next_snap_ratio = event->snap_ratio();
      }
      gfx::Rect bounds = GetSnappedWindowBoundsInParent(window, next_state_type,
                                                        next_snap_ratio);
      // We don't want Unminimize() to restore the pre-snapped state during the
      // transition. See crbug.com/1031313 for why we need this.
      // kRestoreShowStateKey property will be updated properly after the window
      // is snapped correctly.
      if (window_state->IsMinimized())
        window->ClearProperty(aura::client::kRestoreShowStateKey);

      window_state->UpdateWindowPropertiesFromStateType();
      VLOG(1) << "Processing State Transtion: event=" << event_type
              << ", state=" << state_type_
              << ", next_state=" << next_state_type;

      // Then ask delegate to set the desired bounds for the snap state.
      delegate_->HandleBoundsRequest(window_state, next_state_type, bounds,
                                     window_state->GetDisplay().id());
    }
  } else {
    // Clients handle a window state change asynchronously. So in the case
    // that the window is in a transitional state (already snapped but not
    // applied to its window state yet), we here skip to pass WM_EVENT.
    if (SplitViewController::Get(window)->IsWindowInTransitionalState(window))
      return;

    // Reset window state.
    window_state->UpdateWindowPropertiesFromStateType();
    VLOG(1) << "Processing State Transtion: event=" << event_type
            << ", state=" << state_type_ << ", next_state=" << next_state_type;

    // Then ask delegate to handle the window state change.
    delegate_->HandleWindowStateRequest(window_state, next_state_type);
  }
}

}  // namespace ash
