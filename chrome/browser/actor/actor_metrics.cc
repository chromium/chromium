// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_metrics.h"

#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/browser/actor/actor_task.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"

namespace actor {

namespace {
std::string_view ToString(ActorTask::StoppedReason stopped_reason) {
  switch (stopped_reason) {
    case ActorTask::StoppedReason::kStoppedByUser:
      return "Cancelled";
    case ActorTask::StoppedReason::kTaskComplete:
      return "Completed";
    case ActorTask::StoppedReason::kModelError:
      return "ModelError";
    case ActorTask::StoppedReason::kChromeFailure:
      return "ChromeFailure";
    case ActorTask::StoppedReason::kTabDetached:
      return "TabDetached";
    case ActorTask::StoppedReason::kShutdown:
      return "Shutdown";
    case ActorTask::StoppedReason::kUserStartedNewChat:
      return "NewChat";
    case ActorTask::StoppedReason::kUserLoadedPreviousChat:
      return "PreviousChat";
  }
  NOTREACHED();
}
}  // namespace

void RecordActorTaskStateTransitionActionCount(size_t action_count,
                                               ActorTask::State from_state,
                                               ActorTask::State to_state) {
  base::UmaHistogramCounts1000(
      base::StrCat({"Actor.Task.StateTransition.ActionCount.",
                    ToString(from_state), "_", ToString(to_state)}),
      action_count);
}

void RecordActorTaskStateTransitionDuration(base::TimeDelta duration,
                                            ActorTask::State state) {
  base::UmaHistogramLongTimes100(
      base::StrCat({"Actor.Task.StateTransition.Duration.", ToString(state)}),
      duration);
}

void RecordToolTimings(std::string_view tool_name,
                       base::TimeDelta execution_duration,
                       base::TimeDelta page_stabilization_duration) {
  base::UmaHistogramMediumTimes(
      base::StrCat({"Actor.Tools.ExecutionDuration.", tool_name}),
      execution_duration);
  base::UmaHistogramMediumTimes(
      base::StrCat({"Actor.Tools.PageStabilization.", tool_name}),
      page_stabilization_duration);
}

void RecordActorTaskVisibilityDurationHistograms(
    base::TimeDelta visible_duration,
    base::TimeDelta non_visible_duration,
    ActorTask::StoppedReason stopped_reason) {
  base::UmaHistogramLongTimes100(
      base::StrCat({"Actor.Task.Duration.Visible.", ToString(stopped_reason)}),
      visible_duration);

  base::UmaHistogramLongTimes100(
      base::StrCat(
          {"Actor.Task.Duration.NotVisible.", ToString(stopped_reason)}),
      non_visible_duration);
}

void RecordActorTaskCompletion(ActorTask::StoppedReason stopped_reason,
                               base::TimeDelta total_time,
                               base::TimeDelta controlled_time,
                               size_t interruptions_count,
                               size_t actions_count) {
  base::UmaHistogramLongTimes100(base::StrCat({"Actor.Task.Duration.WallClock.",
                                               ToString(stopped_reason)}),
                                 total_time);
  base::UmaHistogramLongTimes100(
      base::StrCat({"Actor.Task.Duration.", ToString(stopped_reason)}),
      controlled_time);
  base::UmaHistogramCounts1000(
      base::StrCat({"Actor.Task.Interruptions.", ToString(stopped_reason)}),
      interruptions_count);
  base::UmaHistogramCounts1000(
      base::StrCat({"Actor.Task.Count.", ToString(stopped_reason)}),
      actions_count);
  base::UmaHistogramEnumeration("Actor.Task.StoppedReason", stopped_reason);
}

void RecordActorTaskCreated(bool success) {
  base::UmaHistogramBoolean("Actor.Task.Created", success);
}

void RecordActionResultCode(actor::mojom::ActionResultCode action_result_code) {
  // Note: Uses a sparse histogram instead of a linear (i.e. enumeration)
  // histogram here because, the linear histograms are limited to 1000 values in
  // base/metrics/histogram.cc.
  base::UmaHistogramSparse("Actor.ExecutionEngine.Action.ResultCode",
                           std::to_underlying(action_result_code));
}

void RecordPageContextApcDuration(base::TimeDelta duration) {
  base::UmaHistogramMediumTimes("Actor.PageContext.APC.Duration", duration);
}

void RecordPageContextScreenshotDuration(base::TimeDelta duration) {
  base::UmaHistogramMediumTimes("Actor.PageContext.Screenshot.Duration",
                                duration);
}

void RecordPageContextTabCount(size_t tab_count) {
  base::UmaHistogramCounts1000("Actor.PageContext.TabCount", tab_count);
}

void RecordDirectDownloadTriggered(bool success) {
  base::UmaHistogramBoolean("Actor.Download.DirectDownloadTriggered", success);
}

void RecordDownloadSaveAsDialogTriggered(bool success) {
  base::UmaHistogramBoolean("Actor.Download.SaveAsDialogTriggered", success);
}

void RecordActorNavigationGatingListSize(size_t allow_list_size,
                                         size_t confirmed_list_size) {
  base::UmaHistogramCounts1000("Actor.NavigationGating.AllowListSize",
                               allow_list_size);
  base::UmaHistogramCounts1000("Actor.NavigationGating.ConfirmedListSize2",
                               confirmed_list_size);
}

void RecordNavigationGatingDecision(ExecutionEngine::GatingDecision decision) {
  base::UmaHistogramEnumeration("Actor.NavigationGating.GatingDecision",
                                decision);
}

// Records the outcome of an post-performActions observation fetch.
void RecordObservationOutcomeHistogram(
    const optimization_guide::proto::ActionsResult& result,
    bool is_for_retry) {
  using optimization_guide::proto::TabObservation;

  bool success = true;
  for (const TabObservation& tab_observation : result.tabs()) {
    CHECK(tab_observation.has_result(), base::NotFatalUntil::M147);
    if (tab_observation.result() != TabObservation::TAB_OBSERVATION_OK) {
      success = false;
      break;
    }
  }

  std::optional<ActorObservationOutcome> outcome;
  if (success) {
    outcome = is_for_retry ? ActorObservationOutcome::kSuccessAfterRetry
                           : ActorObservationOutcome::kSuccess;
  } else {
    outcome = is_for_retry ? ActorObservationOutcome::kFailureAfterRetry
                           : ActorObservationOutcome::kFailure;
  }

  base::UmaHistogramEnumeration(kActorPageContextObservationOutcome, *outcome);
}

// Records the outcome of an post-performActions observation fetch.
void RecordTabObservationResultHistogram(
    const optimization_guide::proto::ActionsResult& result) {
  using optimization_guide::proto::TabObservation;

  for (const TabObservation& tab_observation : result.tabs()) {
    std::optional<ActorTabObservationResult> tab_result;
    CHECK(tab_observation.has_result(), base::NotFatalUntil::M147);
    switch (tab_observation.result()) {
      case TabObservation::TAB_OBSERVATION_OK:
        tab_result = ActorTabObservationResult::kSuccess;
        break;
      case TabObservation::TAB_OBSERVATION_TAB_WENT_AWAY:
        tab_result = ActorTabObservationResult::kTabWentAway;
        break;
      case TabObservation::TAB_OBSERVATION_SCREENSHOT_TIMEOUT:
        // Deprecated and unused
        NOTREACHED(base::NotFatalUntil::M147);
        break;
      case TabObservation::TAB_OBSERVATION_PAGE_CRASHED:
        tab_result = ActorTabObservationResult::kPageCrashed;
        break;
      case TabObservation::TAB_OBSERVATION_UNKNOWN_ERROR:
        tab_result = ActorTabObservationResult::kUnknown;
        break;
      case TabObservation::TAB_OBSERVATION_WEB_CONTENTS_CHANGED:
        tab_result = ActorTabObservationResult::kWebContentsChanged;
        break;
      case TabObservation::TAB_OBSERVATION_PAGE_CONTEXT_NOT_ELIGIBLE:
        tab_result = ActorTabObservationResult::kPageContextNotEligible;
        break;
      case TabObservation::TAB_OBSERVATION_FETCH_ERROR: {
        CHECK(tab_observation.has_annotated_page_content_result(),
              base::NotFatalUntil::M147);
        CHECK(tab_observation.has_screenshot_result(),
              base::NotFatalUntil::M147);
        TabObservation::AnnotatedPageContentResult apc_result =
            tab_observation.annotated_page_content_result();
        TabObservation::ScreenshotResult screenshot_result =
            tab_observation.screenshot_result();

        if (apc_result != TabObservation::ANNOTATED_PAGE_CONTENT_OK &&
            screenshot_result != TabObservation::SCREENSHOT_OK) {
          // If both APC and Screenshot failed record it as a single result to
          // avoid combinatorial results. This should be rare.
          tab_result = ActorTabObservationResult::kApcAndScreenshotNotOk;
        } else {
          switch (apc_result) {
            case TabObservation::ANNOTATED_PAGE_CONTENT_OK:
              break;
            case TabObservation::ANNOTATED_PAGE_CONTENT_TIMEOUT:
              tab_result = ActorTabObservationResult::kApcTimeout;
              break;
            case TabObservation::ANNOTATED_PAGE_CONTENT_ERROR:
              tab_result = ActorTabObservationResult::kApcError;
              break;
            case optimization_guide::proto::
                TabObservation_AnnotatedPageContentResult_TabObservation_AnnotatedPageContentResult_INT_MIN_SENTINEL_DO_NOT_USE_:
            case optimization_guide::proto::
                TabObservation_AnnotatedPageContentResult_TabObservation_AnnotatedPageContentResult_INT_MAX_SENTINEL_DO_NOT_USE_:
              NOTREACHED(base::NotFatalUntil::M147);
              break;
          }

          switch (screenshot_result) {
            case TabObservation::SCREENSHOT_OK:
              break;
            case TabObservation::SCREENSHOT_TIMEOUT:
              tab_result = ActorTabObservationResult::kScreenshotTimeout;
              break;
            case TabObservation::SCREENSHOT_ERROR:
              tab_result = ActorTabObservationResult::kScreenshotError;
              break;
            case optimization_guide::proto::
                TabObservation_ScreenshotResult_TabObservation_ScreenshotResult_INT_MIN_SENTINEL_DO_NOT_USE_:
            case optimization_guide::proto::
                TabObservation_ScreenshotResult_TabObservation_ScreenshotResult_INT_MAX_SENTINEL_DO_NOT_USE_:
              NOTREACHED(base::NotFatalUntil::M147);
              break;
          }

          // We already ensured one of APC and screenshot has a failure.
          CHECK(tab_result.has_value(), base::NotFatalUntil::M147);
        }
        break;
      }
      case optimization_guide::proto::
          TabObservation_TabObservationResult_TabObservation_TabObservationResult_INT_MIN_SENTINEL_DO_NOT_USE_:
      case optimization_guide::proto::
          TabObservation_TabObservationResult_TabObservation_TabObservationResult_INT_MAX_SENTINEL_DO_NOT_USE_:
        NOTREACHED(base::NotFatalUntil::M147);
        break;
    }

    CHECK(tab_result.has_value(), base::NotFatalUntil::M147);
    base::UmaHistogramEnumeration(kActorPageContextTabObservationResult,
                                  *tab_result);
  }
}

void RecordSplitModeTimeOfUseFrameStatus(SplitModeTimeOfUseFrameStatus status) {
  base::UmaHistogramEnumeration("Actor.PageTool.SplitModeTimeOfUseFrameStatus",
                                status);
}

void RecordTimeOfUseObservationSuccess(bool success) {
  base::UmaHistogramBoolean("Actor.PageTool.TimeOfUseObservationSuccess",
                            success);
}

}  // namespace actor
