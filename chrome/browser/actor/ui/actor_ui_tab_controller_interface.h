// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_INTERFACE_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_INTERFACE_H_

#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/actor/ui/actor_overlay.mojom.h"
#include "chrome/browser/actor/ui/states/actor_overlay_state.h"
#include "chrome/browser/actor/ui/states/handoff_button_state.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace actor::ui {
class HandoffButtonController;
class ActorOverlayViewController;
using UiResultCallback = base::OnceCallback<void(bool)>;

struct UiTabState {
  bool operator==(const UiTabState& other) const = default;
  ActorOverlayState actor_overlay;
  HandoffButtonState handoff_button;
  bool tab_indicator_visible = false;
  bool border_glow_visible = false;
};

inline std::ostream& operator<<(std::ostream& os, UiTabState state) {
  return os << "UiTabState{\n"
            << "  actor_overlay: " << state.actor_overlay << ",\n"
            << "  handoff_button: " << state.handoff_button << "\n"
            << "  tab_indicator_visible: " << state.tab_indicator_visible
            << "\n"
            << "  border_glow_visible: " << state.border_glow_visible << "\n"
            << "}";
}

static constexpr base::TimeDelta kUpdateScrimBackgroundDebounceDelay =
    base::Milliseconds(150);

class ActorUiTabControllerFactoryInterface {
 public:
  virtual ~ActorUiTabControllerFactoryInterface() = default;
  virtual std::unique_ptr<HandoffButtonController>
  CreateHandoffButtonController(tabs::TabInterface& tab) = 0;
  virtual std::unique_ptr<ActorOverlayViewController>
  CreateActorOverlayViewController(tabs::TabInterface& tab) = 0;
};

class ActorUiTabControllerInterface {
 public:
  DECLARE_USER_DATA(ActorUiTabControllerInterface);
  explicit ActorUiTabControllerInterface(tabs::TabInterface& tab);
  virtual ~ActorUiTabControllerInterface();

  // Called whenever the UiTabState changes. These calls will be debounced by a
  // kUpdateUiDebounceDelay period of time. This means the callback will always
  // be executed, however it may happen after the UI reaches a future state
  // beyond the one the callback was passed to.
  virtual void OnUiTabStateChange(const UiTabState& ui_tab_state,
                                  UiResultCallback callback) = 0;

  // Sets the last active task id's state to paused. If there is no task
  // associated to the active task id, this function will do nothing.
  virtual void SetActorTaskPaused() = 0;

  // Sets the last active task id's state to resume. If there is no task
  // associated to the active task id, this function will do nothing.
  virtual void SetActorTaskResume() = 0;

  // Tab subscriptions:
  // Called when the tab's active state changes.
  virtual void OnTabActiveStatusChanged(bool tab_active_status,
                                        tabs::TabInterface* tab) = 0;

  // Called when the hover status changes on the overlay.
  virtual void OnOverlayHoverStatusChanged() = 0;

  // Called when the hover status changes on the handoff button.
  virtual void OnHandoffButtonHoverStatusChanged() = 0;

  // Returns whether the tab should show the actor tab indicator.
  virtual bool ShouldShowActorTabIndicator() = 0;

  virtual base::WeakPtr<ActorUiTabControllerInterface> GetWeakPtr() = 0;
  virtual void BindActorOverlay(
      mojo::PendingRemote<mojom::ActorOverlayPage> page,
      mojo::PendingReceiver<mojom::ActorOverlayPageHandler> receiver) = 0;

  // Retrieves an ActorUiTabControllerInterface from the provided tab, or
  // nullptr if it does not exist.
  static ActorUiTabControllerInterface* From(tabs::TabInterface* tab);
  // Returns the current UiTabState.
  virtual UiTabState GetCurrentUiTabState() const = 0;
  // Returns the Actor Overlay View Controller.
  virtual ActorOverlayViewController* GetActorOverlayViewController() = 0;

  using ActorTabIndicatorStateChangedCallback =
      base::RepeatingCallback<void(bool)>;
  virtual base::CallbackListSubscription
  RegisterActorTabIndicatorStateChangedCallback(
      ActorTabIndicatorStateChangedCallback callback) = 0;

 private:
  ::ui::ScopedUnownedUserData<ActorUiTabControllerInterface>
      scoped_unowned_user_data_;
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_INTERFACE_H_
