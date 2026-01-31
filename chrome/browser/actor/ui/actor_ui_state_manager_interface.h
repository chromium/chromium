// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_INTERFACE_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_INTERFACE_H_

#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/actor/ui/ui_event.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/buildflags.h"

namespace actor::ui {
using UiCompleteCallback =
    base::OnceCallback<void(::actor::mojom::ActionResultPtr)>;

static constexpr base::TimeDelta kProfileScopedUiUpdateDebounceDelay =
    base::Milliseconds(500);

struct StoppedTaskInfo {
  ActorTask::State final_state;
  std::string title;
  tabs::TabInterface::Handle last_acted_on_tab_handle;
};

class ActorUiStateManagerInterface {
 public:
  virtual ~ActorUiStateManagerInterface() = default;

  // Handles a UiEvent that may be processed asynchronously.
  virtual void OnUiEvent(AsyncUiEvent event, UiCompleteCallback callback) = 0;
  // Handles a UiEvent that must be processed synchronously.
  virtual void OnUiEvent(SyncUiEvent event) = 0;

#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
  // Shows toast that notifies user the Actor is working in the background.
  // Shows a maximum of kToastShownMax per profile.
  virtual void MaybeShowToast(BrowserWindowInterface* bwi) = 0;
#endif

  // Gets the title of a given task, this includes active tasks and tasks that
  // have stopped within an `kGlicActorUiCompletedTaskExpiryDelaySeconds` period
  // of time.
  // "" is returned when we had a task but it never set a title.
  // std::nullopt is returned when we don't have a task for the given id.
  virtual std::optional<std::string> GetActorTaskTitle(TaskId id) = 0;
  // Gets the last tab handle that was acted on by the actor for a given task,
  // this includes active tasks and tasks that have stopped within an
  // `kGlicActorUiCompletedTaskExpiryDelaySeconds` period of time.
  // nullptr is returned when we had a task but it never acted on a tab.
  // std::nullopt is returned when we don't have a task for the given id.
  virtual std::optional<raw_ptr<tabs::TabInterface>> GetLastActedOnTab(
      TaskId id) = 0;
  // Gets the state of a given task, this includes active tasks and tasks that
  // have stopped within an `kGlicActorUiCompletedTaskExpiryDelaySeconds` period
  // of time.
  // std::nullopt is returned when we don't have a task for the given id.
  virtual std::optional<actor::ActorTask::State> GetActorTaskState(
      TaskId id) = 0;

  // Gets the number of inactive tasks (finished and failed). Cancelled tasks
  // are not included in this count.
  virtual size_t GetInactiveTaskCount() = 0;

  // Register for this callback to be notified whenever the actor task state
  // changes. This callback may be debounced by a delay.
  using ActorTaskStateChangeCallback = base::RepeatingCallback<void(TaskId)>;
  virtual base::CallbackListSubscription RegisterActorTaskStateChange(
      ActorTaskStateChangeCallback callback) = 0;

  // Register for this callback to be notified whenever the actor task is
  // stopped. This callback will occur immediately once the task enters
  // a stopped state.
  using ActorTaskStoppedCallback = base::RepeatingCallback<void(TaskId)>;
  virtual base::CallbackListSubscription RegisterActorTaskStopped(
      ActorTaskStoppedCallback callback) = 0;

  // Register for this callback to be notified whenever the actor task has hit
  // its expiry period after being stopped/cleared after
  // `kGlicActorUiCompletedTaskExpiryDelaySeconds` seconds.
  using ActorTaskRemovedCallback = base::RepeatingCallback<void(TaskId)>;
  virtual base::CallbackListSubscription RegisterActorTaskRemoved(
      ActorTaskRemovedCallback callback) = 0;
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_INTERFACE_H_
