// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/actor/ui/states/agent_overlay_state.h"
#include "chrome/browser/actor/ui/states/handoff_button_state.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace actor {

// TODO(crbug.com/425952887): Implement this class.
class ActorUiTabController {
 public:
  struct UiTabState {
    AgentOverlayState agent_overlay;
    HandoffButtonState handoff_button;
  };

  explicit ActorUiTabController(tabs::TabInterface& tab);
  ~ActorUiTabController();

 private:
  // Owns this class via TabModel.
  const raw_ref<tabs::TabInterface> tab_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_H_
