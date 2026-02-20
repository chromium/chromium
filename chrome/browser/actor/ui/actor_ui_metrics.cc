// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"

namespace actor::ui {

namespace {

constexpr std::string_view kActorUiPrefix = "Actor.Ui.";
constexpr std::string_view kEventDispatcherPrefix = "Actor.EventDispatcher.";

template <typename... Args>
std::string GetActorUiMetricName(Args... args) {
  return base::StrCat({kActorUiPrefix, std::string_view(args)...});
}

// NOTE: Histograms that depend on this existed in the actor/ui directory before
// refactoring into this file so they use the "Actor.EventDispatcher" prefix
// instead of "Actor.Ui".
std::string GetEventDispatcherHistogramName(std::string_view name,
                                            std::string_view suffix = "") {
  return base::StrCat({kEventDispatcherPrefix, name, suffix});
}

}  // namespace

void LogHandoffButtonClick(HandoffButtonState::ControlOwnership ownership) {
  switch (ownership) {
    case HandoffButtonState::ControlOwnership::kActor:
      base::RecordAction(base::UserMetricsAction(
          GetActorUiMetricName("HandoffButton.TakeControl.Clicked").c_str()));
      break;
    case HandoffButtonState::ControlOwnership::kClient:
      base::RecordAction(base::UserMetricsAction(
          GetActorUiMetricName("HandoffButton.GiveControl.Clicked").c_str()));
      break;
  }
}

void LogTaskListBubbleRowClicked() {
  base::RecordComputedAction(GetActorUiMetricName("TaskListBubble.Row.Click"));
}

void LogTaskNudgeClick(ActorTaskNudgeState nudge_state) {
  DCHECK_NE(nudge_state.text, ActorTaskNudgeState::Text::kDefault)
      << "Nudge is hidden in default state so it cannot be clicked.";
  DCHECK_NE(nudge_state.text,
            ActorTaskNudgeState::Text::kMultipleTasksNeedAttention)
      << "MultipleTasksNeedAttention state is deprecated.";
  base::RecordComputedAction(
      GetActorUiMetricName("TaskNudge.", ToString(nudge_state), ".Click"));
}

void LogGlobalTaskIndicatorClick(ActorTaskNudgeState nudge_state) {
  DCHECK_NE(nudge_state.text,
            ActorTaskNudgeState::Text::kMultipleTasksNeedAttention)
      << "MultipleTasksNeedAttention state is deprecated.";
  base::RecordComputedAction(GetActorUiMetricName(
      "GlobalTaskIndicator.", ToString(nudge_state), ".Click"));
}

void RecordTaskListBubbleRows(size_t count) {
  base::UmaHistogramCounts100(GetActorUiMetricName("TaskListBubble.Rows"),
                              count);
}

void RecordTaskIconError(ActorUiTaskIconError error) {
  base::UmaHistogramEnumeration(GetActorUiMetricName("TaskIcon.Error"), error);
}

void RecordTaskNudgeShown(ActorTaskNudgeState nudge_state) {
  DCHECK_NE(nudge_state.text, ActorTaskNudgeState::Text::kDefault)
      << "Nudge is hidden in default state so it cannot be shown.";
  DCHECK_NE(nudge_state.text,
            ActorTaskNudgeState::Text::kMultipleTasksNeedAttention)
      << "MultipleTasksNeedAttention state is deprecated.";
  base::UmaHistogramEnumeration(GetActorUiMetricName("TaskNudge.Shown"),
                                nudge_state.text);
}

void RecordGlobalTaskIndicatorNudgeShown(ActorTaskNudgeState nudge_state) {
  DCHECK_NE(nudge_state.text,
            ActorTaskNudgeState::Text::kMultipleTasksNeedAttention)
      << "MultipleTasksNeedAttention state is deprecated.";
  base::UmaHistogramEnumeration(
      GetActorUiMetricName("GlobalTaskIndicator.Nudge.Shown"),
      nudge_state.text);
}

void RecordActuatingTabWebContentsAttached() {
  base::RecordAction(base::UserMetricsAction(
      GetActorUiMetricName("ActuatingTabWebContentsAttached").c_str()));
}

void RecordTabControllerError(ActorUiTabControllerError error) {
  base::UmaHistogramEnumeration(GetActorUiMetricName("TabController.Error"),
                                error);
}

base::ScopedUmaHistogramTimer GetUiEventDurationScopedTimer(
    std::string_view ui_event_name) {
  return base::ScopedUmaHistogramTimer(
      GetEventDispatcherHistogramName(ui_event_name, ".Duration"),
      base::ScopedUmaHistogramTimer::ScopedHistogramTiming::kMicrosecondTimes);
}

void RecordUiEventDuration(std::string_view ui_event_name,
                           base::TimeDelta duration) {
  // Use a high-resolution timer that records in microseconds.
  // The range is set from 1 microsecond to 10 seconds with 50 buckets.
  base::UmaHistogramMicrosecondsTimes(
      GetEventDispatcherHistogramName(ui_event_name, ".Duration"), duration);
}

void RecordUiEventFailure(std::string_view ui_event_name) {
  base::UmaHistogramBoolean(
      GetEventDispatcherHistogramName(ui_event_name, ".Failure"), true);
}

void RecordComputedTargetResult(ComputedTargetResult target_result) {
  base::UmaHistogramEnumeration(
      GetEventDispatcherHistogramName("ComputedTargetResult"), target_result);
}

void RecordModelPageTargetType(ModelPageTargetType target_type) {
  base::UmaHistogramEnumeration(
      GetEventDispatcherHistogramName("ModelPageTargetType"), target_type);
}

void RecordGetDomNodeResult(GetDomNodeResult result) {
  base::UmaHistogramEnumeration("Actor.DomNodeGeometry.GetDomNodeResult",
                                result);
}

void RecordRendererResolvedTargetResult(
    RendererResolvedTargetResult target_result) {
  base::UmaHistogramEnumeration(
      GetEventDispatcherHistogramName("RendererResolvedTargetResult"),
      target_result);
}

}  // namespace actor::ui
