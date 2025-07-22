// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "components/tabs/public/tab_interface.h"

namespace actor::ui {
using ::tabs::TabInterface;

ActorUiTabController::ActorUiTabController(TabInterface& tab,
                                           ActorKeyedService* actor_service)
    : tab_(tab), actor_keyed_service_(actor_service) {
  CHECK(actor_keyed_service_);
  actor_overlay_view_controller_ =
      std::make_unique<ActorOverlayViewController>(&*tab_);
  tab_subscriptions_.push_back(tab.RegisterDidActivate(
      base::BindRepeating(&ActorUiTabController::OnTabActiveStatusChanged,
                          weak_factory_.GetWeakPtr(), /*is_activated=*/true)));
  tab_subscriptions_.push_back(tab.RegisterWillDeactivate(
      base::BindRepeating(&ActorUiTabController::OnTabActiveStatusChanged,
                          weak_factory_.GetWeakPtr(), /*is_activated=*/false)));
  tab_subscriptions_.push_back(tab_->RegisterWillDetach(base::BindRepeating(
      &ActorUiTabController::OnTabWillDetach, weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterDidInsert(base::BindRepeating(
      &ActorUiTabController::OnTabDidInsert, weak_factory_.GetWeakPtr())));
}

ActorUiTabController::~ActorUiTabController() = default;

void ActorUiTabController::OnUiTabStateChange(const UiTabState& ui_tab_state,
                                              UiResultCallback callback) {
  UpdateState(ui_tab_state, current_tab_active_status_, std::move(callback));
}

void ActorUiTabController::OnTabActiveStatusChanged(bool tab_active_status,
                                                    tabs::TabInterface* tab) {
  UpdateState(current_ui_tab_state_, tab_active_status, base::DoNothing());
}

void ActorUiTabController::OnTabWillDetach(TabInterface* tab,
                                           TabInterface::DetachReason reason) {
  // TODO(crbug.com/422540636): Implement.
}

void ActorUiTabController::OnTabDidInsert(TabInterface* tab) {
  // TODO(crbug.com/422540636): Implement.
}

void ActorUiTabController::UpdateState(const UiTabState& ui_tab_state,
                                       bool tab_active_status,
                                       UiResultCallback callback) {
  if (current_ui_tab_state_ == ui_tab_state &&
      current_tab_active_status_ == tab_active_status) {
    return;
  }

  VLOG(4) << "Tab scoped UI components updated FROM -> TO:\n"
          << "ui_tab_state: " << current_ui_tab_state_ << " -> " << ui_tab_state
          << ", tab_active_status: " << current_tab_active_status_ << " -> "
          << tab_active_status << "\n";

  if (current_ui_tab_state_ != ui_tab_state) {
    current_ui_tab_state_ = ui_tab_state;
  }

  if (current_tab_active_status_ != tab_active_status) {
    current_tab_active_status_ = tab_active_status;
  }

  // TODO(crbug.com/425952887): Change this once ui components are implemented,
  // for now always return true.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));

  // TODO(crbug.com/428216197): Only notify relevant UI components on change.
}

void ActorUiTabController::SetActiveTaskId(TaskId task_id) {
  // TODO(crbug.com/432121373): Enable this check again once StoppedActingOnTab
  // events are dispatched.
  // CHECK(!active_task_id_);
  active_task_id_ = task_id;
}

void ActorUiTabController::ClearActiveTaskId() {
  active_task_id_ = TaskId(0);
}
void ActorUiTabController::SetActorTaskPaused() {
  if (auto* task = actor_keyed_service_->GetTask(active_task_id_)) {
    task->Pause();
  }
}

void ActorUiTabController::SetActorTaskResume() {
  if (auto* task = actor_keyed_service_->GetTask(active_task_id_)) {
    task->Resume();
  }
}

void ActorUiTabController::BindActorOverlay(
    mojo::PendingReceiver<mojom::ActorOverlayPageHandler> receiver) {
  actor_overlay_view_controller_->BindOverlay(std::move(receiver));
}

void ActorUiTabController::SetHandoffButtonVisibility(bool is_visible) {
  // TODO(crbug.com/425952887): Implement this function.
}

base::WeakPtr<ActorUiTabControllerInterface>
ActorUiTabController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace actor::ui
