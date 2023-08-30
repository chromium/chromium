// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_overview_session.h"

#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ui/compositor/layer.h"

namespace ash {

namespace {

// Histogram names that record presentation time of resize operation with
// following conditions:
// a) clamshell split view, empty overview grid;
// b) clamshell split view, nonempty overview grid;
// c) clamshell split view, two snapped windows;
constexpr char kClamshellSplitViewResizeSingleHistogram[] =
    "Ash.SplitViewResize.PresentationTime.ClamshellMode.SingleWindow";
constexpr char kClamshellSplitViewResizeMultiHistogram[] =
    "Ash.SplitViewResize.PresentationTime.ClamshellMode.MultiWindow";
constexpr char kClamshellSplitViewResizeWithOverviewHistogram[] =
    "Ash.SplitViewResize.PresentationTime.ClamshellMode.WithOverview";

constexpr char kClamshellSplitViewResizeSingleMaxLatencyHistogram[] =
    "Ash.SplitViewResize.PresentationTime.MaxLatency.ClamshellMode."
    "SingleWindow";
constexpr char kClamshellSplitViewResizeWithOverviewMaxLatencyHistogram[] =
    "Ash.SplitViewResize.PresentationTime.MaxLatency.ClamshellMode."
    "WithOverview";

bool InClamshellSplitViewMode(SplitViewController* controller) {
  return controller && controller->InClamshellSplitViewMode() &&
         GetOverviewSession();
}

}  // namespace

SplitViewOverviewSession::SplitViewOverviewSession(aura::Window* window) {
  CHECK(window);
  window_observation_.Observe(window);
}

SplitViewOverviewSession::~SplitViewOverviewSession() = default;

void SplitViewOverviewSession::OnResizeLoopStarted(aura::Window* window) {
  auto* split_view_controller =
      SplitViewController::Get(window->GetRootWindow());
  // TODO(sophiewen): Check needed since `this` is created by split view. When
  // Snap Groups is enabled, this can be created directly in
  // SnapGroupController.
  if (!InClamshellSplitViewMode(split_view_controller)) {
    return;
  }

  // In clamshell mode, if splitview is active (which means overview is active
  // at the same time or the feature flag `kSnapGroup` is enabled and
  // `kAutomaticallyLockGroup` is true, only the resize that happens on the
  // window edge that's next to the overview grid will resize the window and
  // overview grid at the same time. For the resize that happens on the other
  // part of the window, we'll just end splitview and overview mode.
  if (WindowState::Get(window)->drag_details()->window_component !=
      GetWindowComponentForResize(window)) {
    // Ending overview will also end clamshell split view unless
    // `SnapGroupController::IsArm1AutomaticallyLockEnabled()` returns true.
    Shell::Get()->overview_controller()->EndOverview(
        OverviewEndAction::kSplitView);
    return;
  }

  if (IsSnapGroupEnabledInClamshellMode() &&
      split_view_controller->state() ==
          SplitViewController::State::kBothSnapped) {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        window->layer()->GetCompositor(),
        kClamshellSplitViewResizeMultiHistogram,
        kClamshellSplitViewResizeSingleMaxLatencyHistogram);
    return;
  }

  CHECK(GetOverviewSession());
  if (GetOverviewSession()
          ->GetGridWithRootWindow(window->GetRootWindow())
          ->empty()) {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        window->layer()->GetCompositor(),
        kClamshellSplitViewResizeSingleHistogram,
        kClamshellSplitViewResizeSingleMaxLatencyHistogram);
  } else {
    presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
        window->layer()->GetCompositor(),
        kClamshellSplitViewResizeWithOverviewHistogram,
        kClamshellSplitViewResizeWithOverviewMaxLatencyHistogram);
  }
}

void SplitViewOverviewSession::OnResizeLoopEnded(aura::Window* window) {
  auto* split_view_controller =
      SplitViewController::Get(window->GetRootWindow());
  if (!InClamshellSplitViewMode(split_view_controller)) {
    return;
  }

  presentation_time_recorder_.reset();

  // TODO(sophiewen): Only used by metrics. See if we can remove this.
  split_view_controller->NotifyWindowResized();

  split_view_controller->MaybeEndOverviewOnWindowResize(window);
}

void SplitViewOverviewSession::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  auto* split_view_controller =
      SplitViewController::Get(window->GetRootWindow());
  if (!InClamshellSplitViewMode(split_view_controller)) {
    return;
  }

  if (IsSnapGroupEnabledInClamshellMode() &&
      split_view_controller->BothSnapped()) {
    // When the second window is snapped in a snap group, we *don't* want to
    // override `divider_position_` with `new_bounds` below, which don't take
    // into account the divider width.
    return;
  }

  WindowState* window_state = WindowState::Get(window);
  if (window_state->is_dragged()) {
    CHECK_NE(WindowResizer::kBoundsChange_None,
             window_state->drag_details()->bounds_change);
    if (window_state->drag_details()->bounds_change ==
        WindowResizer::kBoundsChange_Repositions) {
      // Ending overview will also end clamshell split view unless
      // `SnapGroupController::IsArm1AutomaticallyLockEnabled()` returns true.
      Shell::Get()->overview_controller()->EndOverview(
          OverviewEndAction::kSplitView);
      return;
    }
    CHECK(window_state->drag_details()->bounds_change &
          WindowResizer::kBoundsChange_Resizes);
    CHECK(presentation_time_recorder_);
    presentation_time_recorder_->RequestNext();
  }

  // SplitViewController will update the divider position and notify observers
  // to update their bounds.
  // TODO(b/296935443): Remove this when bounds calculations are refactored out.
  // We should notify and update observer bounds directly rather than relying on
  // SplitViewController to update `divider_position_`.
  split_view_controller->UpdateDividerPositionOnWindowResize(window,
                                                             new_bounds);
}

}  // namespace ash
