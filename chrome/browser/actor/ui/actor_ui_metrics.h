// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_METRICS_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_METRICS_H_

#include <string_view>

#include "base/time/time.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/actor/ui/states/actor_task_nudge_state.h"
#include "chrome/browser/actor/ui/states/handoff_button_state.h"

namespace actor::ui {

// Logs a click on the handoff button.
void LogHandoffButtonClick(HandoffButtonState::ControlOwnership ownership);

// Logs a click on the task icon.
void LogTaskIconClick();

// Logs a click on the task nudge.
// This fails if the nudge is in the default state.
void LogTaskNudgeClick(ActorTaskNudgeState nudge_state);

// Recorded when the task nudge is shown.
void RecordTaskNudgeShown(ActorTaskNudgeState nudge_state);

// Records web content attachment for the actuating tab.
void RecordActuatingTabWebContentsAttached();

// Recorded when an error happens in the Tab Controller.
void RecordTabControllerError(ActorUiTabControllerError error);

// Returns the UiEvent duration histogram name.
std::string GetUiEventDurationHistogramName(std::string_view ui_event_name);

// Records the duration of a UI event
void RecordUiEventDuration(std::string_view ui_event_name,
                           base::TimeDelta duration);

// Recorded when a UI event fails
void RecordUiEventFailure(std::string_view ui_event_name);

}  // namespace actor::ui
#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_METRICS_H_
