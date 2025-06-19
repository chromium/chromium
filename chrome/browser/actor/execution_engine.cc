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
#include "base/notimplemented.h"
#include "base/types/id_type.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/tools/tool_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/optimization_guide/content/browser/page_content_proto_util.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
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

// Whether the action requires a frame.
bool ActionRequiresFrame(const Action& action) {
  switch (action.action_case()) {
    case Action::kClick:
    case Action::kType:
    case Action::kScroll:
    case Action::kMoveMouse:
    case Action::kDragAndRelease:
    case Action::kSelect:
      return true;
    // TODO(crbug.com/411462297): These requests do not require frames. For now
    // we return `true` to preserve existing behavior.
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

// Perform validation based on APC and document identifier for coordinate based
// target to compare the candidate frame with the target frame identified in
// last observation.
bool ValidateTargetFrameCandidate(
    const ActionTarget* target,
    RenderFrameHost* candidate_frame,
    WebContents& web_contents,
    AnnotatedPageContent* last_observed_page_content) {
  std::string target_document_token;
  // The document identifier in the action itself takes highest precedence.
  if (target->has_document_identifier()) {
    target_document_token = target->document_identifier().serialized_token();
  } else if (last_observed_page_content) {
    // Otherwise, fall back to APC hit testing.
    // TODO(crbug.com/426021822): FindNodeAtPoint does not handle corner cases
    // like clip paths. Need more checks to ensure we don't drop actions
    // unnecessarily.
    std::optional<optimization_guide::TargetNodeInfo> target_node_info =
        optimization_guide::FindNodeAtPoint(*last_observed_page_content,
                                            target->coordinate());
    if (target_node_info) {
      target_document_token =
          target_node_info->document_identifier.serialized_token();
    } else {
      return false;
    }
  } else {
    // An error if document identifier isn't set on target and no cached APC
    // available
    return false;
  }

  RenderFrameHost* apc_target_frame =
      GetRenderFrameForDocumentIdentifier(web_contents, target_document_token);

  // Only return the candidate if its RenderWidgetHost matches the target
  // and it's also a local root frame(i.e. has no parent or parent has
  // a different RenderWidgetHost)
  if (apc_target_frame && apc_target_frame->GetRenderWidgetHost() ==
                              candidate_frame->GetRenderWidgetHost()) {
    return true;
  }
  return false;
}

}  // namespace

ExecutionEngine::ExecutionEngine(Profile* profile)
    : profile_(profile),
      journal_(ActorKeyedService::Get(profile)->GetJournal().GetSafeRef()) {
  CHECK(profile_);
  // Idempotent. Enables the action blocklist if it isn't already enabled.
  InitActionBlocklist(profile_.get());
}

ExecutionEngine::ExecutionEngine(Profile* profile, tabs::TabInterface* tab)
    : profile_(profile),
      journal_(ActorKeyedService::Get(profile)->GetJournal().GetSafeRef()),
      tab_scoped_actions_deprecated_(true),
      tab_(tab) {
  CHECK(profile_);
  // Idempotent. Enables the action blocklist if it isn't already enabled.
  InitActionBlocklist(profile_.get());

  CHECK(tab_);
  tab_will_detach_subscription_ = tab_->RegisterWillDetach(base::BindRepeating(
      &ExecutionEngine::OnTabWillDetach, base::Unretained(this)));
}

ExecutionEngine::~ExecutionEngine() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ExecutionEngine::SetOwner(ActorTask* task) {
  task_ = task;
}

// static
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
  if (actions_v1_) {
    BrowserAction& proto = actions_v1_->proto;
    if (proto.actions_size() <= action_index_) {
      CompleteActionsV1(std::move(previous_action_result));
      return;
    }
  } else {
    auto& proto = actions_v2_->proto;
    if (proto.actions_size() <= action_index_) {
      CompleteActionsV2(std::move(previous_action_result));
      return;
    }
  }

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
  CHECK(actions_v1_ || actions_v2_);

  const Action& action = GetNextAction();
  ++action_index_;

  tabs::TabInterface* tab = GetTab(action);

  if (ActionRequiresTab(action) && !tab) {
    journal_->Log(GURL::EmptyGURL(), task_->id(), "Act Failed",
                  "The tab is no longer present");
    CompleteActions(MakeResult(mojom::ActionResultCode::kTabWentAway,
                               "The tab is no longer present."));
    return;
  }
  RenderFrameHost* target_frame_candidate = nullptr;
  if (ActionRequiresFrame(action)) {
    target_frame_candidate =
        FindTargetLocalRootFrame(*tab->GetContents(), action);

    if (!target_frame_candidate) {
      journal_->Log(LastCommittedURLOfCurrentTask(), task_->id(), "Act Failed",
                    "The target frame is no longer present in the tab.");
      CompleteActions(
          MakeResult(mojom::ActionResultCode::kFrameWentAway,
                     "The target frame is no longer present in the tab."));
      return;
    }

    const ActionTarget* target = ExtractTarget(action);

    // Perform validation for coordinate based target only.
    if (target && target->has_coordinate() &&
        !ValidateTargetFrameCandidate(target, target_frame_candidate,
                                      *tab->GetContents(),
                                      last_observed_page_content_.get())) {
      journal_->Log(LastCommittedURLOfCurrentTask(), task_->id(), "Act Failed",
                    "The target frame has changed.");
      CompleteActions(MakeResult(
          mojom::ActionResultCode::kFrameLocationChangedSinceObservation,
          "The target frame has changed."));
      return;
    }
  }
  tool_controller_.Invoke(
      action, *journal_, task_->id(), tab, target_frame_candidate,
      base::BindOnce(&ExecutionEngine::FinishOneAction, GetWeakPtr()));
}

void ExecutionEngine::FinishOneAction(mojom::ActionResultPtr result) {
  CHECK(actions_v1_ || actions_v2_);

  // The current action errored out. Stop the chain.
  if (!IsOk(*result)) {
    CompleteActions(std::move(result));
    return;
  }

  KickOffNextAction(std::move(result));
}

void ExecutionEngine::CompleteActions(mojom::ActionResultPtr result) {
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

}  // namespace actor
