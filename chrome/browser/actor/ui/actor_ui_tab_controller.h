// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {
class ActorKeyedService;
}

namespace actor::ui {

class ActorUiTabController : public ActorUiTabControllerInterface {
 public:
  ActorUiTabController(tabs::TabInterface& tab,
                       ActorKeyedService* actor_service);
  ~ActorUiTabController() override;

  // ActorUiTabControllerInterface:
  void OnUiTabStateChange(const UiTabState& ui_tab_state,
                          UiResultCallback callback) override;
  void SetActiveTaskId(TaskId task_id) override;
  void ClearActiveTaskId() override;
  base::WeakPtr<ActorUiTabControllerInterface> GetWeakPtr() override;
  void SetActorTaskPaused() override;
  void SetActorTaskResume() override;

 private:
  // Notifies tab scoped ui components that their state has changed.
  void NotifyTabScopedUiComponents(const UiTabState& ui_tab_state,
                                   bool tab_activated);
  // Tab subscriptions:
  // Called when this tab's activation status changes.
  void OnTabActivationChanged(bool is_activated, tabs::TabInterface* tab);
  // Called when the tab is detached.
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);
  // Called when the tab is inserted.
  void OnTabDidInsert(tabs::TabInterface* tab);

  // Holds subscriptions for TabInterface callbacks.
  std::vector<base::CallbackListSubscription> tab_subscriptions_;

  // Owns this class via TabModel.
  const raw_ref<tabs::TabInterface> tab_;

  // The current ui tab state.
  UiTabState current_ui_tab_state_ = {
      .actor_overlay = ActorOverlayState(),
      .handoff_button = HandoffButtonState(),
  };
  // The last active task id actuating on this tab.
  TaskId active_task_id_;

  raw_ptr<ActorKeyedService> actor_keyed_service_ = nullptr;

  base::WeakPtrFactory<ActorUiTabController> weak_factory_{this};
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_TAB_CONTROLLER_H_
