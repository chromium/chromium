// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_task.h"

#include <memory>
#include <optional>
#include <ostream>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/state_transitions.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/actor/action_tracker_for_metrics.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_metrics.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor.mojom-data-view.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "chrome/common/chrome_features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/buildflags.h"
#include "ui/gfx/geometry/size.h"

namespace actor {

namespace {

bool IsStateActorControlledAndNotWaiting(ActorTask::State state) {
  return (state == ActorTask::State::kCreated ||
          state == ActorTask::State::kActing ||
          state == ActorTask::State::kReflecting);
}

bool IsStateActorControlled(ActorTask::State state) {
  return IsStateActorControlledAndNotWaiting(state) ||
         // Although waiting on the user, this state is used when the user can
         // only interact with the client. i.e. The user cannot interact with
         // the task's tabs in this state so it is still considered to be under
         // the client's control.
         state == ActorTask::State::kWaitingOnUser;
}

bool IsInterruptedState(ActorTask::State state) {
  return (state == ActorTask::State::kWaitingOnUser ||
          state == ActorTask::State::kPausedByActor ||
          state == ActorTask::State::kPausedByUser);
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

ActorTask::ActorControlledTabState::ActorControlledTabState(ActorTask* task)
    : task(task) {}
ActorTask::ActorControlledTabState::~ActorControlledTabState() {
  // Stop observing the Webcontents immediately to prevent reentrant calls to
  // OnVisibilityChanged() when other members (e.g. `actuation_runner`) are
  // destroyed.
  Observe(nullptr);
}

void ActorTask::ActorControlledTabState::SetContents(
    content::WebContents* contents) {
  Observe(contents);
}

void ActorTask::ActorControlledTabState::PrimaryPageChanged(
    content::Page& page) {
  content::WebContents* contents =
      content::WebContents::FromRenderFrameHost(&page.GetMainDocument());
  if (task->IsUnderActorControl()) {
    task->DidContentsEnterActorControl(this, contents);
  } else {
    task->DidContentsExitActorControl(this, contents);
  }
}

void ActorTask::ActorControlledTabState::OnVisibilityChanged(
    content::Visibility visibility) {
  if (!task->IsUnderActorControl()) {
    return;
  }
  task->UpdateVisibilityTimes();
  task->RecomputeHasVisibleTab();
}

ActorTask::ActorTask(Profile* profile,
                     std::unique_ptr<ExecutionEngine> execution_engine,
                     std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher,
                     webui::mojom::TaskOptionsPtr options,
                     base::WeakPtr<ActorTaskDelegate> delegate)
    : profile_(profile),
      create_time_(base::TimeTicks::Now()),
      action_tracker_for_metrics_(std::make_unique<ActionTrackerForMetrics>()),
      execution_engine_(std::move(execution_engine)),
      ui_event_dispatcher_(std::move(ui_event_dispatcher)),
      journal_(ActorKeyedService::Get(profile)->GetJournal().GetSafeRef()),
      title_(options && options->title.has_value() ? options->title.value()
                                                   : ""),
      delegate_(std::move(delegate)),
      ui_weak_ptr_factory_(ui_event_dispatcher_.get()) {}

ActorTask::~ActorTask() {
  // The owner of the ActorTasks (ActorKeyedService) should have stopped all
  // tasks already.
  CHECK(IsCompleted());
}

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

base::WeakPtr<ActorTask> ActorTask::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void ActorTask::SetState(State new_state) {
  using enum State;
  journal_->Log(GURL(), id(), "ActorTask::SetState",
                JournalDetailsBuilder()
                    .Add("current_state", ToString(state_))
                    .Add("new_state", ToString(new_state))
                    .Build());
#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<State>>
      allowed_transitions(base::StateTransitions<State>(
          {{kCreated,
            {kActing, kReflecting, kPausedByActor, kPausedByUser, kCancelled,
             kFinished, kFailed}},
           {kActing,
            {kReflecting, kPausedByActor, kPausedByUser, kCancelled, kFinished,
             kWaitingOnUser, kFailed}},
           {kReflecting,
            {kActing, kPausedByActor, kPausedByUser, kCancelled, kFinished,
             kWaitingOnUser, kFailed}},
           {kPausedByActor, {kReflecting, kCancelled, kFinished, kFailed}},
           {kPausedByUser, {kReflecting, kCancelled, kFinished, kFailed}},
           {kWaitingOnUser,
            {kActing, kReflecting, kPausedByActor, kPausedByUser, kCancelled,
             kFinished, kFailed}},
           {kCancelled, {}},
           {kFailed, {}},
           {kFinished, {}}}));
  if (new_state != state_) {
    DCHECK_STATE_TRANSITION(allowed_transitions,
                            /*old_state=*/state_,
                            /*new_state=*/new_state);
  }
#endif  // DCHECK_IS_ON()

  // Actor and user control states must be mutually exclusive.
  CHECK(IsCompleted() || IsUnderActorControl() != IsUnderUserControl());

  action_tracker_for_metrics_->WillMoveToState(new_state);

  State old_state = state_;
  const base::TimeDelta old_state_duration = current_state_timer_.Elapsed();

  // If the old state was active, add its duration to the total active time for
  // the task.
  if (IsStateActorControlledAndNotWaiting(old_state)) {
    total_actor_controlled_active_time_ += old_state_duration;
  }

  // Record granular state transition histograms.
  RecordActorTaskStateTransitionDuration(old_state_duration, old_state);
  RecordActorTaskStateTransitionActionCount(actions_in_current_state_,
                                            old_state, new_state);

  state_ = new_state;
  current_state_timer_ = base::ElapsedTimer();
  actions_in_current_state_ = 0;
  // When transitioning into an actor-controlled state, start the timer for
  // visibility metrics and prepare each tab for actuation.
  if (IsStateActorControlled(new_state) && !IsStateActorControlled(old_state)) {
    visibility_timer_ = base::ElapsedTimer();
    for (const auto& [tab, _] : controlled_tabs_) {
      DidTabEnterActorControl(tab);
    }
    // When transitioning out of an actor-controlled state, record the final
    // visibility duration and clean up each tab.
  } else if (!IsStateActorControlled(new_state) &&
             IsStateActorControlled(old_state)) {
    UpdateVisibilityTimes();
    for (const auto& [tab, _] : controlled_tabs_) {
      DidTabExitActorControl(tab);
    }
    ResetToObserveTabsSet();
  }
  if (IsInterruptedState(new_state)) {
    ++total_number_of_interruptions_;
  }
  ui_event_dispatcher_->OnActorTaskSyncChange(
      ui::UiEventDispatcher::ChangeTaskState{.task_id = id_,
                                             .old_state = old_state,
                                             .new_state = new_state,
                                             .title = title_});

  if (base::FeatureList::IsEnabled(
          actor::kGlicPerformActionsReturnsBeforeStateChange)) {
    // The callback_for_act_ is posted before calling SetState. We want that to
    // invoke before the client sees the state change so post that as well.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ActorKeyedService::NotifyTaskStateChanged,
                       actor::ActorKeyedService::Get(profile_)->GetWeakPtr(),
                       id_, state_));
  } else {
    actor::ActorKeyedService::Get(profile_)->NotifyTaskStateChanged(id_,
                                                                    state_);
  }

  // If the state is to be finished/cancelled record a histogram.
  if (state_ == kFinished || state_ == kCancelled || state_ == kFailed) {
    CHECK(stopped_reason_.has_value());
    RecordActorTaskVisibilityDurationHistograms(
        total_time_visible_, total_time_not_visible_, stopped_reason_.value());
    RecordActorTaskCompletion(
        stopped_reason_.value(), base::TimeTicks::Now() - create_time_,
        total_actor_controlled_active_time_, total_number_of_interruptions_,
        total_number_of_actions_);
  }
}

void ActorTask::Act(std::vector<std::unique_ptr<ToolRequest>>&& actions,
                    ActCallback callback) {
  if (IsUnderUserControl()) {
    journal_->Log(GURL(), id(), "ActorTask::Act",
                  JournalDetailsBuilder().AddError("Task is paused").Build());
    std::move(callback).Run(MakeResult(mojom::ActionResultCode::kTaskPaused),
                            std::nullopt, {});
    return;
  }
  if (IsCompleted()) {
    journal_->Log(GURL(), id(), "ActorTask::Act",
                  JournalDetailsBuilder().AddError("Task is Stopped").Build());
    std::move(callback).Run(MakeResult(mojom::ActionResultCode::kTaskWentAway),
                            std::nullopt, {});
    return;
  }

  ResetToObserveTabsSet();

  SetState(State::kActing);

  actions_in_current_state_ += actions.size();
  total_number_of_actions_ += actions.size();

  action_tracker_for_metrics_->WillAct(actions);
  callback_for_act_ = std::move(callback);

  execution_engine_->Act(std::move(actions),
                         base::BindOnce(&ActorTask::OnFinishedAct,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void ActorTask::OnFinishedAct(
    mojom::ActionResultPtr result,
    std::optional<size_t> index_of_failed_action,
    std::vector<ActionResultWithLatencyInfo> action_results) {
  if (state_ != State::kActing) {
    // Note: this likely isn't a problem when it happens - e.g. the task was
    // paused while the act was in progress but we note it here for debugging
    // purposes.
    journal_->Log(GURL(), id(), "ActorTask::OnFinishedAct",
                  JournalDetailsBuilder()
                      .Add("result", ToDebugString(*result))
                      .Add("Not in kActing state", base::ToString(state_))
                      .Build());
  }

  // The callback may already have been called, if the task was stopped or
  // paused.
  if (callback_for_act_) {
    // Interruption (WaitingOnUser) can happen while acting, but in that case
    // the tool is the source and must not finish before uninterrupting.
    DCHECK_EQ(state_, State::kActing);
    action_tracker_for_metrics_->OnFinishedAct(*result);
    std::move(callback_for_act_)
        .Run(std::move(result), index_of_failed_action,
             std::move(action_results));
  }

  if (state_ == State::kActing) {
    SetState(State::kReflecting);
  }
}

void ActorTask::Stop(StoppedReason stop_reason) {
  // Invoke the callback before changing states so that the client sees the Act
  // result before seeing the state transition.
  if (callback_for_act_) {
    DCHECK(state_ == State::kActing || state_ == State::kWaitingOnUser);
    mojom::ActionResultPtr result =
        MakeResult(mojom::ActionResultCode::kTaskWentAway);
    action_tracker_for_metrics_->OnFinishedAct(*result);
    std::move(callback_for_act_)
        .Run(std::move(result), /*index_of_failed_action=*/std::nullopt,
             /*action_results=*/{});
  }

  if (execution_engine_) {
    execution_engine_->CancelOngoingActions(
        mojom::ActionResultCode::kTaskWentAway);
    execution_engine_->RunUserTakeoverCallbackIfExists(/*should_cancel=*/true);
  }
  end_time_ = base::Time::Now();
  // Remove all the tabs from the task.
  while (!controlled_tabs_.empty()) {
    RemoveTab(controlled_tabs_.begin()->first);
  }
  State final_state;
  switch (stop_reason) {
    case StoppedReason::kUserStartedNewChat:
    case StoppedReason::kUserLoadedPreviousChat:
    case StoppedReason::kStoppedByUser:
    case StoppedReason::kTabDetached:
    case StoppedReason::kShutdown:
      final_state = State::kCancelled;
      break;
    case StoppedReason::kTaskComplete:
      final_state = State::kFinished;
      break;
    case StoppedReason::kModelError:
    case StoppedReason::kChromeFailure:
      final_state = State::kFailed;
      break;
  }
  stopped_reason_ = stop_reason;
  SetState(final_state);
}

void ActorTask::Pause(bool from_actor) {
  if (IsCompleted()) {
    return;
  }

  // Invoke the callback before changing states so that the client sees the Act
  // result before seeing the state transition.
  if (callback_for_act_) {
    DCHECK(state_ == State::kActing || state_ == State::kWaitingOnUser);
    mojom::ActionResultPtr result =
        MakeResult(mojom::ActionResultCode::kTaskPaused);
    action_tracker_for_metrics_->OnFinishedAct(*result);
    std::move(callback_for_act_)
        .Run(MakeResult(mojom::ActionResultCode::kTaskPaused),
             /*index_of_failed_action=*/std::nullopt, /*action_results=*/{});
  }

  if (execution_engine_) {
    execution_engine_->CancelOngoingActions(
        mojom::ActionResultCode::kTaskPaused);
    execution_engine_->RunUserTakeoverCallbackIfExists(/*should_cancel=*/false);
  }
  if (from_actor) {
    SetState(State::kPausedByActor);
  } else {
    SetState(State::kPausedByUser);
  }
}

void ActorTask::Resume() {
  // Only resume from a paused state.
  if (!IsUnderUserControl()) {
    return;
  }

  SetState(State::kReflecting);
}

void ActorTask::Interrupt() {
  if (GetState() != State::kReflecting && GetState() != State::kActing) {
    return;
  }
  SetState(State::kWaitingOnUser);
}

void ActorTask::Uninterrupt(State resumed_state) {
  if (GetState() != State::kWaitingOnUser) {
    return;
  }
  SetState(resumed_state);
}

bool ActorTask::IsUnderUserControl() const {
  return GetState() == State::kPausedByActor ||
         GetState() == State::kPausedByUser;
}

bool ActorTask::IsUnderActorControl() const {
  return IsStateActorControlled(state_);
}

bool ActorTask::IsCompleted() const {
  return IsCompletedState(GetState());
}

// static
bool ActorTask::IsCompletedState(State state) {
  return (state == State::kFinished) || (state == State::kCancelled) ||
         (state == State::kFailed);
}

base::Time ActorTask::GetEndTime() const {
  return end_time_;
}

void ActorTask::AddTab(tabs::TabHandle tab_handle, AddTabCallback callback) {
  if (!IsUnderActorControl()) {
    journal_->Log(
        GURL(), id(), "ActorTask::AddTab",
        JournalDetailsBuilder().AddError("Not Under Actor Control").Build());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            std::move(callback),
            MakeResult(IsUnderUserControl()
                           ? mojom::ActionResultCode::kTaskPaused
                           : mojom::ActionResultCode::kTaskWentAway)));
    return;
  }
  if (controlled_tabs_.contains(tab_handle)) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), MakeOkResult()));
    return;
  }

  journal_->Log(
      GURL(), id(), "ActorTask::AddTab",
      JournalDetailsBuilder().Add("tab_id", tab_handle.raw_value()).Build());

  auto emplace_result = controlled_tabs_.emplace(
      tab_handle, std::make_unique<ActorControlledTabState>(this));
  if (tabs::TabInterface* tab = tab_handle.Get()) {
    emplace_result.first->second->will_detach_subscription =
        tab->RegisterWillDetach(base::BindRepeating(
            &ActorTask::OnTabWillDetach, weak_ptr_factory_.GetWeakPtr()));
  }
  DidTabEnterActorControl(tab_handle);

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
  CHECK(controlled_tabs_.contains(tab->GetHandle()));
  if (!IsUnderActorControl()) {
    // The observer should only be attached when we're under actor control.
    NOTREACHED(base::NotFatalUntil::M145);
    return;
  }
  ActorControlledTabState* state = controlled_tabs_[tab->GetHandle()].get();
  DidContentsEnterActorControl(state, new_contents);
}

void ActorTask::RemoveTab(tabs::TabHandle tab_handle) {
  if (IsActingOnTab(tab_handle)) {
    // Record the tab visibility duration.
    UpdateVisibilityTimes();
    DidTabExitActorControl(tab_handle);
  }
  auto num_removed = controlled_tabs_.erase(tab_handle);
  RecomputeHasVisibleTab();

  if (num_removed > 0) {
    journal_->Log(
        GURL(), id(), "ActorTask::RemoveTab",
        JournalDetailsBuilder().Add("tab_id", tab_handle.raw_value()).Build());

    // Notify the UI of the tab removal.
    // We call this synchronously since a Stop will destroy the ActorTask
    // in the same event pump and the UIEventDispatcher will be destroyed
    // before dispatching the event.
    ui_event_dispatcher_->OnActorTaskSyncChange(
        ui::UiEventDispatcher::RemoveTab{.task_id = id_, .handle = tab_handle});
  }
}

void ActorTask::ObserveTabOnce(tabs::TabHandle tab_handle) {
  CHECK(IsUnderActorControl());

  if (to_observe_tabs_.contains(tab_handle) ||
      controlled_tabs_.contains(tab_handle)) {
    return;
  }

  tabs::TabInterface* tab = tab_handle.Get();
  if (!tab) {
    journal_->Log(GURL(), id(), "ObserveTabOnce",
                  JournalDetailsBuilder()
                      .Add("tab_id", tab_handle.raw_value())
                      .AddError("Tab is gone")
                      .Build());
    return;
  }

  journal_->Log(
      GURL(), id(), "ObserveTabOnce",
      JournalDetailsBuilder().Add("tab_id", tab_handle.raw_value()).Build());

  auto itr =
      to_observe_tabs_
          .emplace(tab_handle, std::make_unique<ActorControlledTabState>(this))
          .first;
  ActorControlledTabState* state = itr->second.get();

  state->will_detach_subscription = tab->RegisterWillDetach(base::BindRepeating(
      &ActorTask::OnTabWillDetach, weak_ptr_factory_.GetWeakPtr()));
  DidContentsEnterActorControl(state, tab->GetContents());
}

void ActorTask::OnTabWillDetach(tabs::TabInterface* tab,
                                tabs::TabInterface::DetachReason reason) {
  if (reason != tabs::TabInterface::DetachReason::kDelete) {
    return;
  }
  if (to_observe_tabs_.contains(tab->GetHandle())) {
    // If the removed tab is only being observed, we can just remove it without
    // disrupting the task. If the task hasn't gotten the observation it wanted
    // for this tab, then won't be able to get it and will need to do something
    // else.
    to_observe_tabs_.erase(tab->GetHandle());
  }
  if (!HasTab(tab->GetHandle())) {
    return;
  }

  // TODO(mcnee): This will also stop a task that's paused. Should we leave
  // paused tasks as is?

  journal_->Log(GURL(), id(), "Acting Tab Deleted",
                JournalDetailsBuilder()
                    .Add("tab_id", tab->GetHandle().raw_value())
                    .Build());

  actor::ActorKeyedService::Get(profile_)->StopTask(
      id(), StoppedReason::kTabDetached);
}

void ActorTask::UpdateVisibilityTimes() {
  if (has_visible_tab_) {
    total_time_visible_ += visibility_timer_.Elapsed();
  } else {
    total_time_not_visible_ += visibility_timer_.Elapsed();
  }
  visibility_timer_ = base::ElapsedTimer();
}

void ActorTask::RecomputeHasVisibleTab() {
  bool has_any_visible_tab = false;
  for (const auto& [handle, controlled_state] : controlled_tabs_) {
    if (controlled_state->web_contents() &&
        controlled_state->web_contents()->GetVisibility() ==
            content::Visibility::VISIBLE) {
      has_any_visible_tab = true;
      break;
    }
  }

  has_visible_tab_ = has_any_visible_tab;
}

void ActorTask::ResetToObserveTabsSet() {
  for (const auto& [tab_handle, state] : to_observe_tabs_) {
    tabs::TabInterface* tab = tab_handle.Get();
    if (!tab) {
      continue;
    }

    DidContentsExitActorControl(state.get(), tab->GetContents());
  }
  to_observe_tabs_.clear();
}

bool ActorTask::HasTab(tabs::TabHandle tab) const {
  return controlled_tabs_.contains(tab);
}

bool ActorTask::IsActingOnTab(tabs::TabHandle tab) const {
  if (!IsUnderActorControl()) {
    return false;
  }

  return HasTab(tab);
}

absl::flat_hash_set<tabs::TabHandle> ActorTask::GetLastActedTabs() const {
  // TODO(crbug.com/420669167): Currently the client only acts on a single tab
  // so we can return the full set but with multi-tab this will need to be
  // smarter about which tabs are relevant to the last/current action.
  absl::flat_hash_set<tabs::TabHandle> last_acted_tabs = GetTabs();

  for (const auto& [handle, _] : to_observe_tabs_) {
    last_acted_tabs.insert(handle);
  }

  return last_acted_tabs;
}

absl::flat_hash_set<tabs::TabHandle> ActorTask::GetTabs() const {
  absl::flat_hash_set<tabs::TabHandle> handles;
  for (const auto& [handle, _] : controlled_tabs_) {
    handles.insert(handle);
  }
  return handles;
}

void ActorTask::DidTabEnterActorControl(tabs::TabHandle handle) {
  DCHECK(IsActingOnTab(handle));
  tabs::TabInterface* tab = handle.Get();
  if (!tab) {
    // This happens in unittests.
    return;
  }
  ActorControlledTabState* state = controlled_tabs_[handle].get();

  // TODO(b/454107412): This is assuming the tab isn't discarded but nothing
  // guarantees that.
  content::WebContents* contents = tab->GetContents();
  if (!contents) {
    return;
  }

  // TODO(crbug.com/450524344)): Add a test for discarded content.
  state->content_discarded_subscription =
      tab->RegisterWillDiscardContents(base::BindRepeating(
          &ActorTask::HandleDiscardContents, weak_ptr_factory_.GetWeakPtr()));
  DidContentsEnterActorControl(state, contents);

  RecomputeHasVisibleTab();
}

void ActorTask::DidContentsEnterActorControl(
    ActorTask::ActorControlledTabState* state,
    content::WebContents* contents) {
  SetFocusState(contents, true);
  state->SetContents(contents);
  state->actuation_runner =
      contents->IncrementCapturerCount(gfx::Size(),
                                       /*stay_hidden=*/false,
                                       /*stay_awake=*/true,
                                       /*is_activity=*/true);
#if BUILDFLAG(IS_MAC) && BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  if (base::FeatureList::IsEnabled(features::kGlicActorInternalPopups)) {
    state->reenable_external_popups = contents->ForbidExternalPopupMenus();
  }
#endif  // BUILDFLAG(IS_MAC) && BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
}

void ActorTask::DidTabExitActorControl(tabs::TabHandle handle) {
  // Note that the state_ may still be in an actor controlled state if we are
  // just removing this tab (e.g. close the tab).
  DCHECK(controlled_tabs_.contains(handle));
  tabs::TabInterface* tab = handle.Get();
  if (!tab) {
    // This happens in unittests.
    return;
  }
  ActorControlledTabState* state = controlled_tabs_[handle].get();
  content::WebContents* contents = tab->GetContents();
  if (!contents) {
    return;
  }

  // Reset focus and remove observers.
  SetFocusState(contents, std::nullopt);
  state->SetContents(nullptr);
  state->content_discarded_subscription = {};
  DidContentsExitActorControl(state, contents);
}

void ActorTask::DidContentsExitActorControl(
    ActorTask::ActorControlledTabState* state,
    content::WebContents* contents) {
  SetFocusState(contents, std::nullopt);
  state->SetContents(nullptr);
  // Triggers the ScopedClosureRunner's destructor (via std::optional's
  // destructor), which automatically calls DecrementCapturerCount on the
  // WebContents.
  state->actuation_runner = {};
#if BUILDFLAG(IS_MAC) && BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
  state->reenable_external_popups = {};
#endif  // BUILDFLAG(IS_MAC) && BUILDFLAG(USE_EXTERNAL_POPUP_MENU)
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
    case kWaitingOnUser:
      return "WaitingOnUser";
    case kFailed:
      return "Failed";
  }
}

std::ostream& operator<<(std::ostream& os, const ActorTask::State& state) {
  return os << ToString(state);
}

void ActorTask::SetExecutionEngineForTesting(
    std::unique_ptr<ExecutionEngine> engine) {
  execution_engine_.reset(std::move(engine.release()));
  execution_engine_->SetOwner(this);
}

}  // namespace actor
