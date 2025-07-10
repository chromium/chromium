// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_H_

#include "base/timer/timer.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"

namespace tabs {
class TabInterface;
}

namespace actor::ui {

class ActorUiStateManager : public ActorUiStateManagerInterface {
 public:
  explicit ActorUiStateManager(ActorKeyedService& actor_service);
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

  // Returns the current profile scoped ui state.
  UiState GetUiState() const;

 private:
  void MaybeUpdateProfileScopedUiState();
  // Returns completed tasks within the kCompletedTaskExpiryDelay of the
  // `current_time`.
  std::vector<TaskId> GetCompletedTasks(base::Time current_time) const;

  base::OneShotTimer update_profile_scoped_ui_debounce_timer_;
  base::OneShotTimer completed_tasks_expiry_timer_;

  const raw_ref<ActorKeyedService> actor_service_;
  UiState state_ = UiState::kInactive;
  base::WeakPtrFactory<ActorUiStateManager> weak_factory_{this};
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_H_
