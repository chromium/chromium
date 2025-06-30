// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_H_

#include "base/memory/raw_ref.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace actor::ui {

// TODO(crbug.com/425952887): Implement this class.
class ActorUiTabController : public ActorUiTabControllerInterface {
 public:
  explicit ActorUiTabController(tabs::TabInterface& tab);
  ~ActorUiTabController() override;
  void OnUiTabStateChange(const UiTabState& ui_tab_state) override;

 private:
  // Owns this class via TabModel.
  const raw_ref<tabs::TabInterface> tab_;
  UiTabState current_ui_tab_state_;
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_H_
