// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_METRICS_H_
#define CHROME_BROWSER_ACTOR_ACTOR_METRICS_H_

#include <cstddef>

#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/common/actor.mojom.h"

namespace actor {

// Records the number of actions taken in `from_state` before transitioning to
// `to_state`.
void RecordActorTaskStateTransitionActionCount(size_t action_count,
                                               ActorTask::State from_state,
                                               ActorTask::State to_state);

// Records the duration spent in `state`.
void RecordActorTaskStateTransitionDuration(base::TimeDelta duration,
                                            ActorTask::State state);

// Records tool timings.
void RecordToolTimings(std::string_view tool_name,
                       base::TimeDelta execution_duration,
                       base::TimeDelta page_stabilization_duration);

// Records histograms tracking the total visible and non-visible durations of an
// ActorTask while in an actor-controlled state upon completion or cancellation.
//
// Records two histograms:
// - The total actor-controlled time while at least one of its tabs was visible
// - The total actor-controlled time while none of it's tabs were visible
//
// This function only records histograms if `state` is either kFinished or
// kCancelled. Calls with other states will trigger NOTREACHED().
void RecordActorTaskVisibilityDurationHistograms(
    base::TimeDelta visible_duration,
    base::TimeDelta non_visible_duration,
    ActorTask::StoppedReason stopped_reason);

// Record task completion metrics.
void RecordActorTaskCompletion(ActorTask::StoppedReason stopped_reason,
                               base::TimeDelta total_time,
                               base::TimeDelta controlled_time,
                               size_t interruptions_count,
                               size_t actions_count);

// Recorded when a ActorTask is successfully created for the first time or not.
void RecordActorTaskCreated(bool success);

// Records the result codes of completed actions.
void RecordActionResultCode(actor::mojom::ActionResultCode action_result_code);

// Records the time spent fetching the APC for a PerformActions response.
void RecordPageContextApcDuration(base::TimeDelta duration);

// Records the time spent fetching a screenshot for a PerformActions response.
void RecordPageContextScreenshotDuration(base::TimeDelta duration);

// Records the number of tabs that were observed for a PerformActions response.
void RecordPageContextTabCount(size_t tab_count);

// Recorded when a direct download is triggered by an ActorTask.
void RecordDirectDownloadTriggered(bool success);

// Recorded when a 'save as' download dialog is triggered by an ActorTask.
void RecordDownloadSaveAsDialogTriggered(bool success);

// Records the the size of the allow list and confirmed list (blocklist) of
// origins for navigation gating.
void RecordActorNavigationGatingListSize(size_t allow_list_size,
                                         size_t confirmed_list_size);

// Records the outcome of navigation gating decisions.
void RecordNavigationGatingDecision(ExecutionEngine::GatingDecision decision);

}  // namespace actor
#endif  // CHROME_BROWSER_ACTOR_ACTOR_METRICS_H_
