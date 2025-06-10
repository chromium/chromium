// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_task.h"

#include <ostream>

#include "base/no_destructor.h"
#include "base/state_transitions.h"
#include "chrome/browser/actor/execution_engine.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

ActorTask::ActorTask() = default;
ActorTask::ActorTask(std::unique_ptr<ExecutionEngine> execution_engine)
    : execution_engine_(std::move(execution_engine)) {
  execution_engine_->SetOwner(this);
}
ActorTask::~ActorTask() = default;

void ActorTask::SetId(base::PassKey<ActorKeyedService>, TaskId id) {
  id_ = id;
}

ExecutionEngine* ActorTask::GetExecutionEngine() const {
  return execution_engine_.get();
}

ActorTask::State ActorTask::GetState() const {
  return state_;
}

void ActorTask::SetState(State state) {
  using enum State;
#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<State>>
      allowed_transitions(base::StateTransitions<State>({
          {kCreated, {kActing, kReflecting, kPausedByClient, kFinished}},
          {kActing, {kReflecting, kPausedByClient, kFinished}},
          {kReflecting, {kActing, kPausedByClient, kFinished}},
          {kPausedByClient, {kActing, kReflecting, kFinished}},
          {kFinished, {}},
      }));
  if (state != state_) {
    DCHECK_STATE_TRANSITION(allowed_transitions,
                            /*old_state=*/state_,
                            /*new_state=*/state);
  }
#endif  // DCHECK_IS_ON()

  state_ = state;
}

void ActorTask::Stop() {
  if (execution_engine_) {
    execution_engine_->CancelOngoingActions(
        mojom::ActionResultCode::kTaskWentAway);
  }
  SetState(State::kFinished);
}

void ActorTask::Pause() {
  if (GetState() == State::kFinished) {
    return;
  }
  if (execution_engine_) {
    execution_engine_->CancelOngoingActions(
        mojom::ActionResultCode::kTaskPaused);
  }
  SetState(State::kPausedByClient);
}

void ActorTask::Resume() {
  if (GetState() != State::kFinished) {
    SetState(State::kReflecting);
  }
}

bool ActorTask::IsPaused() const {
  return GetState() == State::kPausedByClient;
}

std::ostream& operator<<(std::ostream& os, const ActorTask::State& state) {
  using enum ActorTask::State;
  switch (state) {
    case kCreated:
      return os << "Created";
    case kActing:
      return os << "Acting";
    case kReflecting:
      return os << "Reflecting";
    case kPausedByClient:
      return os << "PausedByClient";
    case kFinished:
      return os << "Finished";
  }
}

}  // namespace actor
