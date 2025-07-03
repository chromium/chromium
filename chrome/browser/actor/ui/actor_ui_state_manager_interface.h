// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_INTERFACE_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_INTERFACE_H_

#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/actor/ui/actor_ui_tab_controller.h"
#include "chrome/browser/actor/ui/ui_event.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor::ui {
using UiCompleteCallback = base::OnceCallback<void(mojom::ActionResultPtr)>;
class ActorUiStateManagerInterface {
 public:
  virtual ~ActorUiStateManagerInterface() = default;

  // Called whenever an actor task state changes.
  virtual void OnActorTaskStateChange(TaskId task_id,
                                      ActorTask::State task_state) = 0;

  // Called whenever a ui event occurs.
  virtual void OnUiEvent(UiEvent event, UiCompleteCallback callback) = 0;

  // Notifies the ActorUiTabController of a new `ui_tab_state`.
  // Can be stubbed out to do nothing in tests.
  virtual void NotifyUiTabController(tabs::TabInterface& tab,
                                     const UiTabState& ui_tab_state) = 0;

  // Shows toast that notifies user the agent is working in the background.
  // Shows a maximum of kToastShownMax per profile.
  // TODO(crbug.com/428014205): Define kToastShownMax.
  virtual void MaybeShowToast() = 0;
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_INTERFACE_H_
