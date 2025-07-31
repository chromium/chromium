// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/actor/ui/handoff_button_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"

namespace actor::ui {
namespace {
using ::tabs::TabInterface;

void LogAndIgnoreCallbackError(const std::string_view source_name,
                               bool result) {
  if (!result) {
    LOG(DFATAL) << "Unexpected error in callback from " << source_name;
  }
}
}  // namespace

std::unique_ptr<HandoffButtonController>
ActorUiTabControllerFactory::CreateHandoffButtonController(
    tabs::TabInterface& tab) {
  return std::make_unique<HandoffButtonController>(tab);
}

std::unique_ptr<ActorOverlayViewController>
ActorUiTabControllerFactory::CreateActorOverlayViewController(
    tabs::TabInterface& tab) {
  return std::make_unique<ActorOverlayViewController>(tab);
}

ActorUiTabController::ActorUiTabController(
    tabs::TabInterface& tab,
    ActorKeyedService* actor_service,
    std::unique_ptr<ActorUiTabControllerFactoryInterface> controller_factory)
    : tab_(tab),
      actor_keyed_service_(actor_service),
      controller_factory_(std::move(controller_factory)) {
  CHECK(actor_keyed_service_);
  actor_overlay_view_controller_ =
      controller_factory_->CreateActorOverlayViewController(tab);
  handoff_button_controller_ =
      controller_factory_->CreateHandoffButtonController(tab);
  RegisterTabSubscriptions();
}

ActorUiTabController::~ActorUiTabController() = default;

void ActorUiTabController::RegisterTabSubscriptions() {
  if (features::kGlicActorUiOverlay.Get()) {
    tab_subscriptions_.push_back(tab_->RegisterWillDetach(base::BindRepeating(
        &ActorUiTabController::OnTabWillDetach, weak_factory_.GetWeakPtr())));
    tab_subscriptions_.push_back(tab_->RegisterDidInsert(base::BindRepeating(
        &ActorUiTabController::OnTabDidInsert, weak_factory_.GetWeakPtr())));
  }
  tab_subscriptions_.push_back(tab_->RegisterDidActivate(
      base::BindRepeating(&ActorUiTabController::OnTabActiveStatusChanged,
                          weak_factory_.GetWeakPtr(), /*is_activated=*/true)));
  tab_subscriptions_.push_back(tab_->RegisterWillDeactivate(
      base::BindRepeating(&ActorUiTabController::OnTabActiveStatusChanged,
                          weak_factory_.GetWeakPtr(), /*is_activated=*/false)));
}

void ActorUiTabController::OnUiTabStateChange(const UiTabState& ui_tab_state,
                                              UiResultCallback callback) {
  UpdateState(ui_tab_state, current_tab_active_status_, std::move(callback));
}

void ActorUiTabController::OnTabActiveStatusChanged(bool tab_active_status,
                                                    tabs::TabInterface* tab) {
  UpdateState(
      current_ui_tab_state_, tab_active_status,
      base::BindOnce(&LogAndIgnoreCallbackError, "OnTabActiveStatusChanged"));
}

void ActorUiTabController::OnTabWillDetach(TabInterface* tab,
                                           TabInterface::DetachReason reason) {
  if (features::kGlicActorUiOverlay.Get()) {
    actor_overlay_view_controller_->NullifyWebView();
  }
}

void ActorUiTabController::OnTabDidInsert(TabInterface* tab) {
  if (features::kGlicActorUiOverlay.Get()) {
    actor_overlay_view_controller_->SetWindowController(
        tab->GetBrowserWindowInterface()
            ->GetFeatures()
            .actor_overlay_window_controller());
  }
}

void ActorUiTabController::UpdateState(const UiTabState& ui_tab_state,
                                       bool tab_active_status,
                                       UiResultCallback callback) {
  if (current_ui_tab_state_ == ui_tab_state &&
      current_tab_active_status_ == tab_active_status) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
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

  // TODO(crbug.com/428216197): Only notify relevant UI components on change.
  if (features::kGlicActorUiOverlay.Get()) {
    actor_overlay_view_controller_->UpdateState(
        current_ui_tab_state_.actor_overlay, ComputeActorOverlayVisibility());
  }

  if (features::kGlicActorUiHandoffButton.Get()) {
    // The Handoff Button's visibility is always false through this entrypoint.
    // It's visibility will only be updated via  SetHandoffButtonVisibility().
    // TODO(crbug.com/435172659): Set the visibility based on a unified hover
    // state for the tab components.
    handoff_button_controller_->UpdateState(
        current_ui_tab_state_.handoff_button, false);
  }

  // TODO(crbug.com/425952887): Change this once ui components are implemented,
  // for now always return true.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));
}

bool ActorUiTabController::ComputeActorOverlayVisibility() {
  // Only visible when its state and the associated tab are both active.
  return current_ui_tab_state_.actor_overlay.is_active &&
         current_tab_active_status_;
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
  if (features::kGlicActorUiOverlay.Get()) {
    actor_overlay_view_controller_->BindOverlay(std::move(receiver));
  }
}

void ActorUiTabController::SetHandoffButtonVisibility(bool is_visible) {
  if (!features::kGlicActorUiHandoffButton.Get()) {
    return;
  }

  bool should_be_visible = is_visible && current_tab_active_status_;
  handoff_button_controller_->UpdateState(current_ui_tab_state_.handoff_button,
                                          should_be_visible);
  VLOG(4) << "Handoff button turned " << (should_be_visible ? "ON" : "OFF");
}

base::WeakPtr<ActorUiTabControllerInterface>
ActorUiTabController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace actor::ui
