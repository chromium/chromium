// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_METRICS_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_METRICS_H_

#include <string_view>

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/actor/ui/actor_ui_metrics_types.h"
#include "chrome/browser/actor/ui/dom_node_geometry_types.h"
#include "chrome/browser/actor/ui/states/actor_task_nudge_state.h"
#include "chrome/browser/actor/ui/states/handoff_button_state.h"

namespace actor::ui {

// Logs a click on the handoff button.
void LogHandoffButtonClick(HandoffButtonState::ControlOwnership ownership);

// Logs a click on a row in the task list bubble.
void LogTaskListBubbleRowClicked();

// Logs a click on the task nudge.
// This fails if the nudge is in the default state.
void LogTaskNudgeClick(ActorTaskNudgeState nudge_state);

// Logs a click on the global task indicator.
void LogGlobalTaskIndicatorClick(ActorTaskNudgeState nudge_state);

// Recorded when the task list bubble is shown.
// `count` is the number of rows shown in the bubble.
void RecordTaskListBubbleRows(size_t count);

// Recorded when an error happens in the ActorUiTaskIcon.
void RecordTaskIconError(ActorUiTaskIconError error);

// Recorded when the task nudge is shown.
void RecordTaskNudgeShown(ActorTaskNudgeState nudge_state);

// Recorded when the global task indicator nudge is shown.
void RecordGlobalTaskIndicatorNudgeShown(ActorTaskNudgeState nudge_state);

// Records web content attachment for the actuating tab.
void RecordActuatingTabWebContentsAttached();

// Recorded when an error happens in the Tab Controller.
void RecordTabControllerError(ActorUiTabControllerError error);

// Returns a timer that records the duration of a UI event.
base::ScopedUmaHistogramTimer GetUiEventDurationScopedTimer(
    std::string_view ui_event_name);

// Records the duration of a UI event
void RecordUiEventDuration(std::string_view ui_event_name,
                           base::TimeDelta duration);

// Recorded when a UI event fails
void RecordUiEventFailure(std::string_view ui_event_name);

// Recorded when the result of getting a DOM node is computed.
void RecordGetDomNodeResult(GetDomNodeResult result);

// Recorded when the target result is computed by the event dispatcher.
void RecordComputedTargetResult(ComputedTargetResult target_result);

// Recorded when the model page target type is determined.
void RecordModelPageTargetType(ModelPageTargetType target_type);

// Recorded when the renderer resolved target result is retrieved by the event
// dispatcher.
void RecordRendererResolvedTargetResult(
    RendererResolvedTargetResult target_result);

}  // namespace actor::ui
#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_METRICS_H_
