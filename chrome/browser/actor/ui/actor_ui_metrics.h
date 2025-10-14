// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_METRICS_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_METRICS_H_

#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/actor/ui/states/handoff_button_state.h"

namespace actor::ui {

// Logs a click on the handoff button.
void LogHandoffButtonClick(HandoffButtonState::ControlOwnership ownership);

// Logs a click on the task icon.
void LogTaskIconClick();

// Records web content attachment for the actuating tab.
void RecordActuatingTabWebContentsAttached();

// Recorded when an error happens in the Tab Controller.
void RecordTabControllerError(ActorUiTabControllerError error);

}  // namespace actor::ui
#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_METRICS_H_
