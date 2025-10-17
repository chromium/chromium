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
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor.mojom-data-view.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/action_result.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/size.h"

namespace actor {

namespace {

bool IsStateActive(ActorTask::State state) {
  return (state == ActorTask::State::kCreated ||
          state == ActorTask::State::kActing ||
          state == ActorTask::State::kReflecting);
}

void SetFocusState(content::WebContents* contents,
                   std::optional<bool> focus_state) {
  if (content::RenderWidgetHostView* view =
          contents->GetRenderWidgetHostView()) {
    if (content::RenderWidgetHost* host = view->GetRenderWidgetHost()) {
      // If a new state was provided, use that. Otherwise us the state from
      // the view.
      bool new_state = focus_state.value_or(view->HasFocus());
      if (new_state) {
        host->Focus();
      } else {
        host->Blur();
      }
    }
  }
}

}  // namespace

ActorTask::ActingTabState::ActingTabState(ActorTask* task) : task(task) {}
ActorTask::ActingTabState::~ActingTabState() = default;

void ActorTask::ActingTabState::SetContents(content::WebContents* contents) {
  Observe(contents);
}

void ActorTask::ActingTabState::PrimaryPageChanged(content::Page& page) {
  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(&page.GetMainDocument());
  if (task->IsActive()) {
    task->DidContentsBecomeActive(this, contents);
  } else {
    task->DidContentsBecomeInactive(this, contents);
  }
}

ActorTask::ActorTask(Profile* profile,
                     std::unique_ptr<ExecutionEngine> execution_engine,
                     std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher,
                     webui::mojom::TaskOptionsPtr options,
                     base::WeakPtr<ActorTaskDelegate> delegate)
    : profile_(profile),
      execution_engine_(std::move(execution_engine)),
      ui_event_dispatcher_(std::move(ui_event_dispatcher)),
      title_(options && options->title.has_value() ? options->title.value()
                                                   : ""),
      delegate_(std::move(delegate)),
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

void ActorTask::SetState(State new_state) {
  using enum State;
  VLOG(1) << "ActorTask state change: " << state_ << " -> " << new_state;
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
           {kPausedByActor, {kReflecting, kCancelled, kFinished}},
           {kPausedByUser, {kReflecting, kCancelled, kFinished}},
           {kCancelled, {}},
           {kFinished, {}}}));
  if (new_state != state_) {
    DCHECK_STATE_TRANSITION(allowed_transitions,
                            /*old_state=*/state_,
                            /*new_state=*/new_state);
  }
#endif  // DCHECK_IS_ON()

  State old_state = state_;
  const base::TimeDelta old_state_duration = current_state_timer_.Elapsed();

  // If the old state was active, add its duration to the total active time for
  // the task.
  if (IsActive()) {
    total_active_time_ += old_state_duration;
  }

  // Record granular state transition histograms.
  RecordActorTaskStateTransitionDuration(old_state_duration, old_state);
  RecordActorTaskStateTransitionActionCount(actions_in_current_state_,
                                            old_state, new_state);

  state_ = new_state;
  current_state_timer_ = base::ElapsedTimer();
  actions_in_current_state_ = 0;
  if (IsStateActive(new_state) && !IsStateActive(old_state)) {
    for (const auto& [tab, _] : acting_tabs_) {
      DidTabBecomeActive(tab);
    }
  } else if (!IsStateActive(new_state) && IsStateActive(old_state)) {
    for (const auto& [tab, _] : acting_tabs_) {
      DidTabBecomeInactive(tab);
    }
  }
  ui_event_dispatcher_->OnActorTaskSyncChange(
      ui::UiEventDispatcher::ChangeTaskState{
          .task_id = id_, .old_state = old_state, .new_state = new_state});

  actor::ActorKeyedService::Get(profile_)->NotifyTaskStateChanged(*this);

  // If the state is to be finished/cancelled record a histogram.
  if (state_ == kFinished) {
    base::UmaHistogramCounts1000("Actor.Task.Count.Completed",
                                 total_number_of_actions_);
    base::UmaHistogramLongTimes100("Actor.Task.Duration.Completed",
                                   total_active_time_);
  } else if (state_ == kCancelled) {
    base::UmaHistogramCounts1000("Actor.Task.Count.Cancelled",
                                 total_number_of_actions_);
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

  actions_in_current_state_ += actions.size();
  total_number_of_actions_ += actions.size();

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
}

void ActorTask::Resume() {
  // Only resume from a paused state.
  if (!IsPaused()) {
    return;
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

bool ActorTask::IsActive() const {
  return IsStateActive(state_);
}

base::Time ActorTask::GetEndTime() const {
  return end_time_;
}

void ActorTask::AddTab(tabs::TabHandle tab_handle, AddTabCallback callback) {
  if (!IsActive()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            MakeResult(IsPaused() ? mojom::ActionResultCode::kTaskPaused
                                  : mojom::ActionResultCode::kTaskWentAway)));
    return;
  }
  if (acting_tabs_.contains(tab_handle)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), MakeOkResult()));
    return;
  }

  acting_tabs_.emplace(tab_handle, std::make_unique<ActingTabState>(this));
  DidTabBecomeActive(tab_handle);

  // Notify the UI of the new tab.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ui::UiEventDispatcher::OnActorTaskAsyncChange,
                                ui_weak_ptr_factory_.GetWeakPtr(),
                                ui::UiEventDispatcher::AddTab{
                                    .task_id = id_, .handle = tab_handle},
                                std::move(callback)));

  // Post-task this delegate call to avoid any performance issues.
  if (delegate_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ActorTaskDelegate::OnTabAddedToTask,
                                  delegate_, id_, tab_handle));
  }
}

// TODO(crbug.com/450524344): Add a test for this. Note that at this point the
// tab is not yet associated with the new_contents.
void ActorTask::HandleDiscardContents(tabs::TabInterface* tab,
                                      content::WebContents* old_contents,
                                      content::WebContents* new_contents) {
  CHECK(acting_tabs_.contains(tab->GetHandle()));
  if (!IsActive()) {
    // The observer should only be attached when we're active.
    NOTREACHED(base::NotFatalUntil::M145);
    return;
  }
  ActingTabState* state = acting_tabs_[tab->GetHandle()].get();
  DidContentsBecomeActive(state, new_contents);
}

void ActorTask::RemoveTab(tabs::TabHandle tab_handle) {
  if (IsActingOnTab(tab_handle)) {
    DidTabBecomeInactive(tab_handle);
  }
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
  if (!HasTab(tab->GetHandle())) {
    return;
  }

  // TODO(mcnee): This will also stop a task that's paused. Should we leave
  // paused tasks as is?

  actor::ActorKeyedService::Get(profile_)->StopTask(id(), /*success=*/false);
}

bool ActorTask::HasTab(tabs::TabHandle tab) const {
  return acting_tabs_.contains(tab);
}

bool ActorTask::IsActingOnTab(tabs::TabHandle tab) const {
  if (!IsActive()) {
    return false;
  }

  return HasTab(tab);
}

absl::flat_hash_set<tabs::TabHandle> ActorTask::GetLastActedTabs() const {
  // TODO(crbug.com/420669167): Currently the client only acts on a single tab
  // so we can return the full set but with multi-tab this will need to be
  // smarter about which tabs are relevant to the last/current action.
  return GetTabs();
}

absl::flat_hash_set<tabs::TabHandle> ActorTask::GetTabs() const {
  absl::flat_hash_set<tabs::TabHandle> handles;
  for (const auto& [handle, _] : acting_tabs_) {
    handles.insert(handle);
  }
  return handles;
}

void ActorTask::DidTabBecomeActive(tabs::TabHandle handle) {
  DCHECK(IsActingOnTab(handle));
  tabs::TabInterface* tab = handle.Get();
  if (!tab) {
    // This happens in unitttests.
    return;
  }
  ActingTabState* state = acting_tabs_[handle].get();
  content::WebContents* contents = tab->GetContents();
  if (!contents) {
    return;
  }

  state->will_detach_subscription = tab->RegisterWillDetach(base::BindRepeating(
      &ActorTask::OnTabWillDetach, weak_ptr_factory_.GetWeakPtr()));
  // TODO(crbug.com/450524344)): Add a test for discarded content.
  state->content_discarded_subscription =
      tab->RegisterWillDiscardContents(base::BindRepeating(
          &ActorTask::HandleDiscardContents, weak_ptr_factory_.GetWeakPtr()));
  DidContentsBecomeActive(state, contents);
}

void ActorTask::DidContentsBecomeActive(ActorTask::ActingTabState* state,
                                        content::WebContents* contents) {
  SetFocusState(contents, true);
  state->SetContents(contents);
  state->actuation_runner =
      contents->IncrementCapturerCount(gfx::Size(),
                                       /*stay_hidden=*/false,
                                       /*stay_awake=*/true,
                                       /*is_activity=*/true);
}

void ActorTask::DidTabBecomeInactive(tabs::TabHandle handle) {
  // Note that the state_ may be kActive if we are just removing this tab.
  DCHECK(acting_tabs_.contains(handle));
  tabs::TabInterface* tab = handle.Get();
  if (!tab) {
    // This happens in unitttests.
    return;
  }
  ActingTabState* state = acting_tabs_[handle].get();
  content::WebContents* contents = tab->GetContents();
  if (!contents) {
    return;
  }

  // Reset focus and remove observers.
  SetFocusState(contents, std::nullopt);
  state->will_detach_subscription = {};
  state->SetContents(nullptr);
  state->content_discarded_subscription = {};
  DidContentsBecomeInactive(state, contents);
}

void ActorTask::DidContentsBecomeInactive(ActorTask::ActingTabState* state,
                                          content::WebContents* contents) {
  SetFocusState(contents, std::nullopt);
  state->SetContents(nullptr);
  // Triggers the ScopedClosureRunner's destructor (via std::optional's
  // destructor), which automatically calls DecrementCapturerCount on the
  // WebContents.
  state->actuation_runner = {};
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
