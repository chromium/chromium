// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_task.h"

#include <memory>
#include <ostream>

#include "base/metrics/histogram_functions.h"
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
#include "ui/gfx/geometry/size.h"

namespace actor {

ActorTask::ActingTabState::ActingTabState() = default;
ActorTask::ActingTabState::~ActingTabState() = default;
ActorTask::ActingTabState::ActingTabState(ActingTabState&&) = default;
ActorTask::ActingTabState& ActorTask::ActingTabState::operator=(
    ActingTabState&&) = default;

ActorTask::ActorTask(Profile* profile,
                     std::unique_ptr<ExecutionEngine> execution_engine,
                     std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher,
                     webui::mojom::TaskOptionsPtr options)
    : profile_(profile),
      execution_engine_(std::move(execution_engine)),
      ui_event_dispatcher_(std::move(ui_event_dispatcher)),
      ui_weak_ptr_factory_(ui_event_dispatcher_.get()) {
  if (options && options->title.has_value()) {
    title_ = options->title.value();
  }
}

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
      allowed_transitions(base::StateTransitions<State>(
          {{kCreated,
            {kActing, kReflecting, kPausedByActor, kPausedByUser, kCancelled,
             kFinished}},
           {kActing,
            {kReflecting, kPausedByActor, kPausedByUser, kCancelled,
             kFinished}},
           {kReflecting,
            {kActing, kPausedByActor, kPausedByUser, kCancelled, kFinished}},
           {kPausedByActor, {kActing, kReflecting, kCancelled, kFinished}},
           {kPausedByUser, {kActing, kReflecting, kCancelled, kFinished}},
           {kCancelled, {}},
           {kFinished, {}}}));
  if (state != state_) {
    DCHECK_STATE_TRANSITION(allowed_transitions,
                            /*old_state=*/state_,
                            /*new_state=*/state);
  }
#endif  // DCHECK_IS_ON()

  if ((state_ == kPausedByActor || state_ == kPausedByUser) &&
      state != kCancelled && state != kFinished) {
    current_timer_.emplace();
  }

  ui_event_dispatcher_->OnActorTaskSyncChange(
      ui::UiEventDispatcher::ChangeTaskState{
          .task_id = id_, .old_state = state_, .new_state = state});
  state_ = state;
  actor::ActorKeyedService::Get(profile_)->NotifyTaskStateChanged(*this);

  if (state_ == kPausedByActor || state_ == kPausedByUser ||
      state_ == kFinished || state_ == kCancelled) {
    // If new state is to be paused or done, add the current time.
    if (current_timer_) {
      total_active_time_ += current_timer_->Elapsed();
    }
    current_timer_ = std::nullopt;
  }

  // If the state is to be finished/cancelled record a histogram.
  if (state_ == kFinished) {
    base::UmaHistogramCounts1000("Actor.Task.Count.Completed",
                                 number_of_steps_);
    base::UmaHistogramLongTimes100("Actor.Task.Duration.Completed",
                                   total_active_time_);
  } else if (state_ == kCancelled) {
    base::UmaHistogramCounts1000("Actor.Task.Count.Cancelled",
                                 number_of_steps_);
    base::UmaHistogramLongTimes100("Actor.Task.Duration.Cancelled",
                                   total_active_time_);
  }
}

void ActorTask::Act(std::vector<std::unique_ptr<ToolRequest>>&& actions,
                    ActCallback callback) {
  if (state_ == State::kPausedByActor) {
    std::move(callback).Run(MakeResult(mojom::ActionResultCode::kTaskPaused),
                            std::nullopt, {});
    return;
  }
  if (IsStopped()) {
    std::move(callback).Run(MakeResult(mojom::ActionResultCode::kTaskWentAway),
                            std::nullopt, {});
    return;
  }
  SetState(State::kActing);
  number_of_steps_ += actions.size();
  execution_engine_->Act(
      std::move(actions),
      base::BindOnce(&ActorTask::OnFinishedAct, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void ActorTask::OnFinishedAct(
    ActCallback callback,
    mojom::ActionResultPtr result,
    std::optional<size_t> index_of_failed_action,
    std::vector<ActionResultWithLatencyInfo> action_results) {
  if (state_ != State::kActing) {
    std::move(callback).Run(MakeErrorResult(), std::nullopt, {});
    return;
  }
  SetState(State::kReflecting);
  std::move(callback).Run(std::move(result), index_of_failed_action,
                          std::move(action_results));
}

void ActorTask::Stop(bool success) {
  if (execution_engine_) {
    execution_engine_->CancelOngoingActions(
        mojom::ActionResultCode::kTaskWentAway);
  }
  end_time_ = base::Time::Now();
  // Remove all the tabs from the task.
  while (!acting_tabs_.empty()) {
    RemoveTab(acting_tabs_.begin()->first);
  }
  if (success) {
    SetState(State::kFinished);
  } else {
    SetState(State::kCancelled);
  }
}

void ActorTask::Pause(bool from_actor) {
  if (GetState() == State::kFinished) {
    return;
  }
  if (execution_engine_) {
    execution_engine_->CancelOngoingActions(
        mojom::ActionResultCode::kTaskPaused);
  }
  if (from_actor) {
    SetState(State::kPausedByActor);
  } else {
    SetState(State::kPausedByUser);
  }

  // Release all the capturer count increments. The ScopedClosureRunner's
  // destructor will handle this as `actuation_runner` is reset.
  for (auto& [handle, state] : acting_tabs_) {
    state.actuation_runner = {};
  }
}

void ActorTask::Resume() {
  // Only resume from a paused state.
  if (!IsPaused()) {
    return;
  }

  // Re-create the capturer count runners for all tabs that need one.
  for (auto& [handle, state] : acting_tabs_) {
    if (!handle.Get()) {
      continue;
    }
    if (content::WebContents* web_contents = handle.Get()->GetContents()) {
      state.actuation_runner =
          web_contents->IncrementCapturerCount(gfx::Size(),
                                               /*stay_hidden=*/false,
                                               /*stay_awake=*/true,
                                               /*is_activity=*/true);
    }
  }

  SetState(State::kReflecting);
}

bool ActorTask::IsPaused() const {
  return (GetState() == State::kPausedByActor) ||
         (GetState() == State::kPausedByUser);
}

bool ActorTask::IsStopped() const {
  return (GetState() == State::kFinished) || (GetState() == State::kCancelled);
}

base::Time ActorTask::GetEndTime() const {
  return end_time_;
}

void ActorTask::AddTab(tabs::TabHandle tab_handle, AddTabCallback callback) {
  if (IsPaused() || IsStopped()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            MakeResult(IsPaused() ? mojom::ActionResultCode::kTaskPaused
                                  : mojom::ActionResultCode::kTaskWentAway)));
    return;
  }
  // Make this tab the most recently actuated on, even if it was actuated on
  // before.
  last_actuated_tab_handle_ = tab_handle;
  if (acting_tabs_.contains(tab_handle)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), MakeOkResult()));
    return;
  }

  ActingTabState state;
  tabs::TabInterface* tab = tab_handle.Get();
  // GetContents may be null in unit tests.
  if (tab && tab->GetContents()) {
    content::WebContents* web_contents = tab->GetContents();
    state.actuation_runner =
        web_contents->IncrementCapturerCount(gfx::Size(),
                                             /*stay_hidden=*/false,
                                             /*stay_awake=*/true,
                                             /*is_activity=*/true);

    state.will_detach_subscription =
        tab->RegisterWillDetach(base::BindRepeating(
            &ActorTask::OnTabWillDetach, weak_ptr_factory_.GetWeakPtr()));
  }

  // Notify the UI of the new tab.
  acting_tabs_.emplace(tab_handle, std::move(state));
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ui::UiEventDispatcher::OnActorTaskAsyncChange,
                                ui_weak_ptr_factory_.GetWeakPtr(),
                                ui::UiEventDispatcher::AddTab{
                                    .task_id = id_, .handle = tab_handle},
                                std::move(callback)));
}

void ActorTask::RemoveTab(tabs::TabHandle tab_handle) {
  // Reset the last actuated tab if it is being removed.
  if (tab_handle == last_actuated_tab_handle_) {
    last_actuated_tab_handle_ = tabs::TabHandle::Null();
  }
  // Erasing the entry from the map triggers the ScopedClosureRunner's
  // destructor (via std::optional's destructor), which automatically calls
  // DecrementCapturerCount on the WebContents.
  auto num_removed = acting_tabs_.erase(tab_handle);

  if (num_removed > 0) {
    // Notify the UI of the tab removal.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ui::UiEventDispatcher::OnActorTaskSyncChange,
                                  ui_weak_ptr_factory_.GetWeakPtr(),
                                  ui::UiEventDispatcher::RemoveTab{
                                      .task_id = id_, .handle = tab_handle}));
  }
}

void ActorTask::OnTabWillDetach(tabs::TabInterface* tab,
                                tabs::TabInterface::DetachReason reason) {
  if (reason != tabs::TabInterface::DetachReason::kDelete) {
    return;
  }
  if (!IsActingOnTab(tab->GetHandle())) {
    return;
  }

  // TODO(mcnee): This will also stop a task that's paused. Should we leave
  // paused tasks as is?

  actor::ActorKeyedService::Get(profile_)->StopTask(id(), /*success=*/false);
}

bool ActorTask::IsActingOnTab(tabs::TabHandle tab) const {
  return acting_tabs_.contains(tab);
}

tabs::TabInterface* ActorTask::GetTabForObservation() const {
  DCHECK_GT(acting_tabs_.size(), 0ul);
  DCHECK_LT(acting_tabs_.size(), 2ul);
  for (const auto& [handle, state] : acting_tabs_) {
    if (tabs::TabInterface* tab = handle.Get()) {
      return tab;
    }
  }

  return nullptr;
}

absl::flat_hash_set<tabs::TabHandle> ActorTask::GetLastActedTabs() const {
  // TODO(bokan): Currently the client only acts on a single tab but this
  // should track which tabs were acted on in the last call to Act.
  return GetTabs();
}

tabs::TabHandle ActorTask::GetLastActedTab() {
  // TODO(crbug.com/441064175): Use GetLastActedTabs() or update implementation
  // for multi-tab actuation.
  return last_actuated_tab_handle_;
}

absl::flat_hash_set<tabs::TabHandle> ActorTask::GetTabs() const {
  absl::flat_hash_set<tabs::TabHandle> handles;
  for (const auto& [handle, state] : acting_tabs_) {
    handles.insert(handle);
  }
  return handles;
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
    case kPausedByActor:
      return "PausedByActor";
    case kPausedByUser:
      return "PausedByUser";
    case kCancelled:
      return "Cancelled";
    case kFinished:
      return "Finished";
  }
}

std::ostream& operator<<(std::ostream& os, const ActorTask::State& state) {
  return os << ToString(state);
}

}  // namespace actor
