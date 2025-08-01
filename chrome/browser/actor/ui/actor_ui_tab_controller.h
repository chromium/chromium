// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/actor/ui/actor_overlay.mojom.h"
#include "chrome/browser/actor/ui/actor_overlay_view_controller.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/actor/ui/handoff_button_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace actor {
class ActorKeyedService;
}

namespace actor::ui {

class ActorUiTabControllerFactory
    : public ActorUiTabControllerFactoryInterface {
 public:
  std::unique_ptr<HandoffButtonController> CreateHandoffButtonController(
      tabs::TabInterface& tab) override;
  std::unique_ptr<ActorOverlayViewController> CreateActorOverlayViewController(
      tabs::TabInterface& tab) override;
};

class ActorUiTabController : public ActorUiTabControllerInterface {
 public:
  ActorUiTabController(
      tabs::TabInterface& tab,
      ActorKeyedService* actor_service,
      std::unique_ptr<ActorUiTabControllerFactoryInterface> controller_factory);
  ~ActorUiTabController() override;

  // ActorUiTabControllerInterface:
  void OnUiTabStateChange(const UiTabState& ui_tab_state,
                          UiResultCallback callback) override;
  void OnTabActiveStatusChanged(bool tab_active_status,
                                tabs::TabInterface* tab) override;
  void SetActiveTaskId(TaskId task_id) override;
  void ClearActiveTaskId() override;
  base::WeakPtr<ActorUiTabControllerInterface> GetWeakPtr() override;
  void SetActorTaskPaused() override;
  void SetActorTaskResume() override;
  void SetOverlayHoverStatus(bool is_hovering) override;
  void SetHandoffButtonHoverStatus(bool is_hovering) override;
  void SetCallbackForTesting(base::OnceClosure callback) override;
  bool ShouldShowActorTabIndicator() override;

  // Binds the Mojo receiver to the tab's ActorOverlayViewController.
  // Called by ActorOverlayUI when the chrome://actor-overlay page loads.
  void BindActorOverlay(
      mojo::PendingReceiver<mojom::ActorOverlayPageHandler> receiver) override;

 private:
  // Called only once on startup to initialize tab subscriptions.
  void RegisterTabSubscriptions();

  // Called to propagate a UiTabState and tab status change to UI controllers.
  // This is passed through a debounce timer to stabilize updates.
  void MaybeUpdateState(const UiTabState& ui_tab_state,
                        bool tab_active_status,
                        UiResultCallback callback);
  void UpdateState(const UiTabState& ui_tab_state,
                   bool tab_active_status,
                   UiResultCallback callback);

  // Computes whether the Actor Overlay is visible based on the current state.
  bool ComputeActorOverlayVisibility();

  // Computes whether the Handoff Button is visible based on the current state.
  bool ComputeHandoffButtonVisibility();

  // Tab subscriptions:
  // Called when the tab is detached.
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);
  // Called when the tab is inserted.
  void OnTabDidInsert(tabs::TabInterface* tab);

  // Run the test callback after updates have been made.
  void OnUpdateFinished();

  // Sets the Tab Indicator visibility.
  void SetActorTabIndicatorVisibility(bool should_show_tab_indicator);

  // The current UiTabState.
  UiTabState current_ui_tab_state_ = {
      .actor_overlay = ActorOverlayState(),
      .handoff_button = HandoffButtonState(),
  };

  // The current active status of the tab.
  bool current_tab_active_status_ = false;
  // The last active task id actuating on this tab.
  TaskId active_task_id_;

  bool is_hovering_overlay_ = false;
  bool is_hovering_button_ = false;

  // How many outstanding callbacks are pending for the debounce timer.
  int in_progress_updates_int_ = 0;

  base::OneShotTimer update_state_debounce_timer_;
  base::OnceClosure on_idle_for_testing_;

  // Owns this class via TabModel.
  const raw_ref<tabs::TabInterface> tab_;
  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;
  // The Actor Keyed Service for the associated profile.
  raw_ptr<ActorKeyedService> actor_keyed_service_ = nullptr;

  // Owned controllers:
  // The Actor Overlay View controller for this tab.
  std::unique_ptr<ActorOverlayViewController> actor_overlay_view_controller_;
  // The Handoff Button controller for this tab.
  std::unique_ptr<HandoffButtonController> handoff_button_controller_;
  std::unique_ptr<ActorUiTabControllerFactoryInterface> controller_factory_;

  bool should_show_actor_tab_indicator_ = false;

  base::WeakPtrFactory<ActorUiTabController> weak_factory_{this};
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_H_
