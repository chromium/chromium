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
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
  void MaybeShowToast(BrowserWindowInterface* bwi) override;
#endif

  std::optional<std::string> GetActorTaskTitle(TaskId id) override;
  std::optional<raw_ptr<tabs::TabInterface>> GetLastActedOnTab(
      TaskId id) override;
  std::optional<actor::ActorTask::State> GetActorTaskState(TaskId id) override;
  size_t GetInactiveTaskCount() override;

  base::CallbackListSubscription RegisterActorTaskStateChange(
      ActorTaskStateChangeCallback callback) override;
  base::CallbackListSubscription RegisterActorTaskStopped(
      ActorTaskStoppedCallback callback) override;
  base::CallbackListSubscription RegisterActorTaskRemoved(
      ActorTaskRemovedCallback callback) override;

  // Returns the tabs associated with a given task id.
  std::vector<tabs::TabInterface*> GetTabs(TaskId id);

 private:
  // Notify profile scoped ui components about actor task state changes.
  void NotifyActorTaskStateChange(TaskId task_id);
  // Called whenever an actor task state changes.
  void OnActorTaskStateChange(TaskId task_id, ActorTask::State new_task_state);

  // Notify profile scoped ui components about actor task stop.
  void NotifyActorTaskStopped(TaskId task_id);

  // Notify profile scoped ui components about actor task removal.
  // This is called after an actor task has been stopped and has hit its expiry
  // period after `kGlicActorUiCompletedTaskExpiryDelaySeconds` seconds.
  void ActorTaskRemoved(TaskId task_id);

  // Stores completed and failed tasks. Does NOT store tasks intentionally
  // cancelled by the user. Elements in this map are cleared after
  // kGlicActorUiCompletedTaskExpiryDelaySeconds period of time.
  absl::flat_hash_map<TaskId, StoppedTaskInfo> stopped_task_info_;

  base::OneShotTimer notify_actor_task_state_change_debounce_timer_;

  const raw_ref<ActorKeyedService> actor_service_;

  base::RepeatingCallbackList<void(TaskId)>
      actor_task_state_change_callback_list_;

  base::RepeatingCallbackList<void(TaskId)> actor_task_stopped_callback_list_;

  base::RepeatingCallbackList<void(TaskId)> actor_task_removed_callback_list_;

  base::WeakPtrFactory<ActorUiStateManager> weak_factory_{this};
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_H_
