// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/auto_snap_controller.h"

#include <optional>

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_metrics.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_overview_session.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Returns the snap ratio for the given `window` to be auto-snapped on the
// opposite position of the screen.
std::optional<float> CalculateAutoSnapRatio(
    SplitViewController* split_view_controller,
    aura::Window* window) {
  if (Shell::Get()->IsInTabletMode()) {
    return split_view_controller->ComputeAutoSnapRatio(window);
  }

  auto* window_state = WindowState::Get(window);
  if (auto* split_view_overview_session =
          RootWindowController::ForWindow(window)
              ->split_view_overview_session();
      split_view_overview_session &&
      split_view_overview_session->window() != window) {
    if (!window_state->CanSnap()) {
      return std::nullopt;
    }

    const float snap_ratio =
        1.f - WindowState::Get(split_view_overview_session->window())
                  ->snap_ratio()
                  .value_or(chromeos::kDefaultSnapRatio);
    return std::make_optional<float>(snap_ratio);
  }

  return window_state->snap_ratio();
}

}  // namespace

AutoSnapController::AutoSnapController(aura::Window* root_window)
    : root_window_(root_window) {
  Shell::Get()->activation_client()->AddObserver(this);

  AddWindow(root_window);
  for (aura::Window* window :
       Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk)) {
    AddWindow(window);
  }
}

AutoSnapController::~AutoSnapController() {
  window_observations_.RemoveAllObservations();
  Shell::Get()->activation_client()->RemoveObserver(this);
}

void AutoSnapController::OnWindowActivating(ActivationReason reason,
                                            aura::Window* gained_active,
                                            aura::Window* lost_active) {
  // If `gained_active` was activated as a side effect of a window disposition
  // change, do nothing. For example, when a snapped window is closed, another
  // window will be activated before `OnWindowDestroying()` is called. We should
  // not try to snap another window in this case.
  if (!gained_active ||
      reason == ActivationReason::WINDOW_DISPOSITION_CHANGED) {
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
  if (window == root_window_) {
    // The root window is destroyed asynchronously with RootWindowController,
    // which owns SplitViewController, which owns `this`, so `root_window_` must
    // be reset earlier, in `OnWindowDestroying()`.
    root_window_ = nullptr;
  }
}

bool AutoSnapController::AutoSnapWindowIfNeeded(aura::Window* window) {
  CHECK(window);

  if (window->GetRootWindow() != root_window_) {
    return false;
  }

  OverviewController* overview_controller = OverviewController::Get();
  if (auto* overview_session = overview_controller->overview_session();
      overview_session && overview_session->is_shutting_down()) {
    // `OverviewSession::Shutdown()` may restore window activation and trigger
    // this; do not auto snap in this case.
    return false;
  }

  auto* split_view_controller = SplitViewController::Get(window);
  if (!split_view_controller->InSplitViewMode()) {
    // A window may be activated during mid-drag, during which split view is not
    // active yet.
    return false;
  }

  WindowState* window_state = WindowState::Get(window);
  const SplitViewController::State state = split_view_controller->state();
  // If `window` is floated on top of 2 already snapped windows (this can
  // happen after floating a window, starting split view, and activating
  // an unfloated window from overview), don't snap.
  if (window_state->IsFloated() &&
      state == SplitViewController::State::kBothSnapped) {
    return false;
  }

  if (DesksController::Get()->AreDesksBeingModified()) {
    // Activating a desk from its mini view will activate its most-recently
    // used window, but this should not result in snapping and ending overview
    // mode now. Overview will be ended explicitly as part of the desk
    // activation animation.
    return false;
  }

  // Only windows that are in the MRU list and are not already in split view can
  // be auto-snapped.
  if (split_view_controller->IsWindowInSplitView(window) ||
      !base::Contains(
          Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk),
          window)) {
    return false;
  }

  // If `window` is in transitional state (i.e. it's going to be snapped very
  // soon), no need to request another snap request. Also, no need to end
  // overview mode here because `OverviewGrid::OnSplitViewStateChanged()` will
  // handle it when the snapped state is applied.
  if (split_view_controller->IsWindowInTransitionalState(window)) {
    return false;
  }

  if (split_view_controller->InClamshellSplitViewMode() &&
      overview_controller->InOverviewSession() &&
      !overview_controller->overview_session()->IsWindowInOverview(window) &&
      !window_util::IsInFasterSplitScreenSetupSession(window)) {
    // Do not auto snap windows in clamshell splitview mode if a new window
    // is activated when clamshell splitview mode is active unless in faster
    // split screen setup session.
    overview_controller->EndOverview(OverviewEndAction::kSplitView);
    return false;
  }

  // Do not snap the window if the activation change is caused by dragging a
  // window.
  if (window_state->is_dragged()) {
    return false;
  }

  // If the divider is animating, then `window` cannot be snapped (and is
  // not already snapped either, because then we would have bailed out by
  // now). Then if `window` is user-positionable, we should end split view
  // mode, but the cannot snap toast would be inappropriate because the user
  // still might be able to snap `window`.
  if (split_view_controller->IsDividerAnimating()) {
    if (window_state->IsUserPositionable()) {
      split_view_controller->EndSplitView(
          SplitViewController::EndReason::kUnsnappableWindowActivated);
    }
    return false;
  }

  std::optional<float> auto_snap_ratio =
      CalculateAutoSnapRatio(split_view_controller, window);
  // If it's a user positionable window but can't be snapped, end split view
  // mode and show the cannot snap toast.
  if (!auto_snap_ratio) {
    if (window_state->IsUserPositionable()) {
      split_view_controller->EndSplitView(
          SplitViewController::EndReason::kUnsnappableWindowActivated);
      ShowAppCannotSnapToast();
    }
    return false;
  }
  if (!split_view_controller->CanSnapWindow(window, *auto_snap_ratio)) {
    // If the window can't fit in `auto_snap_ratio`, use its minimum size
    // instead.
    // TODO(sophiewen): See if we can do this without recalculating the snap
    // ratio and divider position.
    auto_snap_ratio.emplace(static_cast<float>(GetMinimumWindowLength(
                                window, IsLayoutHorizontal(window))) /
                            static_cast<float>(GetDividerPositionUpperLimit(
                                window->GetRootWindow())));
  }
  // Snap the window on the non-default side of the screen if split view mode
  // is active.
  split_view_controller->SnapWindow(
      window,
      (split_view_controller->default_snap_position() == SnapPosition::kPrimary)
          ? SnapPosition::kSecondary
          : SnapPosition::kPrimary,
      WindowSnapActionSource::kAutoSnapInSplitView,
      /*activate_window=*/false, *auto_snap_ratio);
  return true;
}

void AutoSnapController::AddWindow(aura::Window* window) {
  if (window->GetRootWindow() != root_window_) {
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
