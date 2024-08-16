// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/client_controlled_state.h"

#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_state_util.h"
#include "ash/wm/wm_metrics.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/display/screen.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

using ::chromeos::WindowStateType;

}  // namespace

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
    case WM_EVENT_SNAP_SECONDARY:
    case WM_EVENT_FLOAT: {
      WindowStateType next_state =
          GetResolvedNextWindowStateType(window_state, event);
      UpdateWindowForTransitionEvents(window_state, next_state, event);
      break;
    }
    case WM_EVENT_RESTORE:
      UpdateWindowForTransitionEvents(
          window_state, window_state->GetRestoreWindowState(), event);
      break;
    case WM_EVENT_SHOW_INACTIVE:
      NOTREACHED();
    default:
      NOTREACHED() << "Unknown event :" << event->type();
  }
}

void ClientControlledState::AttachState(
    WindowState* window_state,
    WindowState::State* state_in_previous_mode) {
  window_state->is_client_controlled_ = true;
}

void ClientControlledState::DetachState(WindowState* window_state) {
  window_state->is_client_controlled_ = false;
}

void ClientControlledState::HandleWorkspaceEvents(WindowState* window_state,
                                                  const WMEvent* event) {
  if (!delegate_)
    return;
  aura::Window* const window = window_state->window();
  // Client is responsible for adjusting bounds after workspace bounds change.
  if (window_state->IsSnapped()) {
    // If `SplitViewController` is aware of `window` (e.g. in tablet), let the
    // controller handle the workspace event.
    if (SplitViewController::Get(window)->IsWindowInSplitView(window)) {
      return;
    }

    gfx::Rect bounds = window->bounds();
    window_state->AdjustSnappedBoundsForDisplayWorkspaceChange(&bounds);

    // Then ask delegate to set the desired bounds for the snap state.
    delegate_->HandleBoundsRequest(window_state, window_state->GetStateType(),
                                   bounds, window_state->GetDisplay().id());
  } else if (window_state->IsFloated()) {
    if (!window->parent()) {
      // If the window is now reparenting to another container (or being
      // destroyed), no need to adjust floated bounds. The next workspace event
      // (`WM_EVENT_ADDED_TO_WORKSPACE`) is coming soon anyway.
      return;
    }
    const gfx::Rect bounds =
        display::Screen::GetScreen()->InTabletMode()
            ? FloatController::GetFloatWindowTabletBounds(window)
            : FloatController::GetFloatWindowClamshellBounds(
                  window,
                  // TODO(b/292579250): Add a mechanism to float as close to the
                  // previous bounds in the event of a workspace event. For now,
                  // use the default float location.
                  chromeos::FloatStartLocation::kBottomRight);
    delegate_->HandleBoundsRequest(window_state, window_state->GetStateType(),
                                   bounds, window_state->GetDisplay().id());
  } else if (event->type() == WM_EVENT_DISPLAY_METRICS_CHANGED) {
    // Explicitly handle the primary change because it can change the display id
    // with no bounds change.
    if (event->AsDisplayMetricsChangedWMEvent()->primary_changed()) {
      const gfx::Rect bounds = window->bounds();
      delegate_->HandleBoundsRequest(window_state, window_state->GetStateType(),
                                     bounds, window_state->GetDisplay().id());
    }
  } else if (event->type() == WM_EVENT_ADDED_TO_WORKSPACE) {
    gfx::Rect bounds = window->bounds();
    AdjustBoundsToEnsureMinimumWindowVisibility(
        window->GetRootWindow()->bounds(), /*client_controlled=*/true, &bounds);

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
      ToggleMaximizeCaption(window_state);
      break;
    case WM_EVENT_TOGGLE_MAXIMIZE:
      ToggleMaximize(window_state);
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
  }
}

void ClientControlledState::HandleBoundsEvents(WindowState* window_state,
                                               const WMEvent* event) {
  if (!delegate_)
    return;
  auto* const window = window_state->window();
  switch (event->type()) {
    case WM_EVENT_SET_BOUNDS: {
      const auto* set_bounds_event = event->AsSetBoundsWMEvent();
      const gfx::Rect& bounds = set_bounds_event->requested_bounds_in_parent();
      if (set_bounds_locally_) {
        if (window_state->IsFloated()) {
          // Don’t preempt on-going animation (e.g. tucking) for floated
          // windows.
          if (window->layer() &&
              window->layer()->GetAnimator()->is_animating()) {
            return;
          }
          // Don't move the tucked window. It's fully controlled by ash now.
          if (Shell::Get()->float_controller()->IsFloatedWindowTuckedForTablet(
                  window)) {
            return;
          }
        }

        switch (next_bounds_change_animation_type_) {
          case WindowState::BoundsChangeAnimationType::kNone:
            window_state->SetBoundsDirect(bounds);
            break;
          case WindowState::BoundsChangeAnimationType::kCrossFade:
            window_state->SetBoundsDirectCrossFade(bounds);
            break;
          case WindowState::BoundsChangeAnimationType::kCrossFadeFloat:
            window_state->SetBoundsDirectCrossFade(bounds, true);
            break;
          case WindowState::BoundsChangeAnimationType::kCrossFadeUnfloat:
            window_state->SetBoundsDirectCrossFade(bounds, false);
            break;
          case WindowState::BoundsChangeAnimationType::kAnimate:
            window_state->SetBoundsDirectAnimated(
                bounds, bounds_change_animation_duration_);
            break;
          case WindowState::BoundsChangeAnimationType::kAnimateZero:
            NOTREACHED();
        }
        next_bounds_change_animation_type_ =
            WindowState::BoundsChangeAnimationType::kNone;
      } else if (!window_state->IsPinned()) {
        // TODO(oshima): Define behavior for pinned app.
        bounds_change_animation_duration_ = set_bounds_event->duration();
        int64_t display_id = set_bounds_event->display_id();
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

  auto* const window = window_state->window();

  // Calling order matters. We need to handle the floated state before handling
  // the minimized state because FloatController may change the visibility of
  // the window.
  auto* const float_controller = Shell::Get()->float_controller();
  if (next_state_type == WindowStateType::kFloated) {
    if (display::Screen::GetScreen()->InTabletMode()) {
      float_controller->FloatForTablet(window, previous_state_type);
    } else {
      float_controller->FloatImpl(window);
    }
  }
  if (previous_state_type == WindowStateType::kFloated) {
    float_controller->UnfloatImpl(window);
  }

  // Don't update the window if the window is detached from parent.
  // This can happen during dragging.
  // TODO(oshima): This was added for DOCKED windows. Investigate if
  // we still need this.
  if (window->parent()) {
    UpdateMinimizedState(window_state, previous_state_type);
  }

  window_state->NotifyPostStateTypeChange(previous_state_type);

  if (IsPinnedWindowStateType(next_state_type) ||
      IsPinnedWindowStateType(previous_state_type)) {
    set_next_bounds_change_animation_type(
        WindowState::BoundsChangeAnimationType::kCrossFade);
    Shell::Get()->screen_pinning_controller()->SetPinnedWindow(window);
  }
  return true;
}

WindowStateType ClientControlledState::GetResolvedNextWindowStateType(
    WindowState* window_state,
    const WMEvent* event) {
  DCHECK(event->IsTransitionEvent());

  const WindowStateType next = GetStateForTransitionEvent(window_state, event);

  if (display::Screen::GetScreen()->InTabletMode() &&
      next == WindowStateType::kNormal && window_state->CanMaximize()) {
    return WindowStateType::kMaximized;
  }

  return next;
}

void ClientControlledState::UpdateWindowForTransitionEvents(
    WindowState* window_state,
    chromeos::WindowStateType next_state_type,
    const WMEvent* event) {
  const WMEventType event_type = event->type();
  aura::Window* window = window_state->window();

  if (chromeos::IsSnappedWindowStateType(next_state_type)) {
    if (window_state->CanSnap()) {
      const bool is_restoring =
          window->GetProperty(aura::client::kIsRestoringKey) ||
          event_type == WM_EVENT_RESTORE;
      CHECK(is_restoring || event->IsSnapEvent());

      // If the window is being unminimized to any snapped state and it's still
      // transitioning, no need to handle the extra snap event.
      if (window_state->IsMinimized() && is_restoring &&
          SplitViewController::Get(window)->IsWindowInTransitionalState(
              window)) {
        return;
      }

      const WindowSnapActionSource snap_action_source =
          is_restoring ? WindowSnapActionSource::kSnapByWindowStateRestore
                       : event->AsSnapEvent()->snap_action_source();
      HandleWindowSnapping(window_state,
                           next_state_type == WindowStateType::kPrimarySnapped
                               ? WM_EVENT_SNAP_PRIMARY
                               : WM_EVENT_SNAP_SECONDARY,
                           snap_action_source);
      window_state->RecordWindowSnapActionSource(snap_action_source);

      // Get the desired window bounds for the snap state.
      // TODO(b/246683799): Investigate why window_state->snap_ratio() can be
      // empty.
      // Use the saved `window_state->snap_ratio()` if restoring, otherwise use
      // the event requested snap ratio, which has a default value.
      const float next_snap_ratio =
          is_restoring
              ? window_state->snap_ratio().value_or(chromeos::kDefaultSnapRatio)
              : event->AsSnapEvent()->snap_ratio();

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

      const gfx::Rect snapped_bounds = GetSnappedWindowBoundsInParent(
          window, next_state_type, next_snap_ratio);

      // The snap ratio of `snapped_bounds` may be different from the requested
      // snap ratio (e.g., if the window has a minimum size requirement or the
      // opposite side of splitview is partial-snapped).
      window_state->ForceUpdateSnapRatio(snapped_bounds);

      // Then ask delegate to set the desired bounds for the snap state.
      delegate_->HandleBoundsRequest(window_state, next_state_type,
                                     snapped_bounds,
                                     window_state->GetDisplay().id());
    }
  } else if (next_state_type == WindowStateType::kFloated) {
    if (chromeos::wm::CanFloatWindow(window)) {
      const gfx::Rect bounds =
          display::Screen::GetScreen()->InTabletMode()
              ? FloatController::GetFloatWindowTabletBounds(window)
              : FloatController::GetFloatWindowClamshellBounds(
                    window, event_type == WM_EVENT_FLOAT
                                ? event->AsFloatEvent()->float_start_location()
                                : chromeos::FloatStartLocation::kBottomRight);

      window_state->UpdateWindowPropertiesFromStateType();
      VLOG(1) << "Processing State Transtion: event=" << event_type
              << ", state=" << state_type_
              << ", next_state=" << next_state_type;

      // Then ask delegate to set the desired bounds for the float state.
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
