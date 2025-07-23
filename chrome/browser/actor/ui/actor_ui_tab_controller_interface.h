// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_INTERFACE_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_INTERFACE_H_

#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/actor/ui/actor_overlay.mojom.h"
#include "chrome/browser/actor/ui/handoff_button_controller.h"
#include "chrome/browser/actor/ui/states/actor_overlay_state.h"
#include "chrome/browser/actor/ui/states/handoff_button_state.h"
#include "components/tabs/public/tab_interface.h"

namespace actor::ui {
using UiResultCallback = base::OnceCallback<void(bool)>;

struct UiTabState {
  bool operator==(const UiTabState& other) const = default;
  ActorOverlayState actor_overlay;
  HandoffButtonState handoff_button;
};

inline std::ostream& operator<<(std::ostream& os, UiTabState state) {
  return os << "UiTabState{\n"
            << "  actor_overlay: " << state.actor_overlay << ",\n"
            << "  handoff_button: " << state.handoff_button << "\n"
            << "}";
}

enum class TabScopedUiComponentType {
  ActorOverlay,
  HandoffButton,
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

  virtual void SetActorTaskResume() = 0;
  // Tab subscriptions:
  // Called when the tab's active state changes.
  virtual void OnTabActiveStatusChanged(bool tab_active_status,
                                        tabs::TabInterface* tab) = 0;

  // Sets the last active task id actuating on this tab.
  // TODO(crbug.com/425952887): At most one task should be acting on a tab at
  // once. In the future we should implement a callback to halt agent execution
  // if the active_task_id is already set and stop agent actuation.
  virtual void SetActiveTaskId(TaskId task_id) = 0;
  // Clears the last active task id actuating on this tab.
  virtual void ClearActiveTaskId() = 0;

  // Sets the visibility of the handoff button.
  virtual void SetHandoffButtonVisibility(bool is_visible) = 0;

  virtual base::WeakPtr<ActorUiTabControllerInterface> GetWeakPtr() = 0;
  virtual void BindActorOverlay(
      mojo::PendingReceiver<mojom::ActorOverlayPageHandler> receiver) = 0;
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_INTERFACE_H_
