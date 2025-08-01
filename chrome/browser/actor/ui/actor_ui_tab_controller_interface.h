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

namespace actor::ui {
class HandoffButtonController;
class ActorOverlayViewController;
using UiResultCallback = base::OnceCallback<void(bool)>;

struct UiTabState {
  bool operator==(const UiTabState& other) const = default;
  ActorOverlayState actor_overlay;
  HandoffButtonState handoff_button;
  bool tab_indicator_visible = false;
};

inline std::ostream& operator<<(std::ostream& os, UiTabState state) {
  return os << "UiTabState{\n"
            << "  actor_overlay: " << state.actor_overlay << ",\n"
            << "  handoff_button: " << state.handoff_button << "\n"
            << "  tab_indicator_visible: " << state.tab_indicator_visible
            << "\n"
            << "}";
}

static constexpr base::TimeDelta kUpdateStateDebounceDelay =
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
  virtual ~ActorUiTabControllerInterface() = default;

  // Called whenever the UiTabState changes.
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

  // Sets the last active task id actuating on this tab.
  // TODO(crbug.com/425952887): At most one task should be acting on a tab at
  // once. In the future we should implement a callback to halt Actor execution
  // if the active_task_id is already set and stop Actor actuation.
  virtual void SetActiveTaskId(TaskId task_id) = 0;
  // Clears the last active task id actuating on this tab.
  virtual void ClearActiveTaskId() = 0;

  // Called when the hover status changes on the overlay and handoff button.
  virtual void SetOverlayHoverStatus(bool is_hovering) = 0;
  virtual void SetHandoffButtonHoverStatus(bool is_hovering) = 0;

  // Returns whether the tab should show the actor tab indicator.
  virtual bool ShouldShowActorTabIndicator() = 0;

  virtual base::WeakPtr<ActorUiTabControllerInterface> GetWeakPtr() = 0;
  virtual void BindActorOverlay(
      mojo::PendingReceiver<mojom::ActorOverlayPageHandler> receiver) = 0;

  // Sets a callback to run when the controller is idle, for tests.
  virtual void SetCallbackForTesting(base::OnceClosure callback) = 0;
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_INTERFACE_H_
