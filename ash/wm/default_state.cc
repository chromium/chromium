// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/default_state.h"

#include "ash/public/cpp/metrics_util.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/pip/pip_controller.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/splitview/split_view_metrics_controller.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_state_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace_controller.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

using ::chromeos::WindowStateType;

// When a window that has restore bounds at least as large as a work area is
// unmaximized, inset the bounds slightly so that they are not exactly the same.
// This makes it easier to resize the window.
const int kMaximizedWindowInset = 10;  // DIPs.

constexpr char kSnapWindowSmoothnessHistogramName[] =
    "Ash.Window.AnimationSmoothness.Snap";
constexpr char kSnapWindowDeviceOrientationHistogramName[] =
    "Ash.Window.Snap.DeviceOrientation";

gfx::Size GetWindowMaximumSize(aura::Window* window) {
  return window->delegate() ? window->delegate()->GetMaximumSize()
                            : gfx::Size();
}

// Moves the window to the specified display if necessary.
void MoveWindowToDisplayAsNeeded(aura::Window* window, int64_t display_id) {
  if (!window || display_id == display::kInvalidDisplayId) {
    return;
  }
  aura::Window* root = Shell::GetRootWindowForDisplayId(display_id);
  if (!root || root == window->GetRootWindow()) {
    // No need to move unless window is rooted in a different display.
    return;
  }
  root->GetChildById(window->parent()->GetId())->AddChild(window);
}

// Ensures the window is moved to the correct display when entering the
// next state, taking into account whether it's restoring or not.
void EnsureWindowInCorrectDisplay(WindowState* window_state,
                                  WindowStateType previous_state_type) {
  if (window_state->IsMinimized()) {
    return;
  }

  // When restoring, we want to use the restore bounds to calculate which
  // display it should be moved to, hence the check for IsRestoring() and
  // HasRestoreBounds(), otherwise we move the window according to its current
  // bounds. A special case is when we come out of minimized state, where the
  // current bounds is the bounds of the new state we're going back to, so we
  // use that to move the window.
  // The check for IsMaximizedOrFullscreenOrPinned() was moved from
  // EnterToNextState(), which preserves some legacy behavior that may not be
  // relevant anymore. It may be needed for the case of maximizing to another
  // display then restoring, which should remain on the other display.
  // TODO(aluh): Look into removing check for IsMaximizedOrFullscreenOrPinned().
  const gfx::Rect window_bounds =
      ((window_state->IsMaximizedOrFullscreenOrPinned() ||
        window_state->IsRestoring(previous_state_type)) &&
       window_state->HasRestoreBounds() &&
       !chromeos::IsMinimizedWindowStateType(previous_state_type))
          ? window_state->GetRestoreBoundsInScreen()
          : window_state->GetCurrentBoundsInScreen();

  // Move only if the window bounds is outside of
  // the display. There is no information about in which
  // display it should be restored, so this is best guess.
  // TODO(oshima): Restore information should contain the
  // work area information like WindowResizer does for the
  // last window location.
  gfx::Rect display_area = display::Screen::GetScreen()
                               ->GetDisplayNearestWindow(window_state->window())
                               .bounds();

  if (!display_area.Intersects(window_bounds)) {
    int64_t display_id =
        display::Screen::GetScreen()->GetDisplayMatching(window_bounds).id();
    MoveWindowToDisplayAsNeeded(window_state->window(), display_id);
  }
}

// Returns true if next state should be entered from the current state.
bool ShouldEnterNextState(WindowStateType current_state,
                          WindowStateType next_state,
                          WindowState* window_state) {
  if (current_state != next_state) {
    return true;
  }
  // This handles the case where a window is already fullscreen on a display
  // and we want to fullscreen it on a different display.
  // TODO(aluh): Consider handling earlier, before call to EnterToNextState(),
  // so we don't have to special case here. May run into tricky restore
  // state/bounds corner cases.
  if (next_state == chromeos::WindowStateType::kFullscreen &&
      window_state->GetFullscreenTargetDisplayId() !=
          display::kInvalidDisplayId) {
    return true;
  }
  return false;
}

}  // namespace

DefaultState::DefaultState(WindowStateType initial_state_type)
    : BaseState(initial_state_type) {}

DefaultState::~DefaultState() = default;

void DefaultState::AttachState(WindowState* window_state,
                               WindowState::State* state_in_previous_mode) {
  DCHECK_EQ(stored_window_state_, window_state);

  // If previous state is unminimized but window state is minimized, sync window
  // state to unminimized.
  if (window_state->IsMinimized() && !chromeos::IsMinimizedWindowStateType(
                                         state_in_previous_mode->GetType())) {
    aura::Window* window = window_state->window();
    window->SetProperty(
        aura::client::kShowStateKey,
        window->GetProperty(aura::client::kRestoreShowStateKey));
  }

  ReenterToCurrentState(window_state, state_in_previous_mode);

  // If the display has changed while in the another mode,
  // we need to let windows know the change.
  display::Display current_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          window_state->window());
  if (stored_display_state_.bounds() != current_display.bounds()) {
    const DisplayMetricsChangedWMEvent event(
        display::DisplayObserver::DISPLAY_METRIC_BOUNDS);
    window_state->OnWMEvent(&event);
  } else if (stored_display_state_.work_area() != current_display.work_area()) {
    const DisplayMetricsChangedWMEvent event(
        display::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
    window_state->OnWMEvent(&event);
  }
}

void DefaultState::DetachState(WindowState* window_state) {
  stored_window_state_ = window_state;
  stored_bounds_ = window_state->window()->bounds();
  stored_restore_bounds_ = window_state->HasRestoreBounds()
                               ? window_state->GetRestoreBoundsInParent()
                               : gfx::Rect();
  // Remember the display state so that in case of the display change
  // while in the other mode, we can perform necessary action to
  // restore the window state to the proper state for the current
  // display.
  stored_display_state_ = display::Screen::GetScreen()->GetDisplayNearestWindow(
      window_state->window());
}

void DefaultState::HandleWorkspaceEvents(WindowState* window_state,
                                         const WMEvent* event) {
  switch (event->type()) {
    case WM_EVENT_ADDED_TO_WORKSPACE: {
      // When a window is dragged and dropped onto a different
      // root window, the bounds will be updated after they are added
      // to the root window.
      // If a window is opened as maximized or fullscreen, its bounds may be
      // empty, so update the bounds now before checking empty.
      // TODO(minch): Check whether we can consolidate with the check inside
      // UpdateBoundsForDisplayOrWorkAreaBoundsChange before doing the
      // adjustment.
      if (window_state->is_dragged() ||
          window_state->allow_set_bounds_direct() ||
          SetMaximizedOrFullscreenBounds(window_state)) {
        return;
      }

      aura::Window* window = window_state->window();
      gfx::Rect bounds = window->bounds();
      // When window is added to a workspace, |bounds| may be not the original
      // not-changed-by-user bounds, for example a resized bounds truncated by
      // available workarea. If the window is visible on all desks, its
      // bounds are global across workspaces so don't restore to pre-added
      // bounds.
      if (window_state->pre_added_to_workspace_window_bounds() &&
          !desks_util::IsWindowVisibleOnAllWorkspaces(window)) {
        bounds = *window_state->pre_added_to_workspace_window_bounds();
      }

      // Don't adjust window bounds if the bounds are empty as this
      // happens when a new views::Widget is created.
      if (bounds.IsEmpty())
        return;

      // Only windows of type WINDOW_TYPE_NORMAL need to be adjusted to have
      // minimum visibility, because they are positioned by the user and the
      // user should always be able to interact with them. Other windows are
      // positioned programmatically.
      if (!window_state->IsUserPositionable())
        return;

      // Use entire display instead of workarea. The logic ensures 30%
      // visibility which should be enough to see where the window gets
      // moved.
      const gfx::Rect display_area =
          screen_util::GetDisplayBoundsInParent(window);
      AdjustBoundsToEnsureMinimumWindowVisibility(
          display_area, /*client_controlled=*/false, &bounds);
      window_state->AdjustSnappedBoundsForDisplayWorkspaceChange(&bounds);
      window_state->SetBoundsConstrained(bounds);
      return;
    }
    case WM_EVENT_DISPLAY_METRICS_CHANGED: {
      const DisplayMetricsChangedWMEvent* display_event =
          event->AsDisplayMetricsChangedWMEvent();
      if (display_event->display_bounds_changed()) {
        // When display bounds has changed, make sure the entire window is fully
        // visible.
        UpdateBoundsForDisplayOrWorkAreaBoundsChange(
            window_state, /*ensure_full_window_visibility=*/true);
      } else if (display_event->work_area_changed()) {
        // Don't resize the maximized window when the desktop is covered
        // by fullscreen window. crbug.com/504299.
        // TODO(afakhry): Decide whether we want the active desk's workspace, or
        // the workspace of the desk of `window_state->window()`.
        // For now use the active desk's.
        auto* workspace_controller = GetActiveWorkspaceController(
            window_state->window()->GetRootWindow());
        DCHECK(workspace_controller);
        const bool in_fullscreen = workspace_controller->GetWindowState() ==
                                   WorkspaceWindowState::kFullscreen;
        if (in_fullscreen && window_state->IsMaximized()) {
          return;
        }

        UpdateBoundsForDisplayOrWorkAreaBoundsChange(
            window_state,
            /*ensure_full_window_visibility=*/false);
      }
      return;
    }
    default:
      NOTREACHED() << "Unknown event:" << event->type();
  }
}

void DefaultState::HandleCompoundEvents(WindowState* window_state,
                                        const WMEvent* event) {
  aura::Window* window = window_state->window();

  switch (event->type()) {
    case WM_EVENT_TOGGLE_MAXIMIZE_CAPTION:
      ToggleMaximizeCaption(window_state);
      return;
    case WM_EVENT_TOGGLE_MAXIMIZE:
      ToggleMaximize(window_state);
      return;
    case WM_EVENT_TOGGLE_VERTICAL_MAXIMIZE: {
      gfx::Rect work_area =
          screen_util::GetDisplayWorkAreaBoundsInParent(window);
      // Maximize vertically if:
      // - The window does not have a max height defined.
      // - The window is floated or has the normal state type. Snapped windows
      //   are excluded because they are already maximized vertically and
      //   reverting to the restored bounds looks weird.
      if (GetWindowMaximumSize(window).height() != 0)
        return;
      if (!window_state->IsNormalStateType() && !window_state->IsFloated())
        return;
      if (!window_state->VerticallyShrinkWindow(work_area)) {
        gfx::Rect restore_bounds = window->GetTargetBounds();
        const gfx::Rect new_bounds =
            gfx::Rect(window->bounds().x(), work_area.y(),
                      window->bounds().width(), work_area.height());
        window_state->SetBoundsDirectCrossFade(new_bounds);
        if (!window_state->HasRestoreBounds())
          window_state->SetRestoreBoundsInParent(restore_bounds);
      }
      return;
    }
    case WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE: {
      // Maximize horizontally if:
      // - The window does not have a max width defined.
      // - The window is snapped or floated or has the normal state type.
      if (GetWindowMaximumSize(window).width() != 0)
        return;
      if (!window_state->IsNormalOrSnapped() && !window_state->IsFloated())
        return;
      gfx::Rect work_area =
          screen_util::GetDisplayWorkAreaBoundsInParent(window);
      if (!window_state->HorizontallyShrinkWindow(work_area)) {
        gfx::Rect new_bounds(work_area.x(), window->bounds().y(),
                             work_area.width(), window->bounds().height());
        gfx::Rect restore_bounds = window->GetTargetBounds();
        if (window_state->IsSnapped()) {
          window_state->SetRestoreBoundsInParent(window->bounds());
          window_state->Restore();

          // The restore logic prevents a window from being restored to bounds
          // which match the workspace bounds exactly so it is necessary to set
          // the bounds again below.
        }
        if (!window_state->HasRestoreBounds())
          window_state->SetRestoreBoundsInParent(restore_bounds);
        window_state->SetBoundsDirectCrossFade(new_bounds);
      }
      return;
    }
    case WM_EVENT_TOGGLE_FULLSCREEN:
      ToggleFullScreen(window_state, window_state->delegate());
      return;
    case WM_EVENT_CYCLE_SNAP_PRIMARY:
    case WM_EVENT_CYCLE_SNAP_SECONDARY:
      CycleSnap(window_state, event->type());
      return;
    default:
      NOTREACHED() << "Unknown event:" << event->type();
  }
}

void DefaultState::HandleBoundsEvents(WindowState* window_state,
                                      const WMEvent* event) {
  switch (event->type()) {
    case WM_EVENT_SET_BOUNDS: {
      const SetBoundsWMEvent* set_bounds_event =
          static_cast<const SetBoundsWMEvent*>(event);
      SetBounds(window_state, set_bounds_event);
    } break;
    default:
      NOTREACHED() << "Unknown event:" << event->type();
  }
}

void DefaultState::HandleTransitionEvents(WindowState* window_state,
                                          const WMEvent* event) {
  WindowStateType current_state_type = window_state->GetStateType();
  WindowStateType next_state_type =
      GetStateForTransitionEvent(window_state, event);
  if (event->IsPinEvent()) {
    // If there already is a pinned window, it is not allowed to set it
    // to this window.
    // TODO(hidehiko): If a system modal window is openening, the pinning
    // probably should fail.
    if (Shell::Get()->screen_pinning_controller()->IsPinned()) {
      LOG(ERROR) << "An PIN event will be failed since another window is "
                 << "already in pinned mode.";
      next_state_type = current_state_type;
    }
  }

  const WMEventType type = event->type();
  // Not all windows can be floated.
  if (type == WM_EVENT_FLOAT &&
      !chromeos::wm::CanFloatWindow(window_state->window())) {
    return;
  }

  if ((type == WM_EVENT_SNAP_PRIMARY || type == WM_EVENT_SNAP_SECONDARY) &&
      window_state->CanSnap()) {
    HandleWindowSnapping(window_state, type,
                         event->AsSnapEvent()->snap_action_source());
  }

  if (next_state_type == current_state_type && window_state->IsSnapped()) {
    DCHECK(window_state->snap_ratio());
    gfx::Rect snapped_bounds =
        GetSnappedWindowBoundsInParent(window_state->window(),
                                       event->type() == WM_EVENT_SNAP_PRIMARY
                                           ? WindowStateType::kPrimarySnapped
                                           : WindowStateType::kSecondarySnapped,
                                       *window_state->snap_ratio());
    window_state->SetBoundsDirectAnimated(snapped_bounds);
    return;
  }

  if (IsSnappedWindowStateType(next_state_type)) {
    const bool is_restoring =
        window_state->window()->GetProperty(aura::client::kIsRestoringKey) ||
        type == WM_EVENT_RESTORE;
    if (is_restoring) {
      window_state->RecordWindowSnapActionSource(
          WindowSnapActionSource::kSnapByWindowStateRestore);
    } else {
      CHECK(event->IsSnapEvent());
      window_state->RecordWindowSnapActionSource(
          event->AsSnapEvent()->snap_action_source());
    }
  }

  std::optional<chromeos::FloatStartLocation> float_start_location =
      event->AsFloatEvent()
          ? std::make_optional(event->AsFloatEvent()->float_start_location())
          : std::nullopt;
  EnterToNextState(window_state, next_state_type, float_start_location);
}

bool DefaultState::SetMaximizedOrFullscreenBounds(WindowState* window_state) {
  DCHECK(!window_state->is_dragged());
  DCHECK(!window_state->allow_set_bounds_direct());
  if (window_state->IsMaximized()) {
    window_state->SetBoundsDirect(
        screen_util::GetMaximizedWindowBoundsInParent(window_state->window()));
    return true;
  }
  if (window_state->IsFullscreen() || window_state->IsPinned()) {
    window_state->SetBoundsDirect(
        screen_util::GetFullscreenWindowBoundsInParent(window_state->window()));
    return true;
  }
  return false;
}

void DefaultState::SetBounds(WindowState* window_state,
                             const SetBoundsWMEvent* event) {
  // TODO(andreaorru|oshima): Fix dragging code so that if a window is dragging
  // tabs, it contains drag details, and `is_dragged` is true for its state.
  // Then we can simplify this condition and remove `IsDraggingTabs`.
  bool is_dragged = window_state->is_dragged() ||
                    window_util::IsDraggingTabs(window_state->window());

  if (is_dragged || window_state->allow_set_bounds_direct()) {
    if (event->animate()) {
      window_state->SetBoundsDirectAnimated(event->requested_bounds_in_parent(),
                                            event->duration());
    } else {
      // TODO(oshima|varkha): Is this still needed? crbug.com/485612.
      window_state->SetBoundsDirect(event->requested_bounds_in_parent());
    }
  } else if (!SetMaximizedOrFullscreenBounds(window_state)) {
    if (event->animate()) {
      window_state->SetBoundsDirectAnimated(event->requested_bounds_in_parent(),
                                            event->duration());
    } else {
      window_state->SetBoundsConstrained(event->requested_bounds_in_parent());
      // Update the restore size if the bounds is updated by PIP itself.
      if (window_state->IsPip() && window_state->HasRestoreBounds()) {
        gfx::Rect restore_bounds = window_state->GetRestoreBoundsInScreen();
        restore_bounds.set_size(
            window_state->window()->GetTargetBounds().size());
        window_state->SetRestoreBoundsInScreen(restore_bounds);
      }
    }
  }
}

void DefaultState::EnterToNextState(
    WindowState* window_state,
    WindowStateType next_state_type,
    std::optional<chromeos::FloatStartLocation> float_start_location) {
  if (!ShouldEnterNextState(state_type_, next_state_type, window_state)) {
    return;
  }

  const bool is_previous_normal_type =
      window_state->IsNonVerticalOrHorizontalMaximizedNormalState();
  WindowStateType previous_state_type = state_type_;
  state_type_ = next_state_type;

  window_state->UpdateWindowPropertiesFromStateType();
  window_state->NotifyPreStateTypeChange(previous_state_type);

  auto* const float_controller = Shell::Get()->float_controller();
  auto* window = window_state->window();
  if (state_type_ == WindowStateType::kFloated) {
    DCHECK_EQ(next_state_type, WindowStateType::kFloated);
    // Add window to float container.
    float_controller->FloatImpl(window);
  }

  // Unfloat floated window when exiting float state to another state.
  if (previous_state_type == WindowStateType::kFloated) {
    float_controller->UnfloatImpl(window);
  }

  // Don't update the window if the window is detached from parent.
  // This can happen during dragging.
  // TODO(oshima): This was added for DOCKED windows. Investigate if
  // we still need this.
  gfx::Rect restore_bounds_in_screen;
  if (window_state->window()->parent()) {
    // Save the current bounds as the restore bounds if changing from normal
    // state (not horizontal/vertical maximized) to other window states.
    if (is_previous_normal_type && !window_state->IsNormalStateType()) {
      window_state->SaveCurrentBoundsForRestore();
    }

    // When restoring from the minimized state to horizontal/vertical maximized.
    // We want to restore to the previous horizontal/vertical maximized bounds
    // and keep its restore bounds.(E.g, double clicking the window border will
    // set the window to be horizontal/vertical maximized and set the restore
    // bounds).
    if (previous_state_type == WindowStateType::kMinimized &&
        window_state->IsVerticalOrHorizontalMaximized() &&
        !window_state->unminimize_to_restore_bounds()) {
      restore_bounds_in_screen = window_state->GetRestoreBoundsInScreen();
      window_state->SaveCurrentBoundsForRestore();
    }

    UpdateBoundsFromState(window_state, previous_state_type,
                          float_start_location);
    UpdateMinimizedState(window_state, previous_state_type);
  }
  window_state->NotifyPostStateTypeChange(previous_state_type);

  if (!restore_bounds_in_screen.IsEmpty()) {
    // Set the restore bounds back after unminimize the window to normal state.
    // Usually normal state window should have no restore bounds unless it was
    // horizontal/vertical maximized before minimize.
    window_state->SetRestoreBoundsInScreen(restore_bounds_in_screen);
  } else if (window_state->window_state_restore_history().empty()) {
    // Clear the restore bounds when restore history stack has been cleared to
    // keep them consistent. Do this after window state updates as restore
    // history stack will be updated during the process.
    window_state->ClearRestoreBounds();
  }

  if (IsPinnedWindowStateType(next_state_type) ||
      IsPinnedWindowStateType(previous_state_type)) {
    Shell::Get()->screen_pinning_controller()->SetPinnedWindow(
        window_state->window());
    if (window_state->delegate())
      window_state->delegate()->ToggleLockedFullscreen(window_state);
  }
}

void DefaultState::ReenterToCurrentState(
    WindowState* window_state,
    WindowState::State* state_in_previous_mode) {
  WindowStateType previous_state_type = state_in_previous_mode->GetType();

  // A state change should not move a window into or out of full screen or
  // pinned or float since these are "special mode" the user wanted to be in and
  // should be respected as such.
  if (IsFullscreenOrPinnedWindowStateType(previous_state_type) ||
      IsFullscreenOrPinnedWindowStateType(state_type_) ||
      previous_state_type == WindowStateType::kFloated ||
      state_type_ == WindowStateType::kFloated) {
    state_type_ = previous_state_type;
  }

  window_state->UpdateWindowPropertiesFromStateType();
  window_state->NotifyPreStateTypeChange(previous_state_type);

  if (IsNormalWindowStateType(state_type_) && !stored_bounds_.IsEmpty()) {
    // Use the restore mechanism to set the bounds for
    // the window in normal state. This also covers unminimize case.
    window_state->SetRestoreBoundsInParent(stored_bounds_);
  }

  UpdateBoundsFromState(window_state, state_in_previous_mode->GetType(),
                        /*float_start_location=*/std::nullopt);
  UpdateMinimizedState(window_state, state_in_previous_mode->GetType());

  // Then restore the restore bounds to their previous value.
  if (!stored_restore_bounds_.IsEmpty())
    window_state->SetRestoreBoundsInParent(stored_restore_bounds_);
  else
    window_state->ClearRestoreBounds();

  window_state->NotifyPostStateTypeChange(previous_state_type);
}

void DefaultState::UpdateBoundsFromState(
    WindowState* window_state,
    WindowStateType previous_state_type,
    std::optional<chromeos::FloatStartLocation> float_start_location) {
  aura::Window* window = window_state->window();
  gfx::Rect bounds_in_parent;

  // A window can be rooted in a different display than its bounds, in cases
  // such as creating a new window with bounds in a different display, or
  // restoring to a previous state that was in a different display.
  EnsureWindowInCorrectDisplay(window_state, previous_state_type);

  switch (state_type_) {
    case WindowStateType::kPrimarySnapped:
    case WindowStateType::kSecondarySnapped:
      DCHECK(window_state->snap_ratio());
      bounds_in_parent = GetSnappedWindowBoundsInParent(
          window_state->window(), state_type_, *window_state->snap_ratio());
      base::UmaHistogramEnumeration(
          kSnapWindowDeviceOrientationHistogramName,
          display::Screen::GetScreen()
                  ->GetDisplayNearestWindow(window)
                  .is_landscape()
              ? SplitViewMetricsController::DeviceOrientation::kLandscape
              : SplitViewMetricsController::DeviceOrientation::kPortrait);
      break;
    case WindowStateType::kDefault:
    case WindowStateType::kNormal: {
      gfx::Rect work_area_in_parent =
          screen_util::GetDisplayWorkAreaBoundsInParent(window);
      if (window_state->HasRestoreBounds()) {
        bounds_in_parent = window_state->GetRestoreBoundsInParent();
        // Check if the |window|'s restored size is bigger than the working area
        // This may happen if a window was resized to maximized bounds or if the
        // display resolution changed while the window was maximized.
        if (previous_state_type == WindowStateType::kMaximized &&
            bounds_in_parent.width() >= work_area_in_parent.width() &&
            bounds_in_parent.height() >= work_area_in_parent.height()) {
          bounds_in_parent = work_area_in_parent;
          bounds_in_parent.Inset(kMaximizedWindowInset);
        }
      } else {
        bounds_in_parent = window->bounds();
      }
      // Make sure that part of the window is always visible.
      if (!window_state->is_dragged()) {
        // Avoid doing this while the window is being dragged as its root
        // window hasn't been updated yet in the case of dragging to another
        // display. crbug.com/666836.
        AdjustBoundsToEnsureMinimumWindowVisibility(work_area_in_parent,
                                                    /*client_controlled=*/false,
                                                    &bounds_in_parent);
      }
      break;
    }
    case WindowStateType::kMaximized:
      bounds_in_parent = screen_util::GetMaximizedWindowBoundsInParent(window);
      break;

    case WindowStateType::kFullscreen:
    case WindowStateType::kPinned:
    case WindowStateType::kTrustedPinned:
      MoveWindowToDisplayAsNeeded(window_state->window(),
                                  window_state->GetFullscreenTargetDisplayId());
      bounds_in_parent = screen_util::GetFullscreenWindowBoundsInParent(window);
      break;

    case WindowStateType::kMinimized:
      break;
    case WindowStateType::kFloated: {
      // When a floated window is previously minimized, un-minimize will restore
      // the float state with previous floated bounds, without re-calculating
      // preferred bounds.
      if (previous_state_type == WindowStateType::kMinimized) {
        bounds_in_parent = window->bounds();
      } else {
        // Default state can be used for always on top windows in tablet mode,
        // which are not managed by the tablet mode window manager. Float state
        // is not allowed for always on top but this may be called when a
        // floated window has been put into always on top and we have not yet
        // exited float state yet. See http://b/317064996 for more details.
        // TODO(http://b/325282588): `DefaultState` should be for clamshell
        // (non-ARC apps) only. See if `TabletModeWindowState` can handle
        // always-on-top window gracefully.
        bounds_in_parent =
            window->GetProperty(aura::client::kZOrderingKey) !=
                    ui::ZOrderLevel::kNormal
                ? window->bounds()
                : Shell::Get()
                      ->float_controller()
                      ->GetFloatWindowClamshellBounds(
                          window,
                          float_start_location.value_or(
                              chromeos::FloatStartLocation::kBottomRight));
      }
      break;
    }
    case WindowStateType::kInactive:
    case WindowStateType::kPip:
      return;
  }

  if (window_state->IsMinimized())
    return;

  if (bool to_float = state_type_ == WindowStateType::kFloated;
      to_float || previous_state_type == WindowStateType::kFloated) {
    // Float and unfloat have their own animation.
    window_state->SetBoundsDirectCrossFade(bounds_in_parent, to_float);
  } else if (IsMinimizedWindowStateType(previous_state_type) ||
             window_state->IsFullscreen() || window_state->IsPinned() ||
             window_state->bounds_animation_type() ==
                 WindowState::BoundsChangeAnimationType::kNone) {
    window_state->SetBoundsDirect(bounds_in_parent);
  } else if (window_state->IsMaximized() ||
             IsMaximizedOrFullscreenOrPinnedWindowStateType(
                 previous_state_type)) {
    window_state->SetBoundsDirectCrossFade(bounds_in_parent);
  } else if (window_state->is_dragged()) {
    // SetBoundsDirectAnimated does not work when the window gets reparented.
    // TODO(oshima): Consider fixing it and re-enable the animation.
    window_state->SetBoundsDirect(bounds_in_parent);
  } else {
    // Record smoothness of the snapping animation if the size of the window
    // changes.
    std::optional<ui::AnimationThroughputReporter> reporter;
    if (window_state->IsSnapped() &&
        bounds_in_parent.size() != window->bounds().size()) {
      reporter.emplace(
          window_state->window()->layer()->GetAnimator(),
          metrics_util::ForSmoothnessV3(base::BindRepeating([](int smoothness) {
            UMA_HISTOGRAM_PERCENTAGE(kSnapWindowSmoothnessHistogramName,
                                     smoothness);
          })));
    }

    window_state->SetBoundsDirectAnimated(bounds_in_parent);
  }
}

void DefaultState::UpdateBoundsForDisplayOrWorkAreaBoundsChange(
    WindowState* window_state,
    bool ensure_full_window_visibility) {
  if (window_state->is_dragged() || window_state->allow_set_bounds_direct() ||
      window_state->ignore_keyboard_bounds_change() ||
      SetMaximizedOrFullscreenBounds(window_state)) {
    return;
  }

  const gfx::Rect work_area_in_parent =
      screen_util::GetDisplayWorkAreaBoundsInParent(window_state->window());
  gfx::Rect bounds = window_state->window()->GetTargetBounds();
  if (ensure_full_window_visibility)
    bounds.AdjustToFit(work_area_in_parent);
  else if (!wm::GetTransientParent(window_state->window()) &&
           !(window_state->IsPip() &&
             Shell::Get()->pip_controller()->is_tucked())) {
    AdjustBoundsToEnsureMinimumWindowVisibility(
        work_area_in_parent, /*client_controlled=*/false, &bounds);
  }
  window_state->AdjustSnappedBoundsForDisplayWorkspaceChange(&bounds);

  if (window_state->window()->GetTargetBounds() == bounds)
    return;

  if (window_state->bounds_animation_type() ==
      WindowState::BoundsChangeAnimationType::kNone) {
    window_state->SetBoundsDirect(bounds);
  } else {
    window_state->SetBoundsDirectAnimated(bounds);
  }
}

}  // namespace ash
