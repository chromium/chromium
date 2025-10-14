// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_INTERFACE_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_INTERFACE_H_

#include "base/time/time.h"
#include "chrome/browser/actor/ui/states/actor_overlay_state.h"
#include "chrome/browser/actor/ui/states/handoff_button_state.h"
#include "chrome/common/actor/task_id.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

namespace actor::ui {
class HandoffButtonController;
using UiResultCallback = base::OnceCallback<void(bool)>;

struct UiTabState {
  bool operator==(const UiTabState& other) const = default;
  ActorOverlayState actor_overlay;
  HandoffButtonState handoff_button;
  bool tab_indicator_visible = false;
  // TODO(crbug.com/447114657) Deprecate the Tab Level border_glow_visible as it
  // is now part of the Overlay.
  bool border_glow_visible = false;
};

// LINT.IfChange(ActorUiTabControllerError)
// These enum values are persisted to logs.  Do not renumber or reuse numeric
// values.

enum class ActorUiTabControllerError {
  kRequestedForNonExistentTab = 0,
  kCallbackError = 1,
  kMaxValue = kCallbackError,
};

// LINT.ThenChange(//tools/metrics/histograms/metadata/actor/enums.xml:ActorUiTabControllerError)

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

  // Called whenever web contents are attached to this tab.
  virtual void OnWebContentsAttached() = 0;

  // Sets the last active task id's state to paused. If there is no task
  // associated to the active task id, this function will do nothing.
  virtual void SetActorTaskPaused() = 0;

  // Sets the last active task id's state to resume. If there is no task
  // associated to the active task id, this function will do nothing.
  virtual void SetActorTaskResume() = 0;

  // Called when the hover status changes on the overlay.
  virtual void OnOverlayHoverStatusChanged(bool is_hovering) = 0;

  // Called when the hover status changes on the handoff button.
  virtual void OnHandoffButtonHoverStatusChanged() = 0;

  // Returns whether the tab should show the actor tab indicator.
  virtual bool ShouldShowActorTabIndicator() = 0;

  virtual base::WeakPtr<ActorUiTabControllerInterface> GetWeakPtr() = 0;

  // Retrieves an ActorUiTabControllerInterface from the provided tab, or
  // nullptr if it does not exist.
  static ActorUiTabControllerInterface* From(tabs::TabInterface* tab);

  // Returns the current UiTabState.
  virtual UiTabState GetCurrentUiTabState() const = 0;

  // Callbacks:
  using ActorTabIndicatorStateChangedCallback =
      base::RepeatingCallback<void(bool)>;
  virtual base::CallbackListSubscription
  RegisterActorTabIndicatorStateChangedCallback(
      ActorTabIndicatorStateChangedCallback callback) = 0;
  using ActorOverlayStateChangeCallback =
      base::RepeatingCallback<void(bool, ActorOverlayState)>;
  virtual base::CallbackListSubscription RegisterActorOverlayStateChange(
      ActorOverlayStateChangeCallback callback) = 0;
  using ActorOverlayBackgroundChangeCallback =
      base::RepeatingCallback<void(bool)>;
  virtual base::CallbackListSubscription RegisterActorOverlayBackgroundChange(
      ActorOverlayBackgroundChangeCallback callback) = 0;

 private:
  ::ui::ScopedUnownedUserData<ActorUiTabControllerInterface>
      scoped_unowned_user_data_;
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_INTERFACE_H_
