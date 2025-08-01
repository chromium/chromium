// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/execution_engine.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/state_transitions.h"
#include "base/types/id_type.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/actor/tools/tool_controller.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "ui/event_dispatcher.h"
#include "url/origin.h"

using content::RenderFrameHost;
using content::WebContents;
using optimization_guide::DocumentIdentifierUserData;
using optimization_guide::proto::Action;
using optimization_guide::proto::Actions;
using optimization_guide::proto::ActionTarget;
using optimization_guide::proto::AnnotatedPageContent;
using optimization_guide::proto::BrowserAction;
using tabs::TabInterface;

namespace actor {

namespace {

void PostTaskForActCallback(ActorTask::ActCallback callback,
                            mojom::ActionResultPtr result,
                            std::optional<size_t> index_of_failed_action) {
  UMA_HISTOGRAM_ENUMERATION("Actor.ExecutionEngine.Action.ResultCode",
                            result->code);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result),
                                index_of_failed_action));
}

}  // namespace

ExecutionEngine::ExecutionEngine(Profile* profile)
    : profile_(profile),
      journal_(ActorKeyedService::Get(profile)->GetJournal().GetSafeRef()),
      ui_event_dispatcher_(ui::NewUiEventDispatcher(
          ActorKeyedService::Get(profile)->GetActorUiStateManager())) {
  CHECK(profile_);
  // Idempotent. Enables the action blocklist if it isn't already enabled.
  InitActionBlocklist(profile_.get());
}

ExecutionEngine::ExecutionEngine(
    Profile* profile,
    std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher)
    : profile_(profile),
      journal_(ActorKeyedService::Get(profile)->GetJournal().GetSafeRef()),
      ui_event_dispatcher_(std::move(ui_event_dispatcher)) {
  CHECK(profile_);
  // Idempotent. Enables the action blocklist if it isn't already enabled.
  InitActionBlocklist(profile_.get());
}

std::unique_ptr<ExecutionEngine> ExecutionEngine::CreateForTesting(
    Profile* profile,
    std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher) {
  return base::WrapUnique<ExecutionEngine>(
      new ExecutionEngine(profile, std::move(ui_event_dispatcher)));
}

ExecutionEngine::~ExecutionEngine() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ExecutionEngine::SetOwner(ActorTask* task) {
  task_ = task;
  actor_login_service_ = std::make_unique<actor_login::ActorLoginServiceImpl>();
  tool_controller_ = std::make_unique<ToolController>(*task_, *this);
}

void ExecutionEngine::SetState(State state) {
  journal_->Log(GURL(), task_->id(), mojom::JournalTrack::kActor,
                "ExecutionEngine::StateChange",
                absl::StrFormat("State %s -> %s", StateToString(state_),
                                StateToString(state)));

#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<State>> transitions(
      base::StateTransitions<State>({
          {State::kInit, {State::kStartAction, State::kComplete}},
          {State::kStartAction,
           {State::kToolCreateAndVerify, State::kComplete}},
          {State::kToolCreateAndVerify,
           {State::kUiPreInvoke, State::kComplete}},
          {State::kUiPreInvoke, {State::kToolInvoke, State::kComplete}},
          {State::kToolInvoke, {State::kUiPostInvoke, State::kComplete}},
          {State::kUiPostInvoke, {State::kComplete, State::kStartAction}},
          {State::kComplete, {State::kStartAction}},
      }));
  DCHECK_STATE_TRANSITION(transitions, state_, state);
#endif  // DCHECK_IS_ON()
  state_ = state;
}

std::string ExecutionEngine::StateToString(State state) {
  switch (state) {
    case State::kInit:
      return "INIT";
    case State::kStartAction:
      return "START_ACTION";
    case State::kToolCreateAndVerify:
      return "CREATE_AND_VERIFY";
    case State::kUiPreInvoke:
      return "UI_PRE_INVOKE";
    case State::kToolInvoke:
      return "TOOL_INVOKE";
    case State::kUiPostInvoke:
      return "UI_POST_INVOKE";
    case State::kComplete:
      return "COMPLETE";
  }
}

void ExecutionEngine::RegisterWithProfile(Profile* profile) {
  InitActionBlocklist(profile);
}

void ExecutionEngine::CancelOngoingActions(mojom::ActionResultCode reason) {
  if (!action_sequence_.empty()) {
    CompleteActions(MakeResult(reason), /*action_index=*/std::nullopt);
  }
}

void ExecutionEngine::FailCurrentTool(mojom::ActionResultCode reason) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_NE(reason, mojom::ActionResultCode::kOk);
  if (state_ != State::kToolInvoke) {
    return;
  }

  external_tool_failure_reason_ = reason;
}

void ExecutionEngine::Act(std::vector<std::unique_ptr<ToolRequest>>&& actions,
                          ActorTask::ActCallback callback) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  CHECK(!actions.empty());
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(task_->GetState(), ActorTask::State::kActing);

  if (!action_sequence_.empty()) {
    journal_->Log(
        actions[0]->GetURLForJournal(), task_->id(),
        mojom::JournalTrack::kActor, "Act Failed",
        "Unable to perform action: task already has action in progress");
    PostTaskForActCallback(std::move(callback),
                           MakeResult(mojom::ActionResultCode::kError,
                                      "Task already has action in progress"),
                           std::nullopt);
    return;
  }

  act_callback_ = std::move(callback);
  next_action_index_ = 0;

  absl::flat_hash_set<int32_t> acting_tab_handles;

  action_sequence_ = std::move(actions);
  for (const std::unique_ptr<ToolRequest>& action : action_sequence_) {
    CHECK(action);
    if (action->GetTabHandle() != tabs::TabHandle::Null()) {
      acting_tab_handles.insert(action->GetTabHandle().raw_value());
    }
  }

  if (state_ == State::kInit) {
    // This is the first Act() by this ExecutionEngine, so we should notify
    // the UI, then kickoff the first action.
    //
    // TODO(crbug.com/411462297): Make sure we're property dispatching
    // StartingToActOnTab UiEvents when tasks aren't scoped to a single tab.
    // This won't work if the first action sequence is creating the tab on which
    // following sequences will act.
    // TODO(crbug.com/420669167): This needs to support taking multiple tabs. Is
    // it even the right interface? Different sets of tabs might be acted on in
    // followup sequences...
    ui_event_dispatcher_->OnPreFirstAct(
        ui::UiEventDispatcher::FirstActInfo{
            .task_id = task_->id(),
            .tab_handle = acting_tab_handles.empty()
                              ? std::nullopt
                              : std::make_optional(tabs::TabHandle(
                                    *acting_tab_handles.begin()))},
        base::BindOnce(&ExecutionEngine::KickOffNextAction, GetWeakPtr()));
  } else {
    // We previously notified the UI, so just kickoff the first action.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ExecutionEngine::KickOffNextAction,
                                  GetWeakPtr(), MakeOkResult()));
  }
}

void ExecutionEngine::KickOffNextAction(
    mojom::ActionResultPtr init_hooks_result) {
  DCHECK(state_ == State::kInit || state_ == State::kUiPostInvoke ||
         state_ == State::kComplete)
      << "Current state is " << StateToString(state_);
  CHECK_LT(next_action_index_, action_sequence_.size());

  // The init hooks errored out.
  if (init_hooks_result && !IsOk(*init_hooks_result)) {
    CompleteActions(std::move(init_hooks_result),
                    /*action_index=*/std::nullopt);
    return;
  }

  SetState(State::kStartAction);

  // TODO(crbug.com/411462297): It's not clear that navigate requests (which are
  // tab scoped) should be doing tab safety checks. For now we return `true` to
  // preserve existing behavior.
  if (GetNextAction().IsTabScoped()) {
    SafetyChecksForNextAction();
  } else {
    ExecuteNextAction();
  }
}

void ExecutionEngine::SafetyChecksForNextAction() {
  tabs::TabInterface* tab = GetNextAction().GetTabHandle().Get();

  if (!tab) {
    journal_->Log(GURL::EmptyGURL(), task_->id(), mojom::JournalTrack::kActor,
                  "Act Failed", "The tab is no longer present");
    CompleteActions(MakeResult(mojom::ActionResultCode::kTabWentAway,
                               "The tab is no longer present."),
                    next_action_index_);
    return;
  }

  // Asynchronously check if we can act on the tab.
  MayActOnTab(
      *tab, *journal_, task_->id(),
      base::BindOnce(
          &ExecutionEngine::DidFinishAsyncSafetyChecks, GetWeakPtr(),
          tab->GetContents()->GetPrimaryMainFrame()->GetLastCommittedOrigin()));
}

void ExecutionEngine::DidFinishAsyncSafetyChecks(
    const url::Origin& evaluated_origin,
    bool may_act) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!action_sequence_.empty());

  tabs::TabInterface* tab = GetNextAction().GetTabHandle().Get();
  if (!tab) {
    journal_->Log(GURL::EmptyGURL(), task_->id(), mojom::JournalTrack::kActor,
                  "Act Failed", "The tab is no longer present");
    CompleteActions(MakeResult(mojom::ActionResultCode::kTabWentAway,
                               "The tab is no longer present."),
                    next_action_index_);
    return;
  }

  TaskId task_id = task_->id();
  if (!evaluated_origin.IsSameOriginWith(tab->GetContents()
                                             ->GetPrimaryMainFrame()
                                             ->GetLastCommittedOrigin())) {
    // A cross-origin navigation occurred before we got permission. The result
    // is no longer applicable. For now just fail.
    // TODO(mcnee): Handle this gracefully.
    journal_->Log(GetNextAction().GetURLForJournal(), task_id,
                  mojom::JournalTrack::kActor, "Act Failed",
                  "Acting after cross-origin navigation occurred");
    CompleteActions(MakeResult(mojom::ActionResultCode::kCrossOriginNavigation,
                               "Acting after cross-origin navigation occurred"),
                    next_action_index_);
    return;
  }

  if (!may_act) {
    journal_->Log(GetNextAction().GetURLForJournal(), task_id,
                  mojom::JournalTrack::kActor, "Act Failed",
                  "URL blocked for actions");
    CompleteActions(MakeResult(mojom::ActionResultCode::kUrlBlocked,
                               "URL blocked for actions"),
                    next_action_index_);
    return;
  }

  ExecuteNextAction();
}

void ExecutionEngine::ExecuteNextAction() {
  DCHECK_EQ(state_, State::kStartAction);
  CHECK(!action_sequence_.empty());
  CHECK(tool_controller_);

  ++next_action_index_;

  SetState(State::kToolCreateAndVerify);
  tool_controller_->CreateToolAndValidate(
      GetInProgressAction(), last_observed_page_content_.get(),
      base::BindOnce(&ExecutionEngine::PostToolCreate, GetWeakPtr()));
}

void ExecutionEngine::PostToolCreate(mojom::ActionResultPtr result) {
  if (!IsOk(*result)) {
    CompleteActions(std::move(result), InProgressActionIndex());
    return;
  }
  SetState(State::kUiPreInvoke);
  ui_event_dispatcher_->OnPreTool(
      GetInProgressAction(),
      base::BindOnce(&ExecutionEngine::FinishedUiPreInvoke, GetWeakPtr()));
}

void ExecutionEngine::FinishedUiPreInvoke(mojom::ActionResultPtr result) {
  DCHECK_EQ(state_, State::kUiPreInvoke);
  if (!IsOk(*result)) {
    CompleteActions(std::move(result), InProgressActionIndex());
    return;
  }

  SetState(State::kToolInvoke);
  tool_controller_->Invoke(
      base::BindOnce(&ExecutionEngine::FinishedToolInvoke, GetWeakPtr()));
}

void ExecutionEngine::FinishedToolInvoke(mojom::ActionResultPtr result) {
  DCHECK_EQ(state_, State::kToolInvoke);
  // The current action errored out. Stop the chain.
  std::optional<mojom::ActionResultCode> external_tool_failure_reason;
  std::swap(external_tool_failure_reason, external_tool_failure_reason_);
  if (external_tool_failure_reason) {
    CompleteActions(MakeResult(*external_tool_failure_reason),
                    InProgressActionIndex());
    return;
  }
  if (!IsOk(*result)) {
    CompleteActions(std::move(result), InProgressActionIndex());
    return;
  }

  SetState(State::kUiPostInvoke);
  ui_event_dispatcher_->OnPostTool(
      GetInProgressAction(),
      base::BindOnce(&ExecutionEngine::FinishedUiPostInvoke, GetWeakPtr()));
}

void ExecutionEngine::FinishedUiPostInvoke(mojom::ActionResultPtr result) {
  DCHECK_EQ(state_, State::kUiPostInvoke);
  CHECK(!action_sequence_.empty());

  if (!IsOk(*result)) {
    CompleteActions(std::move(result), InProgressActionIndex());
    return;
  }

  if (next_action_index_ >= action_sequence_.size()) {
    CompleteActions(MakeOkResult(), std::nullopt);
    return;
  }

  KickOffNextAction(/*init_hooks_result=*/nullptr);
}

void ExecutionEngine::CompleteActions(mojom::ActionResultPtr result,
                                      std::optional<size_t> action_index) {
  CHECK(!action_sequence_.empty());
  CHECK(act_callback_);

  SetState(State::kComplete);

  if (!IsOk(*result)) {
    GURL url;
    if (action_index) {
      url = action_sequence_[*action_index]->GetURLForJournal();
    }
    journal_->Log(url, task_->id(), mojom::JournalTrack::kActor, "Act Failed",
                  ToDebugString(*result));
  }

  // TODO(crbug.com/411462297): Populate observation.
  PostTaskForActCallback(std::move(act_callback_), std::move(result),
                         action_index);

  action_sequence_.clear();
  next_action_index_ = 0;
  actions_weak_ptr_factory_.InvalidateWeakPtrs();
  // TODO(crbug.com/409559623): Conceptually this should also reset
  // `last_observed_page_content_`.
}

void ExecutionEngine::DidObserveContext(
    const mojo_base::ProtoWrapper& apc_proto) {
  last_observed_page_content_ = std::make_unique<AnnotatedPageContent>(
      apc_proto.As<AnnotatedPageContent>().value());
}

const AnnotatedPageContent* ExecutionEngine::GetLastObservedPageContent() {
  return last_observed_page_content_.get();
}

base::WeakPtr<ExecutionEngine> ExecutionEngine::GetWeakPtr() {
  return actions_weak_ptr_factory_.GetWeakPtr();
}

AggregatedJournal& ExecutionEngine::GetJournal() {
  return *journal_;
}

actor_login::ActorLoginService& ExecutionEngine::GetActorLoginService() {
  return *actor_login_service_;
}

void ExecutionEngine::SetActorLoginServiceForTesting(
    std::unique_ptr<actor_login::ActorLoginService> test_service) {
  actor_login_service_ = std::move(test_service);
}

const ToolRequest& ExecutionEngine::GetNextAction() const {
  CHECK_LT(next_action_index_, action_sequence_.size());
  return *action_sequence_.at(next_action_index_).get();
}

size_t ExecutionEngine::InProgressActionIndex() const {
  CHECK(state_ == State::kUiPreInvoke || state_ == State::kToolInvoke ||
        state_ == State::kUiPostInvoke || state_ == State::kToolCreateAndVerify)
      << "Current state is " << StateToString(state_);
  CHECK_GT(next_action_index_, 0ul);
  return next_action_index_ - 1;
}

const ToolRequest& ExecutionEngine::GetInProgressAction() const {
  return *action_sequence_.at(InProgressActionIndex()).get();
}

std::ostream& operator<<(std::ostream& o, const ExecutionEngine::State& s) {
  return o << ExecutionEngine::StateToString(s);
}

}  // namespace actor
