// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_H_
#define CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_H_

#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_ui_state_manager_interface.h"
#include "chrome/browser/actor/task_id.h"

namespace actor {

// TODO(crbug.com/424495020): Implement this class.
class ActorUiStateManager : public ActorUiStateManagerInterface {
 public:
  ActorUiStateManager();
  ~ActorUiStateManager() override;

  // ActorUiStateManagerInterface:
  void OnActorTaskStateChange(TaskId task_id,
                              ActorTask::State task_state) override;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_UI_ACTOR_UI_STATE_MANAGER_H_
