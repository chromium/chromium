// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/swipe_home_to_overview_controller.h"

#include <algorithm>
#include <optional>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/controls/contextual_tooltip.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/drag_window_from_shelf_controller.h"
#include "ash/shelf/shelf_metrics.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_session.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/default_tick_clock.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/tween.h"

namespace ash {

namespace {

// The target/min home launcher view scale.
constexpr float kTargetHomeScale = 0.92f;

// The home UI will be scaled down towards center of the screen as drag location
// moves upwards. The target threshold for scaling is extended above the actual
// threshold for transition so the UI keeps changing even when the gesture goes
// over the threshold. This is the target home screen scaling threshold in terms
// of ratio of the display height.
constexpr float kHomeScalingThresholdDisplayHeightRatio = 0.5f;

// The amount of time the drag has to remain bellow velocity threshold before
// the transition to the overview starts.
constexpr base::TimeDelta kOverviewTransitionDelay = base::Milliseconds(150);

// The duration of transition from the home screen current scaled state to the
// initial (unscaled) state when the gesture is canceled.
constexpr base::TimeDelta kGestureCancelationDuration = base::Milliseconds(350);

// The duration of transition from the home screen current scaled state to the
// initial (unscaled) state when the gesture is canceled due to a back gesture.
constexpr base::TimeDelta kGestureCancelationForBackDuration =
    base::Milliseconds(250);

void UpdateHomeAnimationForGestureCancel(
    bool going_back,
    ui::ScopedLayerAnimationSettings* settings) {
  settings->SetTransitionDuration(going_back
                                      ? kGestureCancelationForBackDuration
                                      : kGestureCancelationDuration);
  settings->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  settings->SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
}

}  // namespace

SwipeHomeToOverviewController::SwipeHomeToOverviewController(int64_t display_id)
    : SwipeHomeToOverviewController(display_id,
                                    base::DefaultTickClock::GetInstance()) {}

SwipeHomeToOverviewController::SwipeHomeToOverviewController(
    int64_t display_id,
    const base::TickClock* tick_clock)
    : display_id_(display_id), overview_transition_timer_(tick_clock) {}

SwipeHomeToOverviewController::~SwipeHomeToOverviewController() {
  CancelDrag();
}

void SwipeHomeToOverviewController::Drag(const gfx::PointF& location_in_screen,
                                         float scroll_x,
                                         float scroll_y) {
  if (state_ == State::kFinished)
    return;

  display::Display display;
  if (!display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id_,
                                                             &display)) {
    CancelDrag();
    return;
  }

  const int shelf_top =
      display.bounds().bottom() - ShelfConfig::Get()->shelf_size();
  if (state_ == State::kInitial) {
    // Do not start drag until the drag goes above the shelf.
    if (location_in_screen.y() > shelf_top)
      return;

    overview_transition_threshold_y_ =
        shelf_top - kVerticalThresholdForOverviewTransition;
    scaling_threshold_y_ =
        display.bounds().y() +
        display.bounds().height() * kHomeScalingThresholdDisplayHeightRatio;
    state_ = State::kTrackingDrag;
  } else {
    if (location_in_screen.y() <= overview_transition_threshold_y_ &&
        std::abs(scroll_x) + std::abs(scroll_y) <= kMovementVelocityThreshold) {
      ScheduleFinalizeDragAndShowOverview();
    } else {
      overview_transition_timer_.Stop();
    }
  }

  // Update the home screen scale to match progress during the drag.
  // Use extended threshold as the projected final transition position - UI
  // changing even after the user gets over the threshold should make the user
  // more likely to keep dragging up when they get really close to the threshold
  // for transition to overview (and reduce false negatives for detecting
  // transition to overview).
  const float distance = location_in_screen.y() - scaling_threshold_y_;
  const float target_distance = overview_transition_threshold_y_ -
                                scaling_threshold_y_ +
                                kVerticalThresholdForOverviewTransition;

  const float progress = gfx::Tween::CalculateValue(
      gfx::Tween::FAST_OUT_SLOW_IN,
      std::clamp(1.f - distance / target_distance, 0.0f, 1.0f));

  float scale = gfx::Tween::FloatValueBetween(progress, 1.0f, kTargetHomeScale);
  Shell::Get()->app_list_controller()->UpdateScaleAndOpacityForHomeLauncher(
      scale, 1.0f /*opacity*/, std::nullopt /*animation_info*/,
      base::NullCallback());
}

void SwipeHomeToOverviewController::EndDrag(
    const gfx::PointF& location_in_screen,
    std::optional<float> velocity_y) {
  if (state_ != State::kTrackingDrag) {
    state_ = State::kFinished;
    return;
  }

  // Upward swipe should return to the home screen's initial state.
  const bool go_back =
      velocity_y &&
      *velocity_y <
          -DragWindowFromShelfController::kVelocityToHomeScreenThreshold;

  // Overview is triggered by |overview_transition_timer_|. If EndDrag()
  // is called before the timer fires, the result of the gesture should be
  // staying on the home screen.
  FinalizeDragAndStayOnHomeScreen(go_back);
}

void SwipeHomeToOverviewController::CancelDrag() {
  if (state_ != State::kTrackingDrag) {
    state_ = State::kFinished;
    return;
  }

  FinalizeDragAndStayOnHomeScreen(/*go_back=*/false);
}

void SwipeHomeToOverviewController::ScheduleFinalizeDragAndShowOverview() {
  if (overview_transition_timer_.IsRunning())
    return;

  overview_transition_timer_.Start(
      FROM_HERE, kOverviewTransitionDelay,
      base::BindOnce(
          &SwipeHomeToOverviewController::FinalizeDragAndShowOverview,
          base::Unretained(this)));
}

void SwipeHomeToOverviewController::FinalizeDragAndShowOverview() {
  state_ = State::kFinished;
  overview_transition_threshold_y_ = 0;

  if (features::IsHideShelfControlsInTabletModeEnabled()) {
    contextual_tooltip::HandleGesturePerformed(
        Shell::Get()->session_controller()->GetActivePrefService(),
        contextual_tooltip::TooltipType::kHomeToOverview);
  }

  UMA_HISTOGRAM_ENUMERATION(kEnterOverviewHistogramName,
                            EnterOverviewFromHomeLauncher::kOverview);

  // NOTE: No need to update the home launcher opacity and scale here - the
  // AppListControllerImpl will update the home launcher state when it detects
  // that the overview is starting.
  Shell::Get()->overview_controller()->StartOverview(
      OverviewStartAction::kExitHomeLauncher);
}

void SwipeHomeToOverviewController::FinalizeDragAndStayOnHomeScreen(
    bool go_back) {
  overview_transition_timer_.Stop();
  overview_transition_threshold_y_ = 0;
  state_ = State::kFinished;

  // App list controller may get destroyed before shelf during shutdown.
  auto* const app_list_controller = Shell::Get()->app_list_controller();
  if (!app_list_controller) {
    return;
  }

  if (go_back) {
    app_list_controller->Back();
    UMA_HISTOGRAM_ENUMERATION(kEnterOverviewHistogramName,
                              EnterOverviewFromHomeLauncher::kBack);
  } else {
    UMA_HISTOGRAM_ENUMERATION(kEnterOverviewHistogramName,
                              EnterOverviewFromHomeLauncher::kCanceled);
  }

  // Make sure the home launcher scale and opacity return to the initial state.
  // Note that this is needed even if the gesture ended up in a fling, as early
  // gesture handling might have updated the launcher scale.
  app_list_controller->UpdateScaleAndOpacityForHomeLauncher(
      1.0f /*scale*/, 1.0f /*opacity*/, std::nullopt /*animation_info*/,
      base::BindRepeating(&UpdateHomeAnimationForGestureCancel, go_back));
}

}  // namespace ash
