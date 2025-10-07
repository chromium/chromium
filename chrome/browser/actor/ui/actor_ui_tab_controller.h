// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/actor/ui/handoff_button_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_tab_helper.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace actor {
class ActorKeyedService;
}
namespace actor::ui {

class ActorUiTabControllerFactory
    : public ActorUiTabControllerFactoryInterface {
 public:
  std::unique_ptr<HandoffButtonController> CreateHandoffButtonController(
      tabs::TabInterface& tab) override;
};

class ActorUiTabController : public ActorUiTabControllerInterface,
                             public ImmersiveModeController::Observer,
                             public OmniboxTabHelper::Observer {
 public:
  ActorUiTabController(
      tabs::TabInterface& tab,
      ActorKeyedService* actor_keyed_service,
      std::unique_ptr<ActorUiTabControllerFactoryInterface> controller_factory);
  ~ActorUiTabController() override;
  DECLARE_USER_DATA(ActorUiTabController);

  // ActorUiTabControllerInterface:
  void OnUiTabStateChange(const UiTabState& ui_tab_state,
                          UiResultCallback callback) override;
  void OnWebContentsAttached() override;
  void SetActorTaskPaused() override;
  void SetActorTaskResume() override;
  void OnOverlayHoverStatusChanged(bool is_hovering) override;
  void OnHandoffButtonHoverStatusChanged() override;
  UiTabState GetCurrentUiTabState() const override;
  bool ShouldShowActorTabIndicator() override;

  // ImmersiveModeController::Observer
  void OnImmersiveFullscreenEntered() override;
  void OnImmersiveFullscreenExited() override;
  void OnImmersiveModeControllerDestroyed() override;

  // OmniboxTabHelper::Observer:
  void OnOmniboxInputStateChanged() override {}
  void OnOmniboxInputInProgress(bool in_progress) override {}
  void OnOmniboxFocusChanged(OmniboxFocusState state,
                             OmniboxFocusChangeReason reason) override;
  void OnOmniboxPopupVisibilityChanged(bool popup_is_open) override {}

  base::WeakPtr<ActorUiTabControllerInterface> GetWeakPtr() override;

  base::CallbackListSubscription RegisterActorTabIndicatorStateChangedCallback(
      ActorTabIndicatorStateChangedCallback callback) override;
  base::CallbackListSubscription RegisterActorOverlayStateChange(
      ActorOverlayStateChangeCallback callback) override;
  base::CallbackListSubscription RegisterActorOverlayBackgroundChange(
      ActorOverlayBackgroundChangeCallback callback) override;

 private:
  // Called only once on startup to initialize tab subscriptions.
  void RegisterTabSubscriptions();

  // Called to propagate state and visibility changes to UI controllers.
  void UpdateUi(UiResultCallback callback);

  // Computes whether the Actor Overlay is visible based on the current state.
  bool ComputeActorOverlayVisibility();

  // Computes whether the Handoff Button is visible based on the current state.
  bool ComputeHandoffButtonVisibility();

  // Called when the tab is inserted.
  void OnTabDidInsert(tabs::TabInterface* tab);

  // Run the test callback after updates have been made.
  void OnUpdateFinished();

  // Sets the Tab Indicator visibility.
  void SetActorTabIndicatorVisibility(bool should_show_tab_indicator);

  // Sets the Border Glow visibility.
  void SetBorderGlowVisibility();

  // Initialize and start observing ImmersiveModeController.
  void InitializeImmersiveModeObserver();

  // Updates the visibility of the scrim background. This method is debounced to
  // consolidate rapid hover events from the overlay and the handoff button. It
  // determines if the scrim background should be visible if the mouse is
  // hovering over either the overlay or the handoff button.
  void UpdateScrimBackground();
  void OnTabWillDetach(tabs::TabInterface* tab_interface,
                       tabs::TabInterface::DetachReason reason);
  void OnTabWillDiscard(tabs::TabInterface* tab_interface,
                        content::WebContents* old_contents,
                        content::WebContents* new_contents);

  void UpdateOmniboxTabHelperObserver();

  // The current UiTabState.
  UiTabState current_ui_tab_state_ = {
      .actor_overlay = ActorOverlayState(),
      .handoff_button = HandoffButtonState(),
  };

  // Copy of the current tab's overlay hover status.
  bool is_overlay_hovered_ = false;

  // Determines if the scrim background should be visible. This is set to true
  // if the mouse is hovering over either the overlay or the handoff button.
  bool should_show_scrim_background_ = false;
  bool is_focusing_omnibox_ = false;

  // Owns this class via TabModel.
  const raw_ref<tabs::TabInterface> tab_;
  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  using ActorTabIndicatorStateChangedCallbackList =
      base::RepeatingCallbackList<void(bool)>;
  ActorTabIndicatorStateChangedCallbackList
      on_actor_tab_indicator_changed_callbacks_;
  using ActorOverlayStateChangedCallbackList =
      base::RepeatingCallbackList<void(bool, ActorOverlayState)>;
  ActorOverlayStateChangedCallbackList
      on_actor_overlay_state_changed_callbacks_;
  using ActorOverlayBackgroundChangeCallbackList =
      base::RepeatingCallbackList<void(bool)>;
  ActorOverlayBackgroundChangeCallbackList
      actor_overlay_background_changed_callbacks_;

  // The Actor Keyed Service for the associated profile.
  raw_ptr<ActorKeyedService> actor_keyed_service_ = nullptr;

  // Owned controllers:
  // The Handoff Button controller for this tab.
  std::unique_ptr<HandoffButtonController> handoff_button_controller_;
  std::unique_ptr<ActorUiTabControllerFactoryInterface> controller_factory_;

  bool should_show_actor_tab_indicator_ = false;
  base::RetainingOneShotTimer update_scrim_background_debounce_timer_;

  ::ui::ScopedUnownedUserData<ActorUiTabController> scoped_unowned_user_data_;

  // Observer to get notifications when the immersive mode reveal state changes.
  base::ScopedObservation<ImmersiveModeController,
                          ImmersiveModeController::Observer>
      immersive_mode_observer_{this};

  // Observer to get notifications when the omnibox is focused.
  base::ScopedObservation<OmniboxTabHelper, OmniboxTabHelper::Observer>
      omnibox_tab_helper_observer_{this};

  base::WeakPtrFactory<ActorUiTabController> weak_factory_{this};
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_H_
