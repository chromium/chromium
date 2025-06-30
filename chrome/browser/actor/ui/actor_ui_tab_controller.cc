// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"

#include "components/tabs/public/tab_interface.h"

namespace actor::ui {
using ::tabs::TabInterface;

ActorUiTabController::ActorUiTabController(TabInterface& tab) : tab_(tab) {}
ActorUiTabController::~ActorUiTabController() = default;
void ActorUiTabController::OnUiTabStateChange(const UiTabState& ui_tab_state) {
  // TODO(crbug.com/425952887): Implement this function.
  if (current_ui_tab_state_ != ui_tab_state) {
    // TODO(crbug.com/428216197): Only notify relevant UI components on change.
    current_ui_tab_state_ = ui_tab_state;
  }
}

}  // namespace actor::ui
