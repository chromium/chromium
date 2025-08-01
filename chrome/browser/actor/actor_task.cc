// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_task.h"

#include <memory>
#include <ostream>

#include "base/no_destructor.h"
#include "base/state_transitions.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor.mojom-data-view.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/action_result.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace actor {

ActorTask::ActorTask(Profile* profile,
                     std::unique_ptr<ExecutionEngine> execution_engine,
                     std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher)
    : profile_(profile),
      execution_engine_(std::move(execution_engine)),
      ui_event_dispatcher_(std::move(ui_event_dispatcher)),
      ui_weak_ptr_factory_(ui_event_dispatcher_.get()) {}

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

  ui_event_dispatcher_->OnActorTaskSyncChange(
      ui::UiEventDispatcher::ChangeTaskState{
          .task_id = id_, .old_state = state_, .new_state = state});
  state_ = state;
  actor::ActorKeyedService::Get(profile_)->NotifyTaskStateChanged(*this);
}

void ActorTask::Act(std::vector<std::unique_ptr<ToolRequest>>&& actions,
                    ActCallback callback) {
  if (state_ == State::kPausedByClient) {
    std::move(callback).Run(MakeResult(mojom::ActionResultCode::kTaskPaused),
                            std::nullopt);
    return;
  }
  if (state_ == State::kFinished) {
    std::move(callback).Run(MakeResult(mojom::ActionResultCode::kTaskWentAway),
                            std::nullopt);
    return;
  }
  SetState(State::kActing);
  execution_engine_->Act(
      std::move(actions),
      base::BindOnce(&ActorTask::OnFinishedAct, weak_ptr_factory_.GetWeakPtr(),
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
  // Remove all the tabs from the task.
  auto tabs_to_remove = tab_handles_;
  for (auto& tab : tabs_to_remove) {
    RemoveTab(tab);
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

base::Time ActorTask::GetEndTime() const {
  return end_time_;
}

void ActorTask::AddTab(tabs::TabHandle tab_handle, AddTabCallback callback) {
  if (tab_handles_.contains(tab_handle)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), MakeOkResult()));
    return;
  }

  CHECK(!actuation_mode_runners_.contains(tab_handle));
  if (tab_handle.Get() && tab_handle.Get()->GetContents()) {
    content::WebContents* web_contents = tab_handle.Get()->GetContents();
    actuation_mode_runners_.emplace(
        tab_handle, web_contents->IncrementCapturerCount(gfx::Size(),
                                                         /*stay_hidden=*/false,
                                                         /*stay_awake=*/true,
                                                         /*is_activity=*/true));
  }

  // Notify the UI of the new tab.
  tab_handles_.insert(tab_handle);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ui::UiEventDispatcher::OnActorTaskAsyncChange,
                                ui_weak_ptr_factory_.GetWeakPtr(),
                                ui::UiEventDispatcher::AddTab{
                                    .task_id = id_, .handle = tab_handle},
                                std::move(callback)));
}

void ActorTask::RemoveTab(tabs::TabHandle tab_handle) {
  // Erasing the ScopedClosureRunner from the map triggers its destructor, which
  // automatically calls DecrementCapturerCount on the WebContents.
  actuation_mode_runners_.erase(tab_handle);

  auto num_removed = tab_handles_.erase(tab_handle);
  if (num_removed > 0) {
    // Notify the UI of the tab removal.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ui::UiEventDispatcher::OnActorTaskSyncChange,
                                  ui_weak_ptr_factory_.GetWeakPtr(),
                                  ui::UiEventDispatcher::RemoveTab{
                                      .task_id = id_, .handle = tab_handle}));
  }
}

bool ActorTask::IsActingOnTab(tabs::TabHandle tab) const {
  return tab_handles_.contains(tab);
}

tabs::TabInterface* ActorTask::GetTabForObservation() const {
  DCHECK_GT(tab_handles_.size(), 0ul);
  DCHECK_LT(tab_handles_.size(), 2ul);
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
