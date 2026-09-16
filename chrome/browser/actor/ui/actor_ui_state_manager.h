// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/timer/timer.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager_interface.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/glic_enums.mojom.h"
#include "components/actor/core/task_id.h"
#include "components/tabs/public/tab_interface.h"

class BrowserWindowInterface;

namespace content {
class BrowserContext;
}

#if !BUILDFLAG(IS_ANDROID)
namespace actor {
class ActorTabStripTrackerDesktop;
}
#endif

namespace actor::ui {

struct UiTabState;

inline constexpr base::TimeDelta kProfileScopedUiUpdateDebounceDelay =
    base::Milliseconds(500);

struct StoppedTaskInfo {
  ActorTask::State final_state;
  std::string title;
  tabs::TabInterface::Handle last_acted_on_tab_handle;
  ActorTask::TaskDuration duration;
  glic::mojom::FeatureMode feature_mode;
};

class ActorUiStateManager : public ActorUiStateManagerInterface {
 public:
  // Register for this callback to be notified whenever the actor task state
  // changes. This callback may be debounced by a delay.
  using ActorTaskStateChangeCallback = base::RepeatingCallback<void(TaskId)>;

  // Register for this callback to be notified whenever the actor task is
  // stopped. This callback will occur immediately once the task enters
  // a stopped state.
  using ActorTaskStoppedCallback = base::RepeatingCallback<void(TaskId)>;

  // Register for this callback to be notified whenever the actor task has hit
  // its expiry period after being stopped/cleared after
  // `kGlicActorUiCompletedTaskExpiryDelaySeconds` seconds.
  using ActorTaskRemovedCallback = base::RepeatingCallback<void(TaskId)>;

  // Returns the ActorUiStateManager for the given context. May return nullptr.
  static ActorUiStateManager* Get(content::BrowserContext* context);

  explicit ActorUiStateManager(ActorKeyedService& actor_service);
  ~ActorUiStateManager() override;

  // ActorUiStateManagerInterface:
  void OnUiEvent(AsyncUiEvent event, UiCompleteCallback callback) override;
  void OnUiEvent(SyncUiEvent event) override;
#if !BUILDFLAG(IS_ANDROID)
  void LazyInitTabTracker() override;
#endif

#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
  // Shows toast that notifies user the Actor is working in the background.
  // Shows a maximum of kToastShownMax per profile.
  void MaybeShowToast(BrowserWindowInterface* bwi);
#endif

  // Gets the title of a given task, this includes active tasks and tasks that
  // have stopped within an `kGlicActorUiCompletedTaskExpiryDelaySeconds` period
  // of time.
  // "" is returned when we had a task but it never set a title.
  // std::nullopt is returned when we don't have a task for the given id.
  std::optional<std::string> GetActorTaskTitle(TaskId id);

  // Gets the last tab handle that was acted on by the actor for a given task,
  // this includes active tasks and tasks that have stopped within an
  // `kGlicActorUiCompletedTaskExpiryDelaySeconds` period of time.
  // nullptr is returned when we had a task but it never acted on a tab.
  // std::nullopt is returned when we don't have a task for the given id.
  std::optional<raw_ptr<tabs::TabInterface>> GetLastActedOnTab(TaskId id);

  // Gets the state of a given task, this includes active tasks and tasks that
  // have stopped within an `kGlicActorUiCompletedTaskExpiryDelaySeconds` period
  // of time.
  // std::nullopt is returned when we don't have a task for the given id.
  std::optional<actor::ActorTask::State> GetActorTaskState(TaskId id);

  std::optional<actor::ActorTask::InterruptReason> GetActorTaskInterruptReason(
      TaskId id);

  // Gets the duration of a given task.
  ActorTask::TaskDuration GetDuration(TaskId task_id);

  // Gets the feature mode of a given task.
  glic::mojom::FeatureMode GetFeatureMode(TaskId task_id);

  // Gets the number of inactive tasks (finished and failed). Cancelled tasks
  // are not included in this count.
  size_t GetInactiveTaskCount();

  // Register for this callback to be notified whenever the actor task state
  // changes. This callback may be debounced by a delay.
  base::CallbackListSubscription RegisterActorTaskStateChange(
      ActorTaskStateChangeCallback callback);

  // Register for this callback to be notified whenever the actor task is
  // stopped. This callback will occur immediately once the task enters
  // a stopped state.
  base::CallbackListSubscription RegisterActorTaskStopped(
      ActorTaskStoppedCallback callback);

  // Register for this callback to be notified whenever the actor task has hit
  // its expiry period after being stopped/cleared after
  // `kGlicActorUiCompletedTaskExpiryDelaySeconds` seconds.
  base::CallbackListSubscription RegisterActorTaskRemoved(
      ActorTaskRemovedCallback callback);

  // Returns the tabs associated with a given task id.
  std::vector<tabs::TabInterface*> GetTabs(TaskId id);

  // Sets the given tab to a pending actuation state (showing the dynamic
  // indicator before an ActorTask is created).
  void SetTabPendingActuation(tabs::TabHandle tab_handle);

  // Clears the pending actuation state for the given tab. Returns true if the
  // tab was in pending actuation and was cleared.
  bool ClearTabPendingActuation(tabs::TabHandle tab_handle);

 private:
  void OnPendingTabDetached(tabs::TabInterface* tab,
                            tabs::TabInterface::DetachReason reason);
  UiTabState GetActorControlledUiTabState(TaskId task_id);
  UiTabState GetActorControlledUiTabState(const tabs::TabInterface* tab);
  void OnTransientTaskDelayExpired(TaskId task_id);
  void StopTimer(TaskId task_id);

  ActorTask::TaskDuration GetDuration(const tabs::TabInterface* tab);
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

  // One-shot timers for active transient tasks UI delays.
  absl::flat_hash_map<TaskId, std::unique_ptr<base::OneShotTimer>>
      transient_task_timers_;

  base::OneShotTimer notify_actor_task_state_change_debounce_timer_;

  // Tracks tabs currently in a pending actuation state prior to an ActorTask
  // starting.
  absl::flat_hash_map<tabs::TabHandle, base::CallbackListSubscription>
      pending_actuation_tabs_;

  const raw_ref<ActorKeyedService> actor_service_;

#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<actor::ActorTabStripTrackerDesktop> tab_strip_tracker_;
#endif

  base::RepeatingCallbackList<void(TaskId)>
      actor_task_state_change_callback_list_;

  base::RepeatingCallbackList<void(TaskId)> actor_task_stopped_callback_list_;

  base::RepeatingCallbackList<void(TaskId)> actor_task_removed_callback_list_;

  base::WeakPtrFactory<ActorUiStateManager> weak_factory_{this};
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_H_
