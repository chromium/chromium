// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/actor_border_view_controller.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/actor/ui/actor_ui_window_controller.h"
#include "chrome/browser/actor/ui/handoff_button_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

DEFINE_USER_DATA(actor::ui::ActorUiTabController);

namespace actor::ui {
using ::tabs::TabInterface;

void LogAndIgnoreCallbackError(const std::string_view source_name,
                               bool result) {
  if (!result) {
    LOG(DFATAL) << "Unexpected error in callback from " << source_name;
    RecordTabControllerError(ActorUiTabControllerError::kCallbackError);
  }
}

std::unique_ptr<HandoffButtonController>
ActorUiTabControllerFactory::CreateHandoffButtonController(
    tabs::TabInterface& tab) {
  return std::make_unique<HandoffButtonController>(tab);
}

ActorUiTabController::ActorUiTabController(
    tabs::TabInterface& tab,
    ActorKeyedService* actor_keyed_service,
    std::unique_ptr<ActorUiTabControllerFactoryInterface> controller_factory)
    : ActorUiTabControllerInterface(tab),
      tab_(tab),
      actor_keyed_service_(actor_keyed_service),
      controller_factory_(std::move(controller_factory)),
      update_scrim_background_debounce_timer_(
          FROM_HERE,
          kUpdateScrimBackgroundDebounceDelay,
          base::BindRepeating(&ActorUiTabController::UpdateScrimBackground,
                              base::Unretained(this))),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  CHECK(actor_keyed_service_);
  handoff_button_controller_ =
      controller_factory_->CreateHandoffButtonController(tab);
  RegisterTabSubscriptions();
}

ActorUiTabController::~ActorUiTabController() = default;

void ActorUiTabController::RegisterTabSubscriptions() {
  tab_subscriptions_.push_back(tab_->RegisterWillDetach(base::BindRepeating(
      &ActorUiTabController::OnTabWillDetach, weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillDiscardContents(
      base::BindRepeating(&ActorUiTabController::OnTabWillDiscard,
                          weak_factory_.GetWeakPtr())));
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
  UpdateUi(std::move(callback));
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

void ActorUiTabController::OnTabWillDetach(
    tabs::TabInterface* tab_interface,
    tabs::TabInterface::DetachReason reason) {
  // Reset the omnibox tab helper observation to ensure that it doesn't live
  // longer than the web contents it is observing.
  if (omnibox_tab_helper_observer_.IsObserving()) {
    omnibox_tab_helper_observer_.Reset();
  }
}

void ActorUiTabController::OnTabWillDiscard(
    tabs::TabInterface* tab_interface,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  // Reset the observation of the omnibox tab helper since it is possible for
  // the active tab to be discarded on CrOS.
  if (omnibox_tab_helper_observer_.IsObserving()) {
    omnibox_tab_helper_observer_.Reset();
  }
}

void ActorUiTabController::UpdateOmniboxTabHelperObserver() {
  if (current_ui_tab_state_.handoff_button.is_active) {
    if (!omnibox_tab_helper_observer_.IsObserving()) {
      if (auto* helper =
              OmniboxTabHelper::FromWebContents(tab_->GetContents())) {
        omnibox_tab_helper_observer_.Observe(helper);
      }
    }
  } else {
    omnibox_tab_helper_observer_.Reset();
  }
}

base::CallbackListSubscription
ActorUiTabController::RegisterActorOverlayBackgroundChange(
    ActorOverlayBackgroundChangeCallback callback) {
  return actor_overlay_background_changed_callbacks_.Add(std::move(callback));
}

base::CallbackListSubscription
ActorUiTabController::RegisterActorOverlayStateChange(
    ActorOverlayStateChangeCallback callback) {
  return on_actor_overlay_state_changed_callbacks_.Add(std::move(callback));
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

void ActorUiTabController::UpdateUi(UiResultCallback callback) {
  // TODO(crbug.com/447593256): Propagate errors when component update fails.
  // TODO(crbug.com/428216197): Only notify relevant UI components on change and
  // decouple visibility + state changes into 2 functions.
  if (features::kGlicActorUiOverlay.Get()) {
    on_actor_overlay_state_changed_callbacks_.Notify(
        ComputeActorOverlayVisibility(), current_ui_tab_state_.actor_overlay);
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
  if (callback) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
  }
}

void ActorUiTabController::OnOmniboxFocusChanged(
    OmniboxFocusState state,
    OmniboxFocusChangeReason reason) {
  is_focusing_omnibox_ = state != OmniboxFocusState::OMNIBOX_FOCUS_NONE;
  UpdateUi(base::BindOnce(&LogAndIgnoreCallbackError, "OnOmniboxFocusChanged"));
}

void ActorUiTabController::OnWebContentsAttached() {
  UpdateUi(base::BindOnce(&LogAndIgnoreCallbackError, "OnWebContentsAttached"));
}

void ActorUiTabController::InitializeImmersiveModeObserver() {
  if (immersive_mode_observer_.IsObserving()) {
    return;
  }
  immersive_mode_observer_.Observe(
      ImmersiveModeController::From(tab_->GetBrowserWindowInterface()));
}

void ActorUiTabController::OnImmersiveFullscreenEntered() {
  if (!actor_keyed_service_->GetTaskFromTab(*tab_)) {
    return;
  }
  UpdateUi(base::BindOnce(&LogAndIgnoreCallbackError,
                          "OnImmersiveFullscreenEntered"));
}

void ActorUiTabController::OnImmersiveFullscreenExited() {
  if (!actor_keyed_service_->GetTaskFromTab(*tab_)) {
    return;
  }
  UpdateUi(base::BindOnce(&LogAndIgnoreCallbackError,
                          "OnImmersiveFullscreenExited"));
}

void ActorUiTabController::OnImmersiveModeControllerDestroyed() {
  immersive_mode_observer_.Reset();
}

void ActorUiTabController::SetBorderGlowVisibility() {
  if (auto* controller =
          ActorBorderViewController::From(tab_->GetBrowserWindowInterface())) {
    controller->SetGlowEnabled(
        base::to_address(tab_),
        current_ui_tab_state_.border_glow_visible && tab_->IsSelected());
  }
}

bool ActorUiTabController::ComputeActorOverlayVisibility() {
  // Only visible when its state and the associated tab are both active.
  return current_ui_tab_state_.actor_overlay.is_active && tab_->IsSelected();
}

bool ActorUiTabController::ComputeHandoffButtonVisibility() {
  // TODO(crbug.com/436662421): Clean up this null check for
  // ActorUiWindowController. The GetImmersiveModeController call is done
  // on the BrowserView, which causes crashes in test scenarios where the
  // BrowserView is not properly created in test environments. To ensure a
  // BrowserView exists, we can check if ActorUiWindowController has been
  // created, since its creation relies on a valid BrowserView. Once those tests
  // are cleaned up, this null checks on the window controller can be removed.
  if (!ActorUiWindowController::From(tab_->GetBrowserWindowInterface())) {
    return false;
  }
  InitializeImmersiveModeObserver();
  if (ImmersiveModeController::From(tab_->GetBrowserWindowInterface())
          ->IsEnabled()) {
    return false;
  }
  UpdateOmniboxTabHelperObserver();
  if (is_focusing_omnibox_) {
    return false;
  }

  // Only visible when:
  // 1. Its state is active and the associated tab is selected.
  return tab_->IsSelected() && current_ui_tab_state_.handoff_button.is_active;
}

void ActorUiTabController::SetActorTaskPaused() {
  TaskId task_id = actor_keyed_service_->GetTaskFromTab(*tab_);
  if (!task_id) {
    VLOG(1) << "There is no active task acting on this tab.";
    return;
  }

  if (auto* task = actor_keyed_service_->GetTask(task_id)) {
    task->Pause(/*from_actor=*/false);
  }
}

void ActorUiTabController::SetActorTaskResume() {
  TaskId task_id = actor_keyed_service_->GetTaskFromTab(*tab_);
  if (!task_id) {
    VLOG(1) << "There is no active task acting on this tab.";
    return;
  }

  if (auto* task = actor_keyed_service_->GetTask(task_id)) {
    task->Resume();
  }
}

// TODO(crbug.com/447624564): After migrating the Handoff button off the TDM and
// onto contents container, investigate removing debouncing on the tab
// controller side and handle it on the ui component side.
void ActorUiTabController::UpdateScrimBackground() {
  bool should_show_scrim_background =
      is_overlay_hovered_ || handoff_button_controller_->IsHovering();
  if (should_show_scrim_background_ == should_show_scrim_background) {
    return;
  }
  should_show_scrim_background_ = should_show_scrim_background;
  // TODO(chrstne): Move this notify to UpdateUI + consolidate visibility &
  // background into 1 struct.
  if (features::kGlicActorUiOverlay.Get()) {
    actor_overlay_background_changed_callbacks_.Notify(
        should_show_scrim_background_);
  }
}

void ActorUiTabController::OnOverlayHoverStatusChanged(bool is_hovering) {
  is_overlay_hovered_ = is_hovering;
  update_scrim_background_debounce_timer_.Reset();
}

void ActorUiTabController::OnHandoffButtonHoverStatusChanged() {
  update_scrim_background_debounce_timer_.Reset();
}

base::WeakPtr<ActorUiTabControllerInterface>
ActorUiTabController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

UiTabState ActorUiTabController::GetCurrentUiTabState() const {
  return current_ui_tab_state_;
}

}  // namespace actor::ui
