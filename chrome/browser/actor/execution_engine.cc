// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/execution_engine.h"

#include <cstddef>
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
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/actor/tools/tool_controller.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/event_dispatcher.h"
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

void PostTaskForActCallback(ExecutionEngine::ActionResultCallback callback,
                            mojom::ActionResultPtr result) {
  UMA_HISTOGRAM_ENUMERATION("Actor.ExecutionEngine.Action.ResultCode",
                            result->code);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

}  // namespace

ExecutionEngine::ExecutionEngine(Profile* profile)
    : profile_(profile),
      journal_(ActorKeyedService::Get(profile)->GetJournal().GetSafeRef()),
      ui_event_dispatcher_(ui::NewUiEventDispatcher()) {
  CHECK(profile_);
  // Idempotent. Enables the action blocklist if it isn't already enabled.
  InitActionBlocklist(profile_.get());
}

ExecutionEngine::ExecutionEngine(Profile* profile, tabs::TabInterface* tab)
    : profile_(profile),
      journal_(ActorKeyedService::Get(profile)->GetJournal().GetSafeRef()),
      tab_(tab),
      ui_event_dispatcher_(ui::NewUiEventDispatcher()) {
  CHECK(profile_);
  // Idempotent. Enables the action blocklist if it isn't already enabled.
  InitActionBlocklist(profile_.get());

  CHECK(tab_);
  tab_will_detach_subscription_ = tab_->RegisterWillDetach(base::BindRepeating(
      &ExecutionEngine::OnTabWillDetach, base::Unretained(this)));
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
  tool_controller_ = std::make_unique<ToolController>(*task_, *journal_);
}

void ExecutionEngine::SetState(State state) {
  VLOG(1) << "ExecutionEngine state change: " << StateToString(state_) << " -> "
          << StateToString(state);
#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<State>> transitions(
      base::StateTransitions<State>({
          {State::kInit, {State::kStartAction, State::kComplete}},
          {State::kStartAction, {State::kUiPreTool, State::kComplete}},
          {State::kUiPreTool, {State::kToolController, State::kComplete}},
          {State::kToolController, {State::kUiPostTool, State::kComplete}},
          {State::kUiPostTool, {State::kComplete, State::kStartAction}},
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
    case State::kUiPreTool:
      return "UI_PRE_TOOL";
    case State::kToolController:
      return "TOOL_CONTROLLER";
    case State::kUiPostTool:
      return "UI_POST_TOOL";
    case State::kComplete:
      return "COMPLETE";
  }
}

void ExecutionEngine::RegisterWithProfile(Profile* profile) {
  InitActionBlocklist(profile);
}

void ExecutionEngine::CancelOngoingActions(mojom::ActionResultCode reason) {
  if (!action_sequence_.empty()) {
    CompleteActions(MakeResult(reason));
  }
}

void ExecutionEngine::Act(const BrowserAction& action,
                          ActionResultCallback callback) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(action.task_id(), task_->id().value());

  if (task_->IsPaused()) {
    journal_->Log(LastCommittedURLOfCurrentTask(), task_->id(), "Act Failed",
                  "Unable to perform action: task is paused");
    PostTaskForActCallback(std::move(callback),
                           MakeResult(mojom::ActionResultCode::kTaskPaused));
    return;
  }

  // NOTE: Improve this API by queuing the action instead.
  if (!action_sequence_.empty()) {
    journal_->Log(
        LastCommittedURLOfCurrentTask(), task_->id(), "Act Failed",
        "Unable to perform action: task already has action in progress");
    PostTaskForActCallback(std::move(callback),
                           MakeResult(mojom::ActionResultCode::kError,
                                      "Task already has action in progress"));
    return;
  }

  next_action_index_ = 0;
  act_callback_ = std::move(callback);

  absl::flat_hash_set<int32_t> acting_tab_handles;

  action_sequence_.reserve(action.actions_size());
  for (int i = 0; i < action.actions_size(); ++i) {
    std::unique_ptr<ToolRequest> request =
        CreateToolRequest(action.actions().at(i), tab_);
    if (request) {
      if (request->GetTabHandle() != tabs::TabHandle::Null()) {
        acting_tab_handles.insert(request->GetTabHandle().raw_value());
      }
      action_sequence_.push_back(std::move(request));
    } else {
      journal_->Log(GURL::EmptyGURL(), task_->id(), "Act Failed",
                    "Failed to convert ActionInformation proto to ToolRequest");
      CompleteActions(MakeResult(mojom::ActionResultCode::kArgumentsInvalid));
      return;
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
        profile_,
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

void ExecutionEngine::Act(const Actions& actions,
                          ActionResultCallback callback) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(actions.task_id(), task_->id().value());

  if (task_->IsPaused()) {
    journal_->Log(LastCommittedURLOfCurrentTask(), task_->id(), "Act Failed",
                  "Unable to perform action: task is paused");
    PostTaskForActCallback(std::move(callback),
                           MakeResult(mojom::ActionResultCode::kTaskPaused));
    return;
  }

  if (!action_sequence_.empty()) {
    journal_->Log(
        LastCommittedURLOfCurrentTask(), task_->id(), "Act Failed",
        "Unable to perform action: task already has action in progress");
    PostTaskForActCallback(std::move(callback),
                           MakeResult(mojom::ActionResultCode::kError,
                                      "Task already has action in progress"));
    return;
  }

  act_callback_ = std::move(callback);
  next_action_index_ = 0;

  absl::flat_hash_set<int32_t> acting_tab_handles;

  action_sequence_.reserve(actions.actions_size());
  for (int i = 0; i < actions.actions_size(); ++i) {
    // The tab for this path is always set from the proto.
    // TODO(crbug.com/411462297): Once BrowserAction is removed
    // CreateToolRequest will no longer take a fallback tab.
    std::unique_ptr<ToolRequest> request = CreateToolRequest(
        actions.actions().at(i), /*deprecated_fallback_tab=*/nullptr);
    if (request) {
      if (request->GetTabHandle() != tabs::TabHandle::Null()) {
        acting_tab_handles.insert(request->GetTabHandle().raw_value());
      }
      action_sequence_.push_back(std::move(request));
    } else {
      journal_->Log(GURL::EmptyGURL(), task_->id(), "Act Failed",
                    "Failed to convert ActionInformation proto to ToolRequest");
      CompleteActions(MakeResult(mojom::ActionResultCode::kArgumentsInvalid));
      return;
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
        profile_,
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
    mojom::ActionResultPtr previous_action_result) {
  // TODO(crbug.com/425784083): Allowing the transition from Complete here is
  // needed (at least) for some tests.
  DCHECK(state_ == State::kInit || state_ == State::kUiPostTool ||
         state_ == State::kComplete)
      << "Current state is " << StateToString(state_);

  // The previous action or init hooks errored out. Stop the chain.
  if (!IsOk(*previous_action_result)) {
    CompleteActions(std::move(previous_action_result));
    return;
  }

  if (next_action_index_ >= action_sequence_.size()) {
    CompleteActions(std::move(previous_action_result));
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
    journal_->Log(GURL::EmptyGURL(), task_->id(), "Act Failed",
                  "The tab is no longer present");
    CompleteActions(MakeResult(mojom::ActionResultCode::kTabWentAway,
                               "The tab is no longer present."));
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
    journal_->Log(GURL::EmptyGURL(), task_->id(), "Act Failed",
                  "The tab is no longer present");
    CompleteActions(MakeResult(mojom::ActionResultCode::kTabWentAway,
                               "The tab is no longer present."));
    return;
  }

  TaskId task_id = task_->id();
  if (!evaluated_origin.IsSameOriginWith(tab->GetContents()
                                             ->GetPrimaryMainFrame()
                                             ->GetLastCommittedOrigin())) {
    // A cross-origin navigation occurred before we got permission. The result
    // is no longer applicable. For now just fail.
    // TODO(mcnee): Handle this gracefully.
    journal_->Log(LastCommittedURLOfCurrentTask(), task_id, "Act Failed",
                  "Acting after cross-origin navigation occurred");
    CompleteActions(
        MakeResult(mojom::ActionResultCode::kCrossOriginNavigation,
                   "Acting after cross-origin navigation occurred"));
    return;
  }

  if (!may_act) {
    journal_->Log(LastCommittedURLOfCurrentTask(), task_id, "Act Failed",
                  "URL blocked for actions");
    CompleteActions(MakeResult(mojom::ActionResultCode::kUrlBlocked,
                               "URL blocked for actions"));
    return;
  }

  ExecuteNextAction();
}

void ExecutionEngine::ExecuteNextAction() {
  DCHECK_EQ(state_, State::kStartAction);
  CHECK(!action_sequence_.empty());
  CHECK(tool_controller_);

  ++next_action_index_;

  SetState(State::kUiPreTool);
  ui_event_dispatcher_->OnPreTool(
      profile_, GetInProgressAction(),
      base::BindOnce(&ExecutionEngine::FinishedUiPreTool, GetWeakPtr()));
}

void ExecutionEngine::FinishedUiPreTool(mojom::ActionResultPtr result) {
  DCHECK_EQ(state_, State::kUiPreTool);
  if (!IsOk(*result)) {
    CompleteActions(std::move(result));
    return;
  }

  SetState(State::kToolController);
  tool_controller_->Invoke(
      GetInProgressAction(), last_observed_page_content_.get(),
      base::BindOnce(&ExecutionEngine::FinishedToolController, GetWeakPtr()));
}

void ExecutionEngine::FinishedToolController(mojom::ActionResultPtr result) {
  DCHECK_EQ(state_, State::kToolController);
  // The current action errored out. Stop the chain.
  if (!IsOk(*result)) {
    CompleteActions(std::move(result));
    return;
  }

  SetState(State::kUiPostTool);
  ui_event_dispatcher_->OnPostTool(
      profile_, GetInProgressAction(),
      base::BindOnce(&ExecutionEngine::FinishedUiPostTool, GetWeakPtr()));
}

void ExecutionEngine::FinishedUiPostTool(mojom::ActionResultPtr result) {
  DCHECK_EQ(state_, State::kUiPostTool);
  CHECK(!action_sequence_.empty());

  KickOffNextAction(std::move(result));
}

void ExecutionEngine::CompleteActions(mojom::ActionResultPtr result) {
  CHECK(!action_sequence_.empty());
  CHECK(act_callback_);

  SetState(State::kComplete);

  if (!IsOk(*result)) {
    journal_->Log(LastCommittedURLOfCurrentTask(), task_->id(), "Act Failed",
                  ToDebugString(*result));
  }

  // TODO(crbug.com/411462297): Populate observation.
  PostTaskForActCallback(std::move(act_callback_), std::move(result));

  action_sequence_.clear();
  next_action_index_ = 0;
  actions_weak_ptr_factory_.InvalidateWeakPtrs();
  // TODO(crbug.com/409559623): Conceptually this should also reset
  // `last_observed_page_content_`.
}

void ExecutionEngine::OnTabWillDetach(tabs::TabInterface* tab,
                                      tabs::TabInterface::DetachReason reason) {
  if (reason != tabs::TabInterface::DetachReason::kDelete) {
    return;
  }
  if (!tab_) {
    return;
  }
  CHECK_EQ(tab, tab_);
  tab_ = nullptr;

  if (!action_sequence_.empty()) {
    journal_->Log(LastCommittedURLOfCurrentTask(), task_->id(), "Act Failed",
                  "The tab is no longer present");
    CompleteActions(MakeResult(mojom::ActionResultCode::kTabWentAway,
                               "The tab is no longer present."));
  }
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

const GURL& ExecutionEngine::LastCommittedURLOfCurrentTask() {
  if (!tab_) {
    return GURL::EmptyGURL();
  }
  return tab_->GetContents()->GetLastCommittedURL();
}

const ToolRequest& ExecutionEngine::GetNextAction() const {
  CHECK_LT(next_action_index_, action_sequence_.size());
  return *action_sequence_.at(next_action_index_).get();
}

const ToolRequest& ExecutionEngine::GetInProgressAction() const {
  CHECK(state_ == State::kUiPreTool || state_ == State::kToolController ||
        state_ == State::kUiPostTool)
      << "Current state is " << StateToString(state_);
  CHECK_GT(next_action_index_, 0ul);
  return *action_sequence_.at(next_action_index_ - 1).get();
}

std::ostream& operator<<(std::ostream& o, const ExecutionEngine::State& s) {
  return o << ExecutionEngine::StateToString(s);
}

}  // namespace actor
