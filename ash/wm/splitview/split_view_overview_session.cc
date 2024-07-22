// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_overview_session.h"

#include <optional>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/metrics/histogram_functions.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
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
  CHECK(!Shell::Get()->IsInTabletMode());
  window_observation_.Observe(window);
  WindowState::Get(window)->AddObserver(this);
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

  // Set the type before we start overview, which will initialize the grid and
  // check whether to create the desk bar and buttons based on `setup_type_`.
  setup_type_ = SplitViewOverviewSetupType::kSnapThenAutomaticOverview;
  OverviewController::Get()->StartOverview(
      action.value_or(OverviewStartAction::kFasterSplitScreenSetup),
      type.value_or(OverviewEnterExitType::kNormal));

  Shell::Get()->accessibility_controller()->TriggerAccessibilityAlert(
      AccessibilityAlert::FASTER_SPLIT_SCREEN_SETUP);
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

void SplitViewOverviewSession::HandleClickOrTap(const ui::LocatedEvent& event) {
  if (event.type() != ui::EventType::kMousePressed &&
      event.type() != ui::EventType::kTouchReleased) {
    return;
  }

  aura::Window* target = static_cast<aura::Window*>(event.target());
  if (target != window_) {
    // The target might be in the window layout menu, not `window_` itself, in
    // which case we don't need to handle it and end overview.
    return;
  }

  const int client_component =
      window_util::GetNonClientComponent(target, event.location());
  if (client_component != HTCLIENT && client_component != HTCAPTION) {
    return;
  }

  gfx::Point location_in_screen = event.location();
  wm::ConvertPointToScreen(target, &location_in_screen);
  if (window_->GetBoundsInScreen().Contains(location_in_screen)) {
    MaybeEndOverview(SplitViewOverviewSessionExitPoint::kSkip,
                     OverviewEnterExitType::kNormal);
  }
}

void SplitViewOverviewSession::OnResizeLoopStarted(aura::Window* window) {
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
  presentation_time_recorder_.reset();

  // TODO(sophiewen): Only used by metrics. See if we can remove this.
  aura::Window* root_window = window->GetRootWindow();
  SplitViewController::Get(root_window)->NotifyWindowResized();

  is_resizing_ = false;

  // The resize behavior design between two setup types differs, in faster split
  // screen setup session, we don't need to consider ending overview if divider
  // position is outside the fixed position.
  if (setup_type_ == SplitViewOverviewSetupType::kSnapThenAutomaticOverview) {
    return;
  }

  // For `SplitViewOverviewSetupType::kOverviewThenManualSnap`, end overview if
  // the divider position is outside the fixed positions.
  const int work_area_length = GetDividerPositionUpperLimit(root_window);
  const int window_length =
      GetWindowLength(window, IsLayoutHorizontal(root_window));
  if (window_length < work_area_length * chromeos::kOneThirdSnapRatio ||
      window_length > work_area_length * chromeos::kTwoThirdSnapRatio) {
    WindowState::Get(window)->Maximize();
    // `EndOverview()` will destroy `this`.
    OverviewController::Get()->EndOverview(OverviewEndAction::kSplitView);
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

  // Overview may be ending, during which we don't need to update the
  // window or grid bounds. `this` will be destroyed soon.
  if (!IsInOverviewSession()) {
    return;
  }

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
}

void SplitViewOverviewSession::OnWindowDestroying(aura::Window* window) {
  CHECK(window_observation_.IsObservingSource(window));
  CHECK_EQ(window_, window);
  MaybeEndOverview(SplitViewOverviewSessionExitPoint::kWindowDestroy,
                   OverviewEnterExitType::kNormal);
}

void SplitViewOverviewSession::OnPreWindowStateTypeChange(
    WindowState* window_state,
    chromeos::WindowStateType old_type) {
  CHECK_EQ(window_, window_state->window());
  // Normally split view would have ended and destroy this, but the window can
  // get unsnapped, e.g. during mid-drag or mid-resize, so bail out here.
  if (!window_state->IsSnapped()) {
    MaybeEndOverview(SplitViewOverviewSessionExitPoint::kUnspecified,
                     OverviewEnterExitType::kNormal);
  }
}

void SplitViewOverviewSession::MaybeEndOverview(
    SplitViewOverviewSessionExitPoint exit_point,
    OverviewEnterExitType exit_type) {
  if (setup_type_ == SplitViewOverviewSetupType::kSnapThenAutomaticOverview) {
    // End overview for faster split screen setup, which will eventually destroy
    // `this`.
    RecordSplitViewOverviewSessionExitPointMetrics(exit_point);
    OverviewController::Get()->EndOverview(OverviewEndAction::kSplitView,
                                           exit_type);
  } else {
    // Or just end `this` if overview was active before snapping the `window_`,
    // in which case overview will still be active.
    RootWindowController::ForWindow(window_)->EndSplitViewOverviewSession(
        exit_point);
  }
}

}  // namespace ash
