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

// Whether we need to run synchronous and asynchronous, tab-scoped safety
// checks.
bool ActionRequiresTabScopedSafetyChecks(const Action& action) {
  switch (action.action_case()) {
    case Action::kClick:
    case Action::kType:
    case Action::kScroll:
    case Action::kMoveMouse:
    case Action::kDragAndRelease:
    case Action::kSelect:
      return true;
    // TODO(crbug.com/411462297): It's not clear that navigate and wait requests
    // should be doing tab safety checks. For now we return `true` to preserve
    // existing behavior.
    case Action::kBack:
    case Action::kForward:
    case Action::kNavigate:
    case Action::kWait:
      return true;
    case Action::kCreateTab:
    case Action::kCloseTab:
    case Action::kActivateTab:
    case Action::kCreateWindow:
    case Action::kCloseWindow:
    case Action::kActivateWindow:
    case Action::kYieldToUser:
    case Action::ACTION_NOT_SET:
      return false;
  }
}

tabs::TabHandle GetTabHandleFromAction(
    const optimization_guide::proto::Action& action) {
  switch (action.action_case()) {
    case Action::kClick:
      return tabs::TabHandle(action.click().tab_id());
    case Action::kType:
      return tabs::TabHandle(action.type().tab_id());
    case Action::kScroll:
      return tabs::TabHandle(action.scroll().tab_id());
    case Action::kMoveMouse:
      return tabs::TabHandle(action.move_mouse().tab_id());
    case Action::kDragAndRelease:
      return tabs::TabHandle(action.drag_and_release().tab_id());
    case Action::kSelect:
      return tabs::TabHandle(action.select().tab_id());
    case Action::kBack:
      return tabs::TabHandle(action.back().tab_id());
    case Action::kForward:
      return tabs::TabHandle(action.forward().tab_id());
    case Action::kNavigate:
      return tabs::TabHandle(action.navigate().tab_id());
    case Action::kCloseTab:
      return tabs::TabHandle(action.close_tab().tab_id());
    case Action::kActivateTab:
      return tabs::TabHandle(action.activate_tab().tab_id());
    case Action::kWait:
    case Action::kCreateTab:
    case Action::kCreateWindow:
    case Action::kCloseWindow:
    case Action::kActivateWindow:
    case Action::kYieldToUser:
    case Action::ACTION_NOT_SET:
      return tabs::TabHandle();
  }
}

// Whether the action requires a tab.
bool ActionRequiresTab(const Action& action) {
  switch (action.action_case()) {
    case Action::kClick:
    case Action::kType:
    case Action::kScroll:
    case Action::kMoveMouse:
    case Action::kDragAndRelease:
    case Action::kSelect:
    case Action::kBack:
    case Action::kForward:
    case Action::kNavigate:
    case Action::kWait:
    case Action::kCloseTab:
    case Action::kActivateTab:
      return true;
    case Action::kCreateTab:
    case Action::kCreateWindow:
    case Action::kCloseWindow:
    case Action::kActivateWindow:
    case Action::kYieldToUser:
    case Action::ACTION_NOT_SET:
      return false;
  }
}

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
      ui_event_dispatcher_(NewUiEventDispatcher()) {
  CHECK(profile_);
  // Idempotent. Enables the action blocklist if it isn't already enabled.
  InitActionBlocklist(profile_.get());
}

ExecutionEngine::ExecutionEngine(Profile* profile, tabs::TabInterface* tab)
    : profile_(profile),
      journal_(ActorKeyedService::Get(profile)->GetJournal().GetSafeRef()),
      tab_scoped_actions_deprecated_(true),
      tab_(tab),
      ui_event_dispatcher_(NewUiEventDispatcher()) {
  CHECK(profile_);
  // Idempotent. Enables the action blocklist if it isn't already enabled.
  InitActionBlocklist(profile_.get());

  CHECK(tab_);
  tab_will_detach_subscription_ = tab_->RegisterWillDetach(base::BindRepeating(
      &ExecutionEngine::OnTabWillDetach, base::Unretained(this)));
}

ExecutionEngine::ExecutionEngine(
    Profile* profile,
    std::unique_ptr<UiEventDispatcher> ui_event_dispatcher,
    tabs::TabInterface* tab)
    : profile_(profile),
      journal_(ActorKeyedService::Get(profile)->GetJournal().GetSafeRef()),
      tab_scoped_actions_deprecated_(true),
      tab_(tab),
      ui_event_dispatcher_(std::move(ui_event_dispatcher)) {
  CHECK(profile_);
  // Idempotent. Enables the action blocklist if it isn't already enabled.
  InitActionBlocklist(profile_.get());

  CHECK(tab_);
  tab_will_detach_subscription_ = tab_->RegisterWillDetach(base::BindRepeating(
      &ExecutionEngine::OnTabWillDetach, base::Unretained(this)));
}

std::unique_ptr<ExecutionEngine> ExecutionEngine::CreateForTesting(
    Profile* profile,
    std::unique_ptr<UiEventDispatcher> ui_event_dispatcher,
    tabs::TabInterface* tab) {
  return base::WrapUnique<ExecutionEngine>(
      new ExecutionEngine(profile, std::move(ui_event_dispatcher), tab));
}

ExecutionEngine::~ExecutionEngine() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ExecutionEngine::SetOwner(ActorTask* task) {
  task_ = task;
  tool_controller_ = std::make_unique<ToolController>(task_->id(), *journal_);
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
          // TODO(crbug.com/425784083): Confirm if this transition is valid
          // outside of tests.
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
  if (actions_v1_) {
    CompleteActions(MakeResult(reason));
  }
}

tabs::TabInterface* ExecutionEngine::GetTabOfCurrentTask() const {
  return tab_;
}

bool ExecutionEngine::HasTask() const {
  return !!actions_v1_ || !!actions_v2_;
}

bool ExecutionEngine::HasTaskForTab(const content::WebContents* tab) const {
  return HasTask() && tab_ && tab_->GetContents() == tab;
}

void ExecutionEngine::Act(const BrowserAction& action,
                          ActionResultCallback callback) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TaskId task_id(action.task_id());

  if (task_->IsPaused()) {
    journal_->Log(LastCommittedURLOfCurrentTask(), task_id, "Act Failed",
                  "Unable to perform action: task is paused");
    PostTaskForActCallback(std::move(callback),
                           MakeResult(mojom::ActionResultCode::kTaskPaused));
    return;
  }

  // NOTE: Improve this API by queuing the action instead.
  if (actions_v1_ || actions_v2_) {
    journal_->Log(
        LastCommittedURLOfCurrentTask(), task_id, "Act Failed",
        "Unable to perform action: task already has action in progress");
    PostTaskForActCallback(std::move(callback),
                           MakeResult(mojom::ActionResultCode::kError,
                                      "Task already has action in progress"));
    return;
  }

  actions_v1_.emplace(action, std::move(callback));
  action_index_ = 0;

  // Kick off the first action.
  KickOffNextAction(/*previous_action_result=*/MakeOkResult());
}

void ExecutionEngine::Act(const Actions& actions,
                          ActionsResultCallback callback) {
  // actions_v2_ never uses tab-scoped tasks.
  CHECK(!tab_scoped_actions_deprecated_);
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TaskId task_id(actions.task_id());

  if (task_->IsPaused()) {
    journal_->Log(LastCommittedURLOfCurrentTask(), task_id, "Act Failed",
                  "Unable to perform action: task is paused");
    optimization_guide::proto::ActionsResult result;
    result.set_action_result(
        static_cast<int32_t>(mojom::ActionResultCode::kTaskPaused));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
    return;
  }

  if (actions_v1_ || actions_v2_) {
    journal_->Log(
        LastCommittedURLOfCurrentTask(), task_id, "Act Failed",
        "Unable to perform action: task already has action in progress");
    optimization_guide::proto::ActionsResult result;
    result.set_action_result(
        static_cast<int32_t>(mojom::ActionResultCode::kError));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
    return;
  }

  actions_v2_.emplace(actions, std::move(callback));
  action_index_ = 0;

  // Kick off the first action.
  KickOffNextAction(/*previous_action_result=*/MakeOkResult());
}

void ExecutionEngine::KickOffNextAction(
    mojom::ActionResultPtr previous_action_result) {
  // TODO(crbug.com/425784083): Allowing the transition from Complete here is
  // needed (at least) for some tests.
  DCHECK(state_ == State::kInit || state_ == State::kUiPostTool ||
         state_ == State::kComplete)
      << "Current state is " << StateToString(state_);
  if (actions_v1_) {
    BrowserAction& proto = actions_v1_->proto;
    if (proto.actions_size() <= action_index_) {
      CompleteActions(std::move(previous_action_result));
      return;
    }
  } else {
    auto& proto = actions_v2_->proto;
    if (proto.actions_size() <= action_index_) {
      CompleteActions(std::move(previous_action_result));
      return;
    }
  }

  SetState(State::kStartAction);
  if (ActionRequiresTabScopedSafetyChecks(GetNextAction())) {
    SafetyChecksForNextAction();
  } else {
    ExecuteNextAction();
  }
}

void ExecutionEngine::SafetyChecksForNextAction() {
  CHECK(ActionRequiresTab(GetNextAction()));
  tabs::TabInterface* tab = GetTab(GetNextAction());

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
  CHECK(actions_v1_ || actions_v2_);

  auto task_id = task_->id();
  tabs::TabInterface* tab = GetTab(GetNextAction());
  CHECK(tab);

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
  CHECK(actions_v1_ || actions_v2_);
  CHECK(tool_controller_);

  const Action& action = GetNextAction();
  ++action_index_;

  // TODO(bokan): ExecutionEngine shouldn't know about the Action proto, it
  // should operate in terms of ToolRequest.
  active_tool_request_ = CreateToolRequest(action, tab_);
  if (!active_tool_request_) {
    journal_->Log(GURL::EmptyGURL(), task_->id(), "Act Failed",
                  "Failed to convert ActionInformation proto to ToolRequest");
    CompleteActions(MakeResult(mojom::ActionResultCode::kArgumentsInvalid));
    return;
  }

  SetState(State::kUiPreTool);
  ui_event_dispatcher_->OnPreTool(
      profile_, *active_tool_request_,
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
      *active_tool_request_, last_observed_page_content_.get(),
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
      profile_, *active_tool_request_,
      base::BindOnce(&ExecutionEngine::FinishedUiPostTool, GetWeakPtr()));
}

void ExecutionEngine::FinishedUiPostTool(mojom::ActionResultPtr result) {
  DCHECK_EQ(state_, State::kUiPostTool);
  CHECK(actions_v1_ || actions_v2_);
  active_tool_request_.reset();

  // The current action errored out. Stop the chain.
  if (!IsOk(*result)) {
    CompleteActions(std::move(result));
    return;
  }

  KickOffNextAction(std::move(result));
}

void ExecutionEngine::CompleteActions(mojom::ActionResultPtr result) {
  SetState(State::kComplete);
  if (actions_v1_) {
    CompleteActionsV1(std::move(result));
    return;
  }
  if (actions_v2_) {
    CompleteActionsV2(std::move(result));
    return;
  }
}

void ExecutionEngine::CompleteActionsV1(mojom::ActionResultPtr result) {
  CHECK(actions_v1_);

  if (!IsOk(*result)) {
    journal_->Log(LastCommittedURLOfCurrentTask(),
                  TaskId(actions_v1_->proto.task_id()), "Act Failed",
                  ToDebugString(*result));
  }

  PostTaskForActCallback(std::move(actions_v1_->callback), std::move(result));
  actions_v1_.reset();
  action_index_ = 0;
  actions_weak_ptr_factory_.InvalidateWeakPtrs();
  // TODO(crbug.com/409559623): Conceptually this should also reset
  // `last_observed_page_content_`.
}

void ExecutionEngine::CompleteActionsV2(mojom::ActionResultPtr result) {
  CHECK(actions_v2_);

  optimization_guide::proto::ActionsResult actions_result;
  actions_result.set_action_result(static_cast<int32_t>(result->code));

  // TODO(crbug.com/411462297): Populate observation.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(actions_v2_->callback),
                                std::move(actions_result)));
  actions_v2_.reset();
  action_index_ = 0;
  actions_weak_ptr_factory_.InvalidateWeakPtrs();
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

  // actions_v2_ never uses tab-scoped tasks.
  if (tab_scoped_actions_deprecated_ && actions_v1_) {
    journal_->Log(LastCommittedURLOfCurrentTask(),
                  TaskId(actions_v1_->proto.task_id()), "Act Failed",
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

const optimization_guide::proto::Action& ExecutionEngine::GetNextAction() {
  if (actions_v1_) {
    return actions_v1_->proto.actions().at(action_index_);
  } else {
    return actions_v2_->proto.actions().at(action_index_);
  }
}

tabs::TabInterface* ExecutionEngine::GetTab(
    const optimization_guide::proto::Action& action) {
  tabs::TabHandle tab_handle = GetTabHandleFromAction(action);
  tabs::TabInterface* tab = tab_handle.Get();
  if (tab) {
    return tab;
  }
  if (tab_scoped_actions_deprecated_) {
    return tab_;
  }
  return nullptr;
}

std::ostream& operator<<(std::ostream& o, const ExecutionEngine::State& s) {
  return o << ExecutionEngine::StateToString(s);
}

}  // namespace actor
