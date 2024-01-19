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
// b) clamshell split view, non-empty overview grid.
constexpr char kClamshellSplitViewResizeSingleHistogram[] =
    "Ash.SplitViewResize.PresentationTime.ClamshellMode.SingleWindow";
constexpr char kClamshellSplitViewResizeWithOverviewHistogram[] =
    "Ash.SplitViewResize.PresentationTime.ClamshellMode.WithOverview";
constexpr char kClamshellSplitViewResizeSingleMaxLatencyHistogram[] =
    "Ash.SplitViewResize.PresentationTime.MaxLatency.ClamshellMode."
    "SingleWindow";
constexpr char kClamshellSplitViewResizeWithOverviewMaxLatencyHistogram[] =
    "Ash.SplitViewResize.PresentationTime.MaxLatency.ClamshellMode."
    "WithOverview";

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
  if (event.type() != ui::ET_MOUSE_PRESSED ||
      window_util::GetNonClientComponent(target, event.location()) !=
          HTCLIENT) {
    // Only MOUSE_PRESSED events in the client hit area will end overview.
    return;
  }
  gfx::Point location_in_screen = event.location();
  wm::ConvertPointToScreen(target, &location_in_screen);
  if (window_->GetBoundsInScreen().Contains(location_in_screen)) {
    MaybeEndOverview(SplitViewOverviewSessionExitPoint::kSkip);
  }
}

void SplitViewOverviewSession::OnResizeLoopStarted(aura::Window* window) {
  // Tablet mode resize will rely on the
  // `SplitViewController::StartResizeWithDivider()`.
  if (Shell::Get()->IsInTabletMode()) {
    return;
  }

  // In clamshell mode, if splitview is active (which means overview is active
  // at the same time), only the resize that happens on the window edge that's
  // next to the overview grid will resize the window and overview grid at the
  // same time. For the resize that happens on the other part of the window,
  // we'll just end splitview and overview mode.
  if (WindowState::Get(window)->drag_details()->window_component !=
      GetWindowComponentForResize(window)) {
    OverviewController::Get()->EndOverview(OverviewEndAction::kSplitView);
    return;
  }

  is_resizing_ = true;

  OverviewSession* overview_session = GetOverviewSession();
  CHECK(overview_session);
  if (overview_session->GetGridWithRootWindow(window->GetRootWindow())
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
  if (Shell::Get()->IsInTabletMode()) {
    return;
  }

  presentation_time_recorder_.reset();

  // TODO(sophiewen): Only used by metrics. See if we can remove this.
  aura::Window* root_window = window->GetRootWindow();
  SplitViewController::Get(root_window)->NotifyWindowResized();

  is_resizing_ = false;

  if (window_util::IsFasterSplitScreenOrSnapGroupEnabledInClamshell()) {
    return;
  }

  // When `FasterSplitScreenOrSnapGroup` is disabled, end overview if the
  // divider position is outside the fixed positions.
  const int work_area_length = GetDividerPositionUpperLimit(root_window);
  const int window_length =
      GetWindowLength(window, IsLayoutHorizontal(root_window));
  if (window_length < work_area_length * chromeos::kOneThirdSnapRatio ||
      window_length > work_area_length * chromeos::kTwoThirdSnapRatio) {
    WindowState::Get(window)->Maximize();
    // `EndOverview()` will destroy `this`.
    Shell::Get()->overview_controller()->EndOverview(
        OverviewEndAction::kSplitView);
  }
}

void SplitViewOverviewSession::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (WindowState* window_state = WindowState::Get(window);
      window_state->is_dragged()) {
    CHECK_NE(WindowResizer::kBoundsChange_None,
             window_state->drag_details()->bounds_change);
    if (window_state->drag_details()->bounds_change ==
        WindowResizer::kBoundsChange_Repositions) {
      OverviewController::Get()->EndOverview(OverviewEndAction::kSplitView);
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

  if (!Shell::Get()->IsInTabletMode()) {
    CHECK(IsInOverviewSession());
    // When in clamshell `SplitViewOverviewSession`, we need to manually refresh
    // the grid bounds, because `OverviewGrid` will calculate the bounds based
    // on `SplitViewController::divider_position_` which wouldn't work for
    // multiple groups.
    // TODO(michelefan | sophiewen): Reconsider the ownership of the session
    // and generalize the `OverviewGrid` bounds calculation to be independent
    // from `SplitViewController`.
    GetOverviewSession()
        ->GetGridWithRootWindow(window->GetRootWindow())
        ->RefreshGridBounds(/*animate=*/false);
    return;
  }
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
