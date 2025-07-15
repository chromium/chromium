// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"

#include "components/tabs/public/tab_interface.h"

namespace actor::ui {
using ::tabs::TabInterface;

ActorUiTabController::ActorUiTabController(TabInterface& tab) : tab_(tab) {
  tab_subscriptions_.push_back(tab.RegisterDidActivate(
      base::BindRepeating(&ActorUiTabController::OnTabActivationChanged,
                          weak_factory_.GetWeakPtr(), /*is_activated=*/true)));
  tab_subscriptions_.push_back(tab.RegisterWillDeactivate(
      base::BindRepeating(&ActorUiTabController::OnTabActivationChanged,
                          weak_factory_.GetWeakPtr(), /*is_activated=*/false)));
}

ActorUiTabController::~ActorUiTabController() = default;

void ActorUiTabController::OnUiTabStateChange(const UiTabState& ui_tab_state) {
  // TODO(crbug.com/425952887): Implement this function.
  if (current_ui_tab_state_ != ui_tab_state) {
    // TODO(crbug.com/428216197): Only notify relevant UI components on change.
    current_ui_tab_state_ = ui_tab_state;
    NotifyTabScopedUiComponents(ui_tab_state, tab_->IsActivated());
  }
}

void ActorUiTabController::SetActiveTaskId(TaskId task_id) {
  CHECK(!active_task_id_);
  active_task_id_ = task_id;
}

void ActorUiTabController::ClearActiveTaskId() {
  active_task_id_ = TaskId(0);
}

void ActorUiTabController::NotifyTabScopedUiComponents(
    const UiTabState& ui_tab_state,
    bool tab_activated) {
  // TODO(crbug.com/425952887): Implement this function.
}

void ActorUiTabController::OnTabActivationChanged(bool is_activated,
                                                  tabs::TabInterface* tab) {
  NotifyTabScopedUiComponents(current_ui_tab_state_, is_activated);
}

}  // namespace actor::ui
