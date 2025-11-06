// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_H_

#include "base/timer/timer.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/common/actor/task_id.h"

namespace tabs {
class TabInterface;
}

namespace actor::ui {
class ActorUiStateManager : public ActorUiStateManagerInterface {
 public:
  explicit ActorUiStateManager(ActorKeyedService& actor_service);
  ~ActorUiStateManager() override;

  // ActorUiStateManagerInterface:
  void OnUiEvent(AsyncUiEvent event, UiCompleteCallback callback) override;
  void OnUiEvent(SyncUiEvent event) override;
  void MaybeShowToast(BrowserWindowInterface* bwi) override;

  base::CallbackListSubscription RegisterActorTaskStateChange(
      ActorTaskStateChangeCallback callback) override;
  base::CallbackListSubscription RegisterActorTaskStopped(
      ActorTaskStoppedCallback callback) override;

  // Returns the tabs associated with a given task id.
  std::vector<tabs::TabInterface*> GetTabs(TaskId id);

 private:
  // Notify profile scoped ui components about actor task state changes.
  void NotifyActorTaskStateChange(TaskId task_id);
  // Called whenever an actor task state changes.
  void OnActorTaskStateChange(TaskId task_id,
                              ActorTask::State new_task_state,
                              const std::string& title);

  // Notify profile scoped ui components about actor task stop.
  void NotifyActorTaskStopped(TaskId task_id,
                              ActorTask::State final_state,
                              const std::string& title);

  base::OneShotTimer notify_actor_task_state_change_debounce_timer_;

  const raw_ref<ActorKeyedService> actor_service_;

  base::RepeatingCallbackList<void(TaskId)>
      actor_task_state_change_callback_list_;

  base::RepeatingCallbackList<void(TaskId, ActorTask::State, std::string)>
      actor_task_stopped_callback_list_;

  base::WeakPtrFactory<ActorUiStateManager> weak_factory_{this};
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_H_
