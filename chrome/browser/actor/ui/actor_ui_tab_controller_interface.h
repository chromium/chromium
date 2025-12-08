// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_INTERFACE_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_INTERFACE_H_

#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/actor/ui/states/actor_overlay_state.h"
#include "chrome/browser/actor/ui/states/handoff_button_state.h"
#include "chrome/browser/actor/ui/states/tab_indicator_state.h"
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
  TabIndicatorStatus tab_indicator = TabIndicatorStatus::kNone;
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
            << "  tab_indicator_status: "
            << static_cast<int>(state.tab_indicator) << "\n"
            << "  border_glow_visible: " << state.border_glow_visible << "\n"
            << "}";
}

class ActorUiTabControllerFactoryInterface {
 public:
  virtual ~ActorUiTabControllerFactoryInterface() = default;
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

  // Called whenever the view bounds of the web view attached to this tab
  // change.
  virtual void OnViewBoundsChanged() = 0;

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

  // Called when the focus status changes on the handoff button.
  virtual void OnHandoffButtonFocusStatusChanged() = 0;

  // Called when the window's immersive mode state changes.
  virtual void OnImmersiveModeChanged() = 0;

  [[nodiscard]] virtual base::ScopedClosureRunner
  RegisterHandoffButtonController(HandoffButtonController* controller) = 0;

  virtual base::WeakPtr<ActorUiTabControllerInterface> GetWeakPtr() = 0;

  // Retrieves an ActorUiTabControllerInterface from the provided tab, or
  // nullptr if it does not exist.
  static ActorUiTabControllerInterface* From(tabs::TabInterface* tab);

  // Returns the current UiTabState.
  virtual UiTabState GetCurrentUiTabState() const = 0;

  // Callbacks:
  using ActorTabIndicatorStateChangedCallback =
      base::RepeatingCallback<void(TabIndicatorStatus)>;
  [[nodiscard]] virtual base::ScopedClosureRunner
  RegisterActorTabIndicatorStateChangedCallback(
      ActorTabIndicatorStateChangedCallback callback) = 0;
  using ActorOverlayStateChangeCallback =
      base::RepeatingCallback<void(bool, ActorOverlayState, base::OnceClosure)>;
  [[nodiscard]] virtual base::ScopedClosureRunner
  RegisterActorOverlayStateChange(ActorOverlayStateChangeCallback callback) = 0;
  using ActorOverlayBackgroundChangeCallback =
      base::RepeatingCallback<void(bool)>;
  [[nodiscard]] virtual base::ScopedClosureRunner
  RegisterActorOverlayBackgroundChange(
      ActorOverlayBackgroundChangeCallback callback) = 0;

 private:
  ::ui::ScopedUnownedUserData<ActorUiTabControllerInterface>
      scoped_unowned_user_data_;
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_INTERFACE_H_
