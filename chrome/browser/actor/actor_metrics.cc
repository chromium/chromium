// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/actor/actor_task.h"

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
                           base::to_underlying(action_result_code));
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
  base::UmaHistogramCounts1000("Actor.NavigationGating.ConfirmedListSize",
                               confirmed_list_size);
}

void RecordNavigationGatingDecision(ExecutionEngine::GatingDecision decision) {
  base::UmaHistogramEnumeration("Actor.NavigationGating.GatingDecision",
                                decision);
}

}  // namespace actor
