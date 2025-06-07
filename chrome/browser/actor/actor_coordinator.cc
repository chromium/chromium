// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_coordinator.h"

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
#include "components/optimization_guide/proto/features/actions_data.pb.h"
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
using optimization_guide::proto::ActionInformation;
using optimization_guide::proto::ActionTarget;
using optimization_guide::proto::AnnotatedPageContent;
using optimization_guide::proto::BrowserAction;
using tabs::TabInterface;

namespace actor {

namespace {

void PostTaskForActCallback(ActorCoordinator::ActionResultCallback callback,
                            mojom::ActionResultPtr result) {
  UMA_HISTOGRAM_ENUMERATION("Actor.ActorCoordinator.Action.ResultCode",
                            result->code);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

}  // namespace

ActorCoordinator::Actions::Actions(const BrowserAction& actions,
                                   ActionResultCallback callback)
    : proto(actions), callback(std::move(callback)) {}

ActorCoordinator::Actions::~Actions() = default;

ActorCoordinator::ActorCoordinator(Profile* profile)
    : profile_(profile),
      journal_(ActorKeyedService::Get(profile)->GetJournal().GetSafeRef()) {
  CHECK(profile_);
  // Idempotent. Enables the action blocklist if it isn't already enabled.
  InitActionBlocklist(profile_.get());
}

ActorCoordinator::ActorCoordinator(Profile* profile, tabs::TabInterface* tab)
    : profile_(profile),
      journal_(ActorKeyedService::Get(profile)->GetJournal().GetSafeRef()),
      tab_scoped_actions_deprecated_(true),
      tab_(tab->GetWeakPtr()) {
  CHECK(profile_);
  // Idempotent. Enables the action blocklist if it isn't already enabled.
  InitActionBlocklist(profile_.get());
}

ActorCoordinator::~ActorCoordinator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
void ActorCoordinator::RegisterWithProfile(Profile* profile) {
  InitActionBlocklist(profile);
}

void ActorCoordinator::PauseTask() {
  if (actions_) {
    CompleteActions(
        MakeResult(mojom::ActionResultCode::kTaskPaused, "Task was paused"));
  }
}

void ActorCoordinator::StopTask() {
  if (actions_) {
    CompleteActions(
        MakeResult(mojom::ActionResultCode::kTaskWentAway, "Task was stopped"));
  }
}

tabs::TabInterface* ActorCoordinator::GetTabOfCurrentTask() const {
  return tab_.get();
}

bool ActorCoordinator::HasTask() const {
  return !!actions_;
}

bool ActorCoordinator::HasTaskForTab(const content::WebContents* tab) const {
  return HasTask() && tab_ && tab_->GetContents() == tab;
}

void ActorCoordinator::Act(const BrowserAction& action,
                           ActionResultCallback callback) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  TaskId task_id(action.task_id());
  if (tab_scoped_actions_deprecated_ && !tab_) {
    journal_->Log(LastCommittedURLOfCurrentTask(), task_id, "Act Failed",
                  "Unable to perform action: tab has been destroyed");
    PostTaskForActCallback(std::move(callback),
                           MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  // NOTE: Improve this API by queuing the action instead.
  if (actions_) {
    journal_->Log(
        LastCommittedURLOfCurrentTask(), task_id, "Act Failed",
        "Unable to perform action: task already has action in progress");
    PostTaskForActCallback(std::move(callback),
                           MakeResult(mojom::ActionResultCode::kError,
                                      "Task already has action in progress"));
    return;
  }

  actions_.emplace(action, std::move(callback));
  action_index_ = 0;

  MayActOnTab(*tab_, base::BindOnce(&ActorCoordinator::OnMayActOnTabResponse,
                                    GetWeakPtr(), task_id,
                                    tab_->GetContents()
                                        ->GetPrimaryMainFrame()
                                        ->GetLastCommittedOrigin()));
}

void ActorCoordinator::OnMayActOnTabResponse(
    TaskId task_id,
    const url::Origin& evaluated_origin,
    bool may_act) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // The actions have already completed or canceled.
  if (!actions_) {
    return;
  }

  if (tab_scoped_actions_deprecated_ && !tab_.get()) {
    journal_->Log(
        LastCommittedURLOfCurrentTask(), task_id, "Act Failed",
        "Unable to perform action: Tab closed while checking site policy");
    CompleteActions(MakeResult(mojom::ActionResultCode::kTabWentAway,
                               "Tab closed while checking site policy"));
    return;
  }

  if (!evaluated_origin.IsSameOriginWith(tab_->GetContents()
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

  // We intentionally allow an empty array of actions, since the model may use
  // this to get APC.
  PerformOneAction(task_id, /*previous_action_result=*/MakeOkResult());
}

void ActorCoordinator::PerformOneAction(
    TaskId task_id,
    mojom::ActionResultPtr previous_action_result) {
  // Something else has reset actions_.
  if (!actions_) {
    return;
  }

  BrowserAction& proto = actions_->proto;

  // All actions finished.
  if (proto.action_information_size() <= action_index_) {
    CompleteActions(std::move(previous_action_result));
    return;
  }

  // Kick off the next action, and increment action_index.
  const ActionInformation& action =
      proto.action_information().at(action_index_++);

  // TODO(https://crbug.com/411462297): tabs should not be required for all
  // actions.
  if (!tab_) {
    journal_->Log(LastCommittedURLOfCurrentTask(), task_id, "Act Failed",
                  "The tab is no longer present");
    CompleteActions(MakeResult(mojom::ActionResultCode::kTabWentAway,
                               "The tab is no longer present."));
    return;
  }

  RenderFrameHost* target_frame = FindTargetFrame(*tab_->GetContents(), action);

  if (!target_frame) {
    journal_->Log(LastCommittedURLOfCurrentTask(), task_id, "Act Failed",
                  "The target frame is no longer present in the tab.");
    CompleteActions(
        MakeResult(mojom::ActionResultCode::kFrameWentAway,
                   "The target frame is no longer present in the tab."));
    return;
  }
  tool_controller_.Invoke(action, *journal_, task_id, *target_frame,
                          base::BindOnce(&ActorCoordinator::FinishOneAction,
                                         GetWeakPtr(), task_id));
}

void ActorCoordinator::FinishOneAction(TaskId task_id,
                                       mojom::ActionResultPtr result) {
  // The task is no longer relevant.
  if (!actions_) {
    return;
  }

  // The current action errored out. Stop the chain.
  if (!IsOk(*result)) {
    CompleteActions(std::move(result));
    return;
  }

  PerformOneAction(task_id, std::move(result));
}

void ActorCoordinator::CompleteActions(mojom::ActionResultPtr result) {
  if (!actions_) {
    return;
  }

  if (!IsOk(*result)) {
    journal_->Log(LastCommittedURLOfCurrentTask(),
                  TaskId(actions_->proto.task_id()), "Act Failed",
                  ToDebugString(*result));
  }

  PostTaskForActCallback(std::move(actions_->callback), std::move(result));
  actions_.reset();
  action_index_ = 0;
}

void ActorCoordinator::DidObserveContext(
    const mojo_base::ProtoWrapper& apc_proto) {
  last_observed_page_content_ = std::make_unique<AnnotatedPageContent>(
      apc_proto.As<AnnotatedPageContent>().value());
}

const AnnotatedPageContent* ActorCoordinator::GetLastObservedPageContent() {
  return last_observed_page_content_.get();
}

base::WeakPtr<ActorCoordinator> ActorCoordinator::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

const GURL& ActorCoordinator::LastCommittedURLOfCurrentTask() {
  if (!tab_) {
    return GURL::EmptyGURL();
  }
  return tab_->GetContents()->GetLastCommittedURL();
}

}  // namespace actor
