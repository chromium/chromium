// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/actor_ui_state_manager.h"

#include "chrome/common/actor/action_result.h"

namespace actor::ui {

ActorUiStateManager::ActorUiStateManager() = default;
ActorUiStateManager::~ActorUiStateManager() = default;

void ActorUiStateManager::OnActorTaskStateChange(TaskId task_id,
                                                 ActorTask::State task_state) {
  // TODO(crbug.com/424495020): Implement this function.
}

void ActorUiStateManager::OnUiEvent(UiEvent event,
                                    UiCompleteCallback callback) {
  // TODO(crbug.com/424495020): Implement this function.
  std::move(callback).Run(MakeOkResult());
}

void ActorUiStateManager::MaybeShowToast() {
  // TODO(crbug.com/428014205): Implement this function.
}

}  // namespace actor::ui
