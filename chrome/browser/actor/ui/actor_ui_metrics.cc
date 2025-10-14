// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "chrome/browser/actor/ui/states/handoff_button_state.h"

namespace actor::ui {

constexpr char kActorUiPrefix[] = "Actor.Ui.";

std::string GetActorUiMetricName(const char* suffix) {
  return base::StrCat({kActorUiPrefix, suffix});
}

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
  base::RecordAction(
      base::UserMetricsAction(GetActorUiMetricName("TaskIcon.Click").c_str()));
}

void RecordActuatingTabWebContentsAttached() {
  base::RecordAction(base::UserMetricsAction(
      GetActorUiMetricName("ActuatingTabWebContentsAttached").c_str()));
}

void RecordTabControllerError(ActorUiTabControllerError error) {
  base::UmaHistogramEnumeration(GetActorUiMetricName("TabController.Error"),
                                error);
}

}  // namespace actor::ui
