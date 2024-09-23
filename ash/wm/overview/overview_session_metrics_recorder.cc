// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_session_metrics_recorder.h"

#include "ash/shell.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_types.h"
#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/named_trigger.h"
#include "base/trace_event/trace_event.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/presentation_time_recorder.h"

namespace ash {

OverviewSessionMetricsRecorder::OverviewSessionMetricsRecorder(
    OverviewStartAction start_action,
    OverviewController* controller)
    : start_action_(start_action) {
  RecordOverviewStartAction(start_action);
  controller_observation_.Observe(controller);
}

OverviewSessionMetricsRecorder::~OverviewSessionMetricsRecorder() {
  // Corner case: If an overview session is started while one is in progress,
  // then the recorder for the first session is destroyed before the
  // `OnOverviewModeEndingAnimationComplete()` notification is received. This
  // case needs to be checked explicitly.
  if (!has_finished_exit_overview_trace_event_) {
    OnOverviewModeEndingAnimationComplete(/*canceled=*/true);
  }
}

void OverviewSessionMetricsRecorder::OnOverviewSessionInitializing() {
  overview_start_time_ = base::Time::Now();
  // We're only interested in chrometto traces where the user definitely intends
  // to see an overview of their windows. These two are the most common use
  // cases in the field, accounting for a combined 85% of overview sessions in
  // stable channel as of 8/29/24.
  if (start_action_ == OverviewStartAction::kAccelerator ||
      start_action_ == OverviewStartAction::kDragWindowFromShelf) {
    base::trace_event::EmitNamedTrigger("ash-overview-start");
  }
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ui", "OverviewController::EnterOverview",
                                    this);

  enter_presentation_time_recorder_ = CreatePresentationTimeHistogramRecorder(
      Shell::GetPrimaryRootWindow()->layer()->GetCompositor(),
      kEnterOverviewPresentationHistogram, "",
      GetOverviewPresentationTimeBucketParams(),
      /*emit_trace_event=*/true);
  enter_presentation_time_recorder_->RequestNext();

  base::UmaHistogramCounts100("Ash.Overview.DeskCount",
                              DesksController::Get()->desks().size());
}

void OverviewSessionMetricsRecorder::OnOverviewSessionInitialized(
    OverviewSession* session) {
  session_ = session;
  CHECK(session_);
  desk_bar_shown_immediately_ = IsDeskBarOpen();
}

void OverviewSessionMetricsRecorder::OnOverviewSessionEnding() {
  RecordOverviewEndAction(session_->overview_end_action());

  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ui", "OverviewController::ExitOverview",
                                    this);

  const DeskBarVisibility desk_bar_visibility =
      desk_bar_shown_immediately_
          ? DeskBarVisibility::kShownImmediately
          : (IsDeskBarOpen() ? DeskBarVisibility::kShownAfterFirstFrame
                             : DeskBarVisibility::kNotShown);
  base::UmaHistogramEnumeration("Ash.Overview.DeskBarVisibility",
                                desk_bar_visibility);

  auto exit_presentation_time_recorder =
      CreatePresentationTimeHistogramRecorder(
          Shell::GetPrimaryRootWindow()->layer()->GetCompositor(),
          kExitOverviewPresentationHistogram, "",
          GetOverviewPresentationTimeBucketParams(),
          /*emit_trace_event=*/true);
  exit_presentation_time_recorder->RequestNext();

  CHECK(enter_presentation_time_recorder_);
  const std::optional<base::TimeDelta> enter_presentation_time =
      enter_presentation_time_recorder_->GetAverageLatency();
  // `enter_presentation_time` can be null if the overview session ends before
  // the first overview frame is presented. This should be rare and is ok to
  // ignore.
  if (enter_presentation_time) {
    RecordOverviewEnterPresentationTimeWithReason(start_action_,
                                                  *enter_presentation_time);
  }

  if (IsRenderingDeskBarWithMiniViews()) {
    SchedulePresentationTimeMetricsWithDeskBar(
        std::move(enter_presentation_time_recorder_),
        std::move(exit_presentation_time_recorder), desk_bar_visibility);
  }

  CHECK(!overview_start_time_.is_null());
  base::UmaHistogramMediumTimes("Ash.Overview.TimeInOverview",
                                base::Time::Now() - overview_start_time_);
  // Prevents dangling raw_ptr failures.
  session_ = nullptr;
}

void OverviewSessionMetricsRecorder::OnOverviewModeStartingAnimationComplete(
    bool canceled) {
  TRACE_EVENT_NESTABLE_ASYNC_END1("ui", "OverviewController::EnterOverview",
                                  this, "canceled", canceled);
}

void OverviewSessionMetricsRecorder::OnOverviewModeEndingAnimationComplete(
    bool canceled) {
  has_finished_exit_overview_trace_event_ = true;
  TRACE_EVENT_NESTABLE_ASYNC_END1("ui", "OverviewController::ExitOverview",
                                  this, "canceled", canceled);
}

bool OverviewSessionMetricsRecorder::IsDeskBarOpen() const {
  CHECK(session_);
  return session_->GetGridWithRootWindow(Shell::GetPrimaryRootWindow())
      ->desks_bar_view();
}

bool OverviewSessionMetricsRecorder::IsRenderingDeskBarWithMiniViews() const {
  return IsDeskBarOpen() &&
         !session_->GetGridWithRootWindow(Shell::GetPrimaryRootWindow())
              ->desks_bar_view()
              ->mini_views()
              .empty();
}

}  // namespace ash
