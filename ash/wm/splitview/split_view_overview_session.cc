// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_overview_session.h"

#include <optional>

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/splitview/auto_snap_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/metrics/histogram_functions.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// Histogram names that record presentation time of resize operation with
// following conditions:
// a) clamshell split view, empty overview grid;
// b) clamshell split view, non-empty overview grid;
// c) clamshell split view, two snapped windows (for Snap Groups);
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

// Normally if we are not in clamshell or overview has ended,
// SplitViewOverviewSession would have been ended, however this can be notified
// during mid-drag or mid-resize, so bail out here.
// TODO(b/307631336): Eventually this will be removed in tablet mode.
bool InClamshellSplitViewMode(SplitViewController* controller) {
  // If `kFasterSplitScreenSetup` is enabled, clamshell split view does *not*
  // have to be active.
  // TODO(sophiewen): Consolidate with `kSnapGroup` flag.
  if (features::IsFasterSplitScreenSetupEnabled()) {
    return display::Screen::GetScreen()->GetTabletState() ==
           display::TabletState::kInClamshellMode;
  }
  return controller && controller->InClamshellSplitViewMode() &&
         IsInOverviewSession();
}

}  // namespace

SplitViewOverviewSession::SplitViewOverviewSession(
    aura::Window* window,
    WindowSnapActionSource snap_action_source)
    : window_(window), snap_action_source_(snap_action_source) {
  CHECK(window);
  window_observation_.Observe(window);
  WindowState::Get(window)->AddObserver(this);

  if (window_util::IsFasterSplitScreenOrSnapGroupEnabledInClamshell()) {
    auto_snap_controller_ =
        std::make_unique<AutoSnapController>(window->GetRootWindow());
  }
}

SplitViewOverviewSession::~SplitViewOverviewSession() {
  WindowState::Get(window_)->RemoveObserver(this);
}

void SplitViewOverviewSession::Init(std::optional<OverviewStartAction> action,
                                    std::optional<OverviewEnterExitType> type) {
  // Overview may already be in session, if a window was dragged to split view
  // from overview in clamshell mode.
  if (IsInOverviewSession()) {
    setup_type_ = SplitViewOverviewSetupType::kOverviewThenManualSnap;
    return;
  }

  Shell::Get()->overview_controller()->StartOverview(
      action.value_or(OverviewStartAction::kFasterSplitScreenSetup),
      type.value_or(OverviewEnterExitType::kNormal));
  setup_type_ = SplitViewOverviewSetupType::kSnapThenAutomaticOverview;
}

void SplitViewOverviewSession::RecordSplitViewOverviewSessionExitPointMetrics(
    SplitViewOverviewSessionExitPoint user_action) {
  if (setup_type_ == SplitViewOverviewSetupType::kSnapThenAutomaticOverview) {
    if (user_action ==
            SplitViewOverviewSessionExitPoint::kCompleteByActivating ||
        user_action == SplitViewOverviewSessionExitPoint::kSkip) {
      base::UmaHistogramBoolean(
          BuildWindowLayoutCompleteOnSessionExitHistogram(),
          user_action ==
              SplitViewOverviewSessionExitPoint::kCompleteByActivating);
    }
    base::UmaHistogramEnumeration(
        BuildSplitViewOverviewExitPointHistogramName(snap_action_source_),
        user_action);
  }
}

chromeos::WindowStateType SplitViewOverviewSession::GetWindowStateType() const {
  WindowState* window_state = WindowState::Get(window_);
  CHECK(window_state->IsSnapped());
  return window_state->GetStateType();
}

void SplitViewOverviewSession::OnKeyEvent() {
  MaybeEndOverview(SplitViewOverviewSessionExitPoint::kSkip,
                   OverviewEnterExitType::kImmediateExit);
}

void SplitViewOverviewSession::OnMouseEvent(const ui::MouseEvent& event) {
  aura::Window* target = static_cast<aura::Window*>(event.target());
  if (window_util::GetNonClientComponent(target, event.location()) !=
      HTNOWHERE) {
    // Ignore events in the window caption areas.
    return;
  }
  gfx::Point location_in_screen = event.location();
  wm::ConvertPointToScreen(target, &location_in_screen);
  if (window_->GetBoundsInScreen().Contains(location_in_screen)) {
    MaybeEndOverview(SplitViewOverviewSessionExitPoint::kSkip);
  }
}

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
    Shell::Get()->overview_controller()->EndOverview(
        OverviewEndAction::kSplitView);
    return;
  }

  is_resizing_ = true;
  if (IsSnapGroupEnabledInClamshellMode() &&
      split_view_controller->state() ==
          SplitViewController::State::kBothSnapped) {
    // TODO(b/300180664): Unreached. Move this to SnapGroup.
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

  if (!window_util::IsFasterSplitScreenOrSnapGroupEnabledInClamshell()) {
    split_view_controller->MaybeEndOverviewOnWindowResize(window);
  }

  is_resizing_ = false;
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
      Shell::Get()->overview_controller()->EndOverview(
          OverviewEndAction::kSplitView);
      return;
    }
    if (!is_resizing_) {
      return;
    }
    CHECK(window_state->drag_details()->bounds_change &
          WindowResizer::kBoundsChange_Resizes);
    CHECK(presentation_time_recorder_);
    presentation_time_recorder_->RequestNext();
  }

  if (window_util::IsFasterSplitScreenOrSnapGroupEnabledInClamshell() &&
      IsInOverviewSession()) {
    // When `FasterSplitScreenSetup` or `SnapGroup` is enabled, we need to
    // manually refresh the grid bounds, because `OverviewGrid` will calculate
    // the bounds based on `SplitViewController::divider_position_` which
    // wouldn't work for multiple groups.
    GetOverviewSession()
        ->GetGridWithRootWindow(window->GetRootWindow())
        ->RefreshGridBounds(/*animate=*/false);
    return;
  }

  // SplitViewController will update the divider position and notify observers
  // to update their bounds.
  // TODO(b/296935443): Remove this when bounds calculations are refactored out.
  // We should notify and update observer bounds directly rather than relying on
  // SplitViewController to update `divider_position_`.
  split_view_controller->UpdateDividerPositionOnWindowResize(window,
                                                             new_bounds);
}

void SplitViewOverviewSession::OnWindowDestroying(aura::Window* window) {
  CHECK(window_observation_.IsObservingSource(window));
  CHECK_EQ(window_, window);
  MaybeEndOverview(SplitViewOverviewSessionExitPoint::kWindowDestroy);
}

void SplitViewOverviewSession::OnPreWindowStateTypeChange(
    WindowState* window_state,
    chromeos::WindowStateType old_type) {
  CHECK_EQ(window_, window_state->window());
  // Normally split view would have ended and destroy this, but the window can
  // get unsnapped, e.g. during mid-drag or mid-resize, so bail out here.
  if (!window_state->IsSnapped()) {
    MaybeEndOverview(SplitViewOverviewSessionExitPoint::kUnspecified);
  }
}

void SplitViewOverviewSession::MaybeEndOverview(
    SplitViewOverviewSessionExitPoint exit_point,
    OverviewEnterExitType exit_type) {
  if (window_util::IsFasterSplitScreenOrSnapGroupEnabledInClamshell()) {
    // If `FasterSplitScreenOrSnapGroup` is enabled, end full overview.
    RecordSplitViewOverviewSessionExitPointMetrics(exit_point);
    // `EndOverview()` will also destroy `this`.
    Shell::Get()->overview_controller()->EndOverview(
        OverviewEndAction::kSplitView, exit_type);
    return;
  }
  // Otherwise simply end `SplitViewOverviewSession` and remain in overview.
  // TODO(sophiewen): Fix tests that expect overview to still be active after
  // `FasterSplitScreenOrSnapGroup` is enabled.
  RootWindowController::ForWindow(window_)->EndSplitViewOverviewSession(
      exit_point);
}

}  // namespace ash
