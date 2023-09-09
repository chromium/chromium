// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/auto_snap_controller.h"

#include "ash/shell.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_metrics.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_properties.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

AutoSnapController::AutoSnapController(
    SplitViewController* split_view_controller)
    : split_view_controller_(split_view_controller) {
  Shell::Get()->activation_client()->AddObserver(this);
  AddWindow(split_view_controller->root_window());
  for (auto* window :
       Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk)) {
    AddWindow(window);
  }
}

AutoSnapController::~AutoSnapController() {
  window_observations_.RemoveAllObservations();
  Shell::Get()->activation_client()->RemoveObserver(this);
}

void AutoSnapController::OnWindowActivated(ActivationReason reason,
                                           aura::Window* gained_active,
                                           aura::Window* lost_active) {
  if (!gained_active) {
    return;
  }

  // If `gained_active` was activated as a side effect of a window disposition
  // change, do nothing. For example, when a snapped window is closed, another
  // window will be activated before `OnWindowDestroying()` is called. We should
  // not try to snap another window in this case.
  if (reason == ActivationReason::WINDOW_DISPOSITION_CHANGED) {
    return;
  }

  AutoSnapWindowIfNeeded(gained_active);
}

void AutoSnapController::OnWindowVisibilityChanging(aura::Window* window,
                                                    bool visible) {
  // When a minimized window's visibility changes from invisible to visible or
  // is about to activate, it triggers an implicit un-minimizing (e.g.
  // `WorkspaceLayoutManager::OnChildWindowVisibilityChanged()` or
  // `WorkspaceLayoutManager::OnWindowActivating()`). This emits a window
  // state change event but it is unnecessary for to-be-snapped windows
  // because some clients (e.g. ARC app) handle a window state change
  // asynchronously. So in the case, we here try to snap a window before
  // other's handling to avoid the implicit un-minimizing.

  // Auto snapping is applicable for window changed to be visible.
  if (!visible) {
    return;
  }

  // Already un-minimized windows are not applicable for auto snapping.
  if (auto* window_state = WindowState::Get(window);
      !window_state || !window_state->IsMinimized()) {
    return;
  }

  // Visibility changes while restoring windows after dragged is transient
  // hide & show operations so not applicable for auto snapping.
  if (window->GetProperty(kHideDuringWindowDragging)) {
    return;
  }

  AutoSnapWindowIfNeeded(window);
}

void AutoSnapController::OnWindowAddedToRootWindow(aura::Window* window) {
  AddWindow(window);
}

void AutoSnapController::OnWindowRemovingFromRootWindow(
    aura::Window* window,
    aura::Window* new_root) {
  RemoveWindow(window);
}

void AutoSnapController::OnWindowDestroying(aura::Window* window) {
  RemoveWindow(window);
}

void AutoSnapController::AutoSnapWindowIfNeeded(aura::Window* window) {
  CHECK(window);

  if (window->GetRootWindow() != split_view_controller_->root_window()) {
    return;
  }

  // We perform an "auto" snapping only if split view mode is active.
  if (!split_view_controller_->InSplitViewMode()) {
    return;
  }

  // If `window` is floated on top of 2 already snapped windows (this can
  // happen after floating a window, starting split view, and activating
  // an unfloated window from overview), don't snap.
  if (WindowState::Get(window)->IsFloated() &&
      split_view_controller_->BothSnapped()) {
    return;
  }

  if (DesksController::Get()->AreDesksBeingModified()) {
    // Activating a desk from its mini view will activate its most-recently
    // used window, but this should not result in snapping and ending overview
    // mode now. Overview will be ended explicitly as part of the desk
    // activation animation.
    return;
  }

  // Only windows that are in the MRU list and are not already in split view
  // can be auto-snapped.
  if (split_view_controller_->IsWindowInSplitView(window) ||
      !base::Contains(
          Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk),
          window)) {
    return;
  }

  // We do not auto snap windows in clamshell splitview mode if a new window
  // is activated when clamshell splitview mode is active.
  // TODO(xdai): Handle this logic in `OverviewSession::OnWindowActivating()`.
  // TODO(michelefan): Considering to auto-snap the second window with one
  // window snapped when
  // `SnapGroupController::IsArm1AutomaticallyLockEnabled()`.
  if (split_view_controller_->InClamshellSplitViewMode()) {
    if (split_view_controller_->IsWindowInTransitionalState(window)) {
      // If `window` is the transitional state (i.e. it's going to be snapped
      // very soon), no need to end overview mode here because
      // `OverviewGrid::OnSplitViewStateChanged()` will handle it when the
      // snapped state is applied.
      return;
    }
    // If activated `window` is not going to be snapped, we just end overview
    // mode which will then end splitview mode.
    Shell::Get()->overview_controller()->EndOverview(
        OverviewEndAction::kSplitView);
    return;
  }

  CHECK(split_view_controller_->InTabletSplitViewMode());

  // Do not snap the window if the activation change is caused by dragging a
  // window.
  if (WindowState::Get(window)->is_dragged()) {
    return;
  }

  // If the divider is animating, then `window` cannot be snapped (and is
  // not already snapped either, because then we would have bailed out by
  // now). Then if `window` is user-positionable, we should end split view
  // mode, but the cannot snap toast would be inappropriate because the user
  // still might be able to snap `window`.
  if (split_view_controller_->IsDividerAnimating()) {
    if (WindowState::Get(window)->IsUserPositionable()) {
      split_view_controller_->EndSplitView(
          SplitViewController::EndReason::kUnsnappableWindowActivated);
    }
    return;
  }

  absl::optional<float> snap_ratio =
      split_view_controller_->ComputeSnapRatio(window);

  // If it's a user positionable window but can't be snapped, end split view
  // mode and show the cannot snap toast.
  if (!snap_ratio) {
    if (WindowState::Get(window)->IsUserPositionable()) {
      split_view_controller_->EndSplitView(
          SplitViewController::EndReason::kUnsnappableWindowActivated);
      ShowAppCannotSnapToast();
    }
    return;
  }

  // Snap the window on the non-default side of the screen if split view mode
  // is active.
  split_view_controller_->SnapWindow(
      window,
      (split_view_controller_->default_snap_position() ==
       SplitViewController::SnapPosition::kPrimary)
          ? SplitViewController::SnapPosition::kSecondary
          : SplitViewController::SnapPosition::kPrimary,
      WindowSnapActionSource::kAutoSnapInSplitView,
      /*activate_window=*/false, *snap_ratio);
}

void AutoSnapController::AddWindow(aura::Window* window) {
  if (split_view_controller_->root_window() != window->GetRootWindow()) {
    return;
  }

  if (!window_observations_.IsObservingSource(window)) {
    window_observations_.AddObservation(window);
  }
}

void AutoSnapController::RemoveWindow(aura::Window* window) {
  window_observations_.RemoveObservation(window);
}

}  // namespace ash
