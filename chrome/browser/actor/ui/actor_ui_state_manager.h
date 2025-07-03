// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_H_

#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"

namespace tabs {
class TabInterface;
}

class Profile;

namespace actor::ui {

class ActorUiStateManager : public ActorUiStateManagerInterface {
 public:
  explicit ActorUiStateManager(Profile* profile);
  ~ActorUiStateManager() override;

  // ActorUiStateManagerInterface:
  void OnActorTaskStateChange(TaskId task_id,
                              ActorTask::State task_state) override;
  void OnUiEvent(UiEvent event, UiCompleteCallback callback) override;
  void MaybeShowToast() override;
  void NotifyUiTabController(tabs::TabInterface& tab,
                             const UiTabState& ui_tab_state) override;

  // Returns the tabs associated with a given task id if it exists.
  std::vector<tabs::TabInterface*> GetTabs(TaskId id);

 private:
  raw_ptr<Profile> profile_;
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_H_
