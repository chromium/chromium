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

class ActorUiStateManagerInterface {
 public:
  virtual ~ActorUiStateManagerInterface() = default;

  // Handles a UiEvent that may be processed asynchronously.
  virtual void OnUiEvent(AsyncUiEvent event, UiCompleteCallback callback) = 0;
  // Handles a UiEvent that must be processed synchronously.
  virtual void OnUiEvent(SyncUiEvent event) = 0;

  // Shows toast that notifies user the Actor is working in the background.
  // Shows a maximum of kToastShownMax per profile.
  virtual void MaybeShowToast(BrowserWindowInterface* bwi) = 0;

  // Register for this callback to be notified whenever the actor task state
  // changes. This callback may be debounced by a delay.
  using ActorTaskStateChangeCallback = base::RepeatingCallback<void(TaskId)>;
  virtual base::CallbackListSubscription RegisterActorTaskStateChange(
      ActorTaskStateChangeCallback callback) = 0;

  // Register for this callback to be notified whenever the actor task is
  // stopped. This callback will occur immediately once the task enters
  // a stopped state.
  using ActorTaskStoppedCallback = base::RepeatingCallback<
      void(TaskId, ActorTask::State, std::string /*title*/)>;
  virtual base::CallbackListSubscription RegisterActorTaskStopped(
      ActorTaskStoppedCallback callback) = 0;
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_INTERFACE_H_
