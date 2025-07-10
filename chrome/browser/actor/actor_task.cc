// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_task.h"

#include <ostream>

#include "base/no_destructor.h"
#include "base/state_transitions.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/common/actor.mojom-data-view.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/action_result.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

ActorTask::ActorTask(std::unique_ptr<ExecutionEngine> execution_engine)
    : execution_engine_(std::move(execution_engine)) {}
ActorTask::~ActorTask() = default;

void ActorTask::SetId(base::PassKey<ActorKeyedService>, TaskId id) {
  id_ = id;
}

void ActorTask::SetIdForTesting(int id) {
  id_ = TaskId(id);
}

ExecutionEngine* ActorTask::GetExecutionEngine() const {
  return execution_engine_.get();
}

ActorTask::State ActorTask::GetState() const {
  return state_;
}

void ActorTask::SetState(State state) {
  using enum State;
  VLOG(1) << "ActorTask state change: " << state_ << " -> " << state;
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
  task_state_change_callback_list_.Notify(id_, state_);
}

void ActorTask::Act(const optimization_guide::proto::BrowserAction& action,
                    ActionResultCallback callback) {
  if (state_ == State::kPausedByClient) {
    std::move(callback).Run(MakeResult(mojom::ActionResultCode::kTaskPaused));
    return;
  }
  SetState(State::kActing);
  execution_engine_->Act(
      action,
      base::BindOnce(&ActorTask::OnFinishedActDeprecated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ActorTask::OnFinishedActDeprecated(ActionResultCallback callback,
                                        mojom::ActionResultPtr result) {
  if (state_ != State::kActing) {
    std::move(callback).Run(MakeErrorResult());
    return;
  }
  SetState(State::kReflecting);
  std::move(callback).Run(std::move(result));
}

void ActorTask::Act(std::vector<std::unique_ptr<ToolRequest>>& actions,
                    ActCallback callback) {
  if (state_ == State::kPausedByClient) {
    std::move(callback).Run(MakeResult(mojom::ActionResultCode::kTaskPaused),
                            std::nullopt);
    return;
  }
  SetState(State::kActing);
  execution_engine_->Act(actions, base::BindOnce(&ActorTask::OnFinishedAct,
                                                 weak_ptr_factory_.GetWeakPtr(),
                                                 std::move(callback)));
}

void ActorTask::OnFinishedAct(ActCallback callback,
                              mojom::ActionResultPtr result,
                              std::optional<size_t> index_of_failed_action) {
  if (state_ != State::kActing) {
    std::move(callback).Run(MakeErrorResult(), std::nullopt);
    return;
  }
  SetState(State::kReflecting);
  std::move(callback).Run(std::move(result), std::nullopt);
}

void ActorTask::Stop() {
  if (execution_engine_) {
    execution_engine_->CancelOngoingActions(
        mojom::ActionResultCode::kTaskWentAway);
  }
  end_time_ = base::Time::Now();
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

base::Time ActorTask::GetEndTime() const {
  return end_time_;
}

base::CallbackListSubscription ActorTask::RegisterTaskStateChange(
    TaskStateChangeCallback callback) {
  return task_state_change_callback_list_.Add(std::move(callback));
}

void ActorTask::AddToTabSet(tabs::TabHandle tab_handle) {
  tab_handles_.insert(tab_handle);
}

bool ActorTask::HasActedOnTab(tabs::TabHandle tab) const {
  return tab_handles_.contains(tab);
}

tabs::TabInterface* ActorTask::GetTabForObservation() const {
  CHECK_EQ(tab_handles_.size(), 1ul);
  for (const tabs::TabHandle& handle : tab_handles_) {
    if (tabs::TabInterface* tab = handle.Get()) {
      return tab;
    }
  }

  return nullptr;
}

std::string ToString(const ActorTask::State& state) {
  using enum ActorTask::State;
  switch (state) {
    case kCreated:
      return "Created";
    case kActing:
      return "Acting";
    case kReflecting:
      return "Reflecting";
    case kPausedByClient:
      return "PausedByClient";
    case kFinished:
      return "Finished";
  }
}

std::ostream& operator<<(std::ostream& os, const ActorTask::State& state) {
  return os << ToString(state);
}

}  // namespace actor
