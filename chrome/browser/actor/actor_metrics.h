// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_METRICS_H_
#define CHROME_BROWSER_ACTOR_ACTOR_METRICS_H_

#include <cstddef>
#include <optional>
#include <string_view>

#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/glic/host/glic.mojom-shared.h"
#include "chrome/common/actor.mojom.h"
#include "components/actor/public/mojom/actor_types.mojom-forward.h"

namespace optimization_guide {
enum class AIPageContentExtractionStatus;

namespace proto {
class ActionsResult;
}  // namespace proto
}  // namespace optimization_guide

namespace page_content_annotations {
enum class ScreenshotCaptureStatus;
}  // namespace page_content_annotations

namespace actor {

// The source or feature that triggered an Annotated Page Content (APC) fetch.
enum class ApcSource {
  kActor,
  kGlic,
};

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
void RecordActorTaskCompletion(
    ActorTask::StoppedReason stopped_reason,
    base::TimeDelta total_time,
    base::TimeDelta controlled_time,
    size_t interruptions_count,
    size_t actions_count,
    std::optional<glic::mojom::InvocationSource> invocation_source);

// Recorded when a ActorTask is successfully created for the first time or not.
void RecordActorTaskCreated(bool success);

// Records the result codes of completed actions.
void RecordActionResultCode(actor::mojom::ActionResultCode action_result_code);

// Records the time spent fetching the APC for a PerformActions response.
void RecordPageContextApcDuration(base::TimeDelta duration);

// Histogram for tracking the outcome of an APC extraction for a PerformActions
// response.
inline constexpr std::string_view kActorPageContextApcStatus =
    "Actor.PageContext.APC.Status";

// Records the outcome of an APC extraction for a PerformActions response.
void RecordPageContextApcStatus(
    optimization_guide::AIPageContentExtractionStatus status);

// Records the time spent fetching a screenshot for a PerformActions response.
void RecordPageContextScreenshotDuration(base::TimeDelta duration);

// Histogram for tracking the outcome of a screenshot capture for a
// PerformActions response.
inline constexpr std::string_view kActorPageContextScreenshotStatus =
    "Actor.PageContext.Screenshot.Status";

// Records the outcome of a screenshot capture for a PerformActions response.
void RecordPageContextScreenshotStatus(
    page_content_annotations::ScreenshotCaptureStatus status);

// Records the number of tabs that were observed for a PerformActions response.
void RecordPageContextTabCount(size_t tab_count);

// Recorded when a direct download is triggered by an ActorTask.
void RecordDirectDownloadTriggered(bool success);

// Recorded when a 'save as' download dialog is triggered by an ActorTask.
void RecordDownloadSaveAsDialogTriggered(bool success);

// Records whether the APC is identical to the one from the previous fetch
// from ANY source.
void RecordApcComparisonIdentical(ApcSource source, bool identical);

// Records script tool specific metrics.
void RecordScriptToolActionResultCode(
    actor::mojom::ActionResultCode action_result_code);
void RecordScriptToolInputSizeBytes(size_t size_bytes);
void RecordScriptToolOutputSizeBytes(size_t size_bytes);

// Records the outcome of navigation gating decisions.
void RecordNavigationGatingDecision(ExecutionEngine::GatingDecision decision);

// The frame a page action targets, relative to the tab's main frame.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ActorPageActionTargetFrameType)
enum class PageActionTargetFrameType {
  kMainFrame = 0,
  // The target frame and all of its ancestors are same-origin with the main
  // frame.
  kSameOriginSubframe = 1,
  // The target frame or one of its ancestors is cross-origin to the main frame
  // (including when the target frame itself is same-origin with the main frame
  // but has a cross-origin ancestor, e.g. the inner A in A(B(A))).
  kCrossOriginSubframe = 2,
  kMaxValue = kCrossOriginSubframe,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:ActorPageActionTargetFrameType)

// Records the type of frame targeted by a page action that is about to be
// invoked.
void RecordPageActionTargetFrameType(PageActionTargetFrameType type);

// The most restrictive outcome of dry-run page-action gating across the frames
// between a page action's target frame and the tab's main frame.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(ActorSubframeGatingDryRunOutcome)
enum class SubframeGatingDryRunOutcome {
  kWouldBeAllowed = 0,
  kWouldRequireUserConfirmation = 1,
  kWouldBlock = 2,
  kMaxValue = kWouldBlock,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:ActorSubframeGatingDryRunOutcome)

// Records the outcome of dry-run page-action gating on a page action's target
// subframe chain.
void RecordSubframeGatingDryRunOutcome(SubframeGatingDryRunOutcome outcome);
void RecordSubframeGatingDryRunBlockReason(MayActOnUrlBlockReason block_reason);

void RecordObservationOutcomeHistogram(
    const optimization_guide::proto::ActionsResult& result,
    bool is_for_retry);

// Histogram for tracking the overall outcome of a page context fetch after
// performActions.
inline constexpr std::string_view kActorPageContextObservationOutcome =
    "Actor.PageContext.ObservationOutcome";

// LINT.IfChange(ActorObservationOutcome)
enum class ActorObservationOutcome {
  kSuccess,
  kSuccessAfterRetry,
  kFailure,
  kFailureAfterRetry,
  kMaxValue = kFailureAfterRetry,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:ActorObservationOutcome)

// Records the outcome of an post-performActions observation fetch.
void RecordTabObservationResultHistogram(
    const optimization_guide::proto::ActionsResult& result);

// Histogram for tracking the individual result codes for tab observations in a
// fetch.
inline constexpr std::string_view kActorPageContextTabObservationResult =
    "Actor.PageContext.TabObservationResult";

// LINT.IfChange(ActorTabObservationResult)
enum class ActorTabObservationResult {
  kSuccess,
  kTabWentAway,
  kPageCrashed,
  kUnknown,
  kWebContentsChanged,
  kPageContextNotEligible,
  kApcTimeout,
  kApcError,
  kScreenshotTimeout,
  kScreenshotError,
  kApcAndScreenshotNotOk,
  kMaxValue = kApcAndScreenshotNotOk,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:ActorTabObservationResult)

// LINT.IfChange(SplitModeTimeOfUseFrameStatus)
enum class SplitModeTimeOfUseFrameStatus {
  kMatch = 0,
  kInitializedFrameDestroyed = 1,
  kFrameMismatch = 2,
  kMaxValue = kFrameMismatch,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:SplitModeTimeOfUseFrameStatus)

// Records whether the target frame changed or was destroyed between the
// Validate and Invoke steps when the renderer resolved target feature is
// enabled.
void RecordSplitModeTimeOfUseFrameStatus(SplitModeTimeOfUseFrameStatus status);

// Records whether target observation succeeded during TimeOfUseValidation.
void RecordTimeOfUseObservationSuccess(bool success);

}  // namespace actor
#endif  // CHROME_BROWSER_ACTOR_ACTOR_METRICS_H_
