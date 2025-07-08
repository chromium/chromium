// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_INTERFACE_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_INTERFACE_H_

#include "chrome/browser/actor/ui/states/agent_overlay_state.h"
#include "chrome/browser/actor/ui/states/handoff_button_state.h"

namespace actor::ui {

struct UiTabState {
  bool operator==(const UiTabState& other) const = default;
  AgentOverlayState agent_overlay;
  HandoffButtonState handoff_button;
};

class ActorUiTabControllerInterface {
 public:
  virtual ~ActorUiTabControllerInterface() = default;

  // Called whenever the UiTabState changes.
  virtual void OnUiTabStateChange(const UiTabState& ui_tab_state) = 0;
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_INTERFACE_H_
