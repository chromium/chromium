// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "chrome/browser/actor/ui/states/handoff_button_state.h"

namespace actor::ui {

namespace {

constexpr std::string_view kActorUiPrefix = "Actor.Ui.";

template <typename... Args>
std::string GetActorUiMetricName(Args... args) {
  return base::StrCat({kActorUiPrefix, std::string_view(args)...});
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

void LogTaskIconClick() {
  // TODO(crbug.com/462712067): Revise to use RecordComputedAction.
  base::RecordAction(
      base::UserMetricsAction(GetActorUiMetricName("TaskIcon.Click").c_str()));
}

void LogTaskNudgeClick(ActorTaskNudgeState nudge_state) {
  DCHECK_NE(nudge_state.text, ActorTaskNudgeState::Text::kDefault)
      << "Nudge is hidden in default state so it cannot be clicked.";
  base::RecordComputedAction(
      GetActorUiMetricName("TaskNudge.", ToString(nudge_state), ".Click"));
}

void RecordTaskNudgeShown(ActorTaskNudgeState nudge_state) {
  base::UmaHistogramEnumeration(GetActorUiMetricName("TaskNudge.Shown"),
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

std::string GetUiEventDurationHistogramName(std::string_view ui_event_name) {
  return base::StrCat({"Actor.EventDispatcher.", ui_event_name, ".Duration"});
}

void RecordUiEventDuration(std::string_view ui_event_name,
                           base::TimeDelta duration) {
  // Use a high-resolution timer that records in microseconds.
  // The range is set from 1 microsecond to 10 seconds with 50 buckets.
  base::UmaHistogramMicrosecondsTimes(
      GetUiEventDurationHistogramName(ui_event_name), duration);
}

void RecordUiEventFailure(std::string_view ui_event_name) {
  base::UmaHistogramBoolean(
      base::StrCat({"Actor.EventDispatcher.", ui_event_name, ".Failure"}),
      true);
}

}  // namespace actor::ui
