// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/ui/actor_border_view_controller.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/actor/ui/handoff_button_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

DEFINE_USER_DATA(actor::ui::ActorUiTabController);

namespace actor::ui {
namespace {
using ::tabs::TabInterface;
using enum actor::ui::HandoffButtonState::ControlOwnership;

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
      controller_factory_(std::move(controller_factory)),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
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
  }
  tab_subscriptions_.push_back(tab_->RegisterDidActivate(
      base::BindRepeating(&ActorUiTabController::OnTabActiveStatusChanged,
                          weak_factory_.GetWeakPtr(), /*is_activated=*/true)));
  tab_subscriptions_.push_back(tab_->RegisterWillDeactivate(
      base::BindRepeating(&ActorUiTabController::OnTabActiveStatusChanged,
                          weak_factory_.GetWeakPtr(), /*is_activated=*/false)));
}

ActorUiTabController* ActorUiTabController::From(tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

void ActorUiTabController::OnUiTabStateChange(const UiTabState& ui_tab_state,
                                              UiResultCallback callback) {
  if (current_ui_tab_state_ == ui_tab_state) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
    return;
  }
  VLOG(4) << "Tab scoped UI components updated FROM -> TO:\n"
          << "ui_tab_state: " << current_ui_tab_state_ << " -> " << ui_tab_state
          << "\n";

  current_ui_tab_state_ = ui_tab_state;
  MaybeUpdateState(std::move(callback));
}

void ActorUiTabController::OnTabActiveStatusChanged(bool tab_active_status,
                                                    tabs::TabInterface* tab) {
  if (current_tab_active_status_ == tab_active_status) {
    return;
  }
  VLOG(4) << "Tab scoped UI components updated FROM -> TO:\n"
          << " tab_active_status: " << current_tab_active_status_ << " -> "
          << tab_active_status << "\n";

  current_tab_active_status_ = tab_active_status;
  MaybeUpdateState(
      base::BindOnce(&LogAndIgnoreCallbackError, "OnTabActiveStatusChanged"));
}

void ActorUiTabController::OnTabWillDetach(TabInterface* tab,
                                           TabInterface::DetachReason reason) {
  if (features::kGlicActorUiOverlay.Get()) {
    actor_overlay_view_controller_->NullifyWebView();
  }
}

bool ActorUiTabController::ShouldShowActorTabIndicator() {
  return features::kGlicActorUiTabIndicator.Get() &&
         should_show_actor_tab_indicator_;
}

base::CallbackListSubscription
ActorUiTabController::RegisterActorTabIndicatorStateChangedCallback(
    ActorTabIndicatorStateChangedCallback callback) {
  return on_actor_tab_indicator_changed_callbacks_.Add(std::move(callback));
}

void ActorUiTabController::SetActorTabIndicatorVisibility(
    bool should_show_tab_indicator) {
  // When GLIC isn't enabled, we never set the tab indicator.
  // TODO(crbug.com/422538779) remove GLIC dependency once the ACTOR_ACCESSING
  // alert migrates away from the GLIC_ACCESSING resources.
#if BUILDFLAG(ENABLE_GLIC)
  if (should_show_actor_tab_indicator_ == should_show_tab_indicator) {
    return;
  }
  should_show_actor_tab_indicator_ = should_show_tab_indicator;
  on_actor_tab_indicator_changed_callbacks_.Notify(
      should_show_actor_tab_indicator_);
  // Notify tab strip model of state change.
  tab_->GetBrowserWindowInterface()->GetTabStripModel()->NotifyTabChanged(
      base::to_address(tab_), TabChangeType::kAll);
#endif
  return;
}

void ActorUiTabController::MaybeUpdateState(UiResultCallback callback) {
  if (!update_state_debounce_timer_.IsRunning()) {
    in_progress_updates_int_++;
  }

  update_state_debounce_timer_.Start(
      FROM_HERE, kUpdateStateDebounceDelay,
      base::BindOnce(&ActorUiTabController::UpdateState,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ActorUiTabController::UpdateState(UiResultCallback callback) {
  // TODO(crbug.com/428216197): Only notify relevant UI components on change.
  if (features::kGlicActorUiOverlay.Get()) {
    actor_overlay_view_controller_->UpdateState(
        current_ui_tab_state_.actor_overlay, ComputeActorOverlayVisibility());
  }
  if (features::kGlicActorUiHandoffButton.Get()) {
    handoff_button_controller_->UpdateState(
        current_ui_tab_state_.handoff_button, ComputeHandoffButtonVisibility());
  }

  if (features::kGlicActorUiTabIndicator.Get()) {
    SetActorTabIndicatorVisibility(current_ui_tab_state_.tab_indicator_visible);
  }

  // Notify the TabGlow controllers.
  if (features::kGlicActorUiBorderGlow.Get()) {
    SetBorderGlowVisibility();
  }

  // TODO(crbug.com/425952887): Change this once ui components are implemented,
  // for now always return true.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), true));

  OnUpdateFinished();
}

void ActorUiTabController::SetBorderGlowVisibility() {
  if (auto* controller =
          ActorBorderViewController::From(tab_->GetBrowserWindowInterface())) {
    controller->SetGlowEnabled(base::to_address(tab_),
                               current_ui_tab_state_.border_glow_visible &&
                                   current_tab_active_status_);
  }
}

bool ActorUiTabController::ComputeActorOverlayVisibility() {
  // Only visible when its state and the associated tab are both active.
  return current_ui_tab_state_.actor_overlay.is_active &&
         current_tab_active_status_;
}

bool ActorUiTabController::ComputeHandoffButtonVisibility() {
  bool is_button_active = current_ui_tab_state_.handoff_button.is_active;
  bool is_client_control =
      current_ui_tab_state_.handoff_button.controller == kClient;

  // Only visible when:
  // 1. Its state and the associated tab is active and the mouse is hovering
  //    over the overlay or the button.
  // 2. Its state and the associated tab is active and the client is in control.
  return current_tab_active_status_ && is_button_active &&
         (is_hovering_overlay_ || is_hovering_button_ || is_client_control);
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
    task->Pause(/*from_actor=*/false);
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

void ActorUiTabController::SetOverlayHoverStatus(bool is_hovering) {
  if (is_hovering_overlay_ == is_hovering) {
    return;
  }
  is_hovering_overlay_ = is_hovering;
  MaybeUpdateState(
      base::BindOnce(&LogAndIgnoreCallbackError, "SetOverlayHoverStatus"));
}

void ActorUiTabController::SetHandoffButtonHoverStatus(bool is_hovering) {
  if (is_hovering_button_ == is_hovering) {
    return;
  }
  is_hovering_button_ = is_hovering;
  MaybeUpdateState(base::BindOnce(&LogAndIgnoreCallbackError,
                                  "SetHandoffButtonHoverStatus"));
}

void ActorUiTabController::OnUpdateFinished() {
  in_progress_updates_int_--;

  // If the controller is now idle, notify the waiting test.
  if (in_progress_updates_int_ == 0 && on_idle_for_testing_) {
    std::move(on_idle_for_testing_).Run();
  }
}

void ActorUiTabController::SetCallbackForTesting(base::OnceClosure callback) {
  on_idle_for_testing_ = std::move(callback);
}

base::WeakPtr<ActorUiTabControllerInterface>
ActorUiTabController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace actor::ui
