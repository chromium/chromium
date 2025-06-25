// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_ui_state_manager.h"

namespace actor {

ActorUIStateManager::ActorUIStateManager() = default;
ActorUIStateManager::~ActorUIStateManager() = default;

void ActorUIStateManager::OnActorTaskStateChange(TaskId task_id,
                                                 ActorTask::State task_state) {
  // TODO(crbug.com/424495020): Implement this function.
}

}  // namespace actor
