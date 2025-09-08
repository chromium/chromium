// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/actor/ui/states/handoff_button_state.h"

namespace actor::ui {

void LogHandoffButtonClick(ui::HandoffButtonState::ControlOwnership ownership) {
  switch (ownership) {
    case ui::HandoffButtonState::ControlOwnership::kActor:
      base::RecordAction(base::UserMetricsAction(
          "Actor.Ui.HandoffButton.TakeControl.Clicked"));
      break;
    case ui::HandoffButtonState::ControlOwnership::kClient:
      base::RecordAction(base::UserMetricsAction(
          "Actor.Ui.HandoffButton.GiveControl.Clicked"));
      break;
  }
}

void LogTaskIconClick() {
  base::RecordAction(base::UserMetricsAction("Actor.Ui.TaskIcon.Click"));
}

}  // namespace actor::ui
