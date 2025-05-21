// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_task.h"

#include "chrome/browser/actor/actor_coordinator.h"

namespace actor {

ActorTask::ActorTask() = default;
ActorTask::ActorTask(std::unique_ptr<ActorCoordinator> actor_coordinator)
    : actor_coordinator_(std::move(actor_coordinator)) {}
ActorTask::~ActorTask() = default;

ActorCoordinator* ActorTask::GetActorCoordinator() const {
  return actor_coordinator_.get();
}

ActorTask::State ActorTask::GetState() const {
  return state_;
}

void ActorTask::SetState(State state) {
  state_ = state;
}

}  // namespace actor
