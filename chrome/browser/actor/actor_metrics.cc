// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "chrome/browser/actor/actor_task.h"

namespace actor {

namespace {
std::string_view ToCancelledOrCompleted(bool success) {
  return success ? "Completed" : "Cancelled";
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
    bool success) {
  base::UmaHistogramLongTimes100(
      base::StrCat(
          {"Actor.Task.Duration.Visible.", ToCancelledOrCompleted(success)}),
      visible_duration);

  base::UmaHistogramLongTimes100(
      base::StrCat(
          {"Actor.Task.Duration.NotVisible.", ToCancelledOrCompleted(success)}),
      non_visible_duration);
}

void RecordActorTaskCompletion(bool success,
                               base::TimeDelta total_time,
                               base::TimeDelta controlled_time,
                               size_t interruptions_count,
                               size_t actions_count) {
  base::UmaHistogramLongTimes100(
      base::StrCat(
          {"Actor.Task.Duration.WallClock.", ToCancelledOrCompleted(success)}),
      total_time);
  base::UmaHistogramLongTimes100(
      base::StrCat({"Actor.Task.Duration.", ToCancelledOrCompleted(success)}),
      controlled_time);
  base::UmaHistogramCounts1000(base::StrCat({"Actor.Task.Interruptions.",
                                             ToCancelledOrCompleted(success)}),
                               interruptions_count);
  base::UmaHistogramCounts1000(
      base::StrCat({"Actor.Task.Count.", ToCancelledOrCompleted(success)}),
      actions_count);
}

}  // namespace actor
