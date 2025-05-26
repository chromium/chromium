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
#include "base/notimplemented.h"
#include "base/types/id_type.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/tools/tool_controller.h"
#include "chrome/browser/actor/tools/tool_invocation.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/chrome_features.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "url/origin.h"

using content::WebContents;
using optimization_guide::proto::ActionInformation;
using optimization_guide::proto::BrowserAction;
using tabs::TabInterface;

namespace actor {

namespace {

void PostTaskForStartCallback(ActorCoordinator::StartTaskCallback callback,
                              base::WeakPtr<tabs::TabInterface> tab) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), tab));
}

void PostTaskForActCallback(ActorCoordinator::ActionResultCallback callback,
                            mojom::ActionResultPtr result) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

}  // namespace

std::optional<base::TimeDelta>
    ActorCoordinator::action_observation_delay_for_testing_ = std::nullopt;

void ActorCoordinator::SetActionObservationDelayForTesting(
    const base::TimeDelta& delay) {
  action_observation_delay_for_testing_ = delay;
}

base::TimeDelta ActorCoordinator::GetActionObservationDelay() {
  return action_observation_delay_for_testing_.value_or(
      features::kGlicActorActorObservationDelay.Get());
}

// Waits for the navigation to complete to create a new tab.
class ActorCoordinator::NewTabWebContentsObserver
    : public content::WebContentsObserver {
 public:
  NewTabWebContentsObserver(
      base::WeakPtr<content::NavigationHandle> pending_navigation_handle,
      base::OnceCallback<void(content::WebContents*)> callback)
      : content::WebContentsObserver(
            CHECK_DEREF(pending_navigation_handle.get()).GetWebContents()),
        pending_navigation_handle_(pending_navigation_handle),
        callback_(std::move(callback)) {}

  ~NewTabWebContentsObserver() override { Notify(nullptr); }

  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    if (!pending_navigation_handle_) {
      Notify(nullptr);
      return;
    }
    if (navigation_handle->GetNavigationId() !=
        pending_navigation_handle_->GetNavigationId()) {
      return;
    }
    pending_navigation_handle_ = nullptr;

    bool success =
        navigation_handle->HasCommitted() && !navigation_handle->IsErrorPage();

    Notify(success ? navigation_handle->GetWebContents() : nullptr);
  }

  bool IsNavigationHandleValid() {
    return !pending_navigation_handle_.WasInvalidated();
  }

 private:
  void Notify(content::WebContents* web_contents) {
    // Notify may be called more than once to run the callback. The callback is
    // destroyed by the first call to Run().
    if (callback_) {
      std::move(callback_).Run(web_contents);
      // The callback provided by the parent class will destroy this instance.
    }
  }
  base::WeakPtr<content::NavigationHandle> pending_navigation_handle_;
  base::OnceCallback<void(content::WebContents*)> callback_;
};

// static
TaskId::Generator ActorCoordinator::Task::id_generator_;

ActorCoordinator::Action::Action(const BrowserAction& action,
                                 ActionResultCallback callback)
    : proto(action), callback(std::move(callback)) {}

ActorCoordinator::Action::~Action() = default;

ActorCoordinator::Task::Task(tabs::TabInterface& task_tab)
    : id(id_generator_.GenerateNextId()), tab(task_tab.GetWeakPtr()) {}
ActorCoordinator::Task::~Task() = default;

ActorCoordinator::ActorCoordinator(Profile* profile) : profile_(profile) {
  CHECK(profile_);
}

ActorCoordinator::~ActorCoordinator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
void ActorCoordinator::RegisterWithProfile(Profile* profile) {
  InitActionBlocklist(profile);
}

void ActorCoordinator::StartTask(const BrowserAction& action,
                                 StartTaskCallback callback,
                                 std::optional<tabs::TabHandle> tab_handle) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Try to start a new actor task, as only a single task is allowed at a time.
  // Posts to a sequence to avoid potential races with multiple attempts to
  // start a task.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ActorCoordinator::TryStartNewTask, GetWeakPtr(), action,
                     std::move(callback), std::move(tab_handle)));
}

void ActorCoordinator::StopTask() {
  if (!task_state_) {
    return;
  }

  if (task_state_->current_action) {
    CompleteAction(
        MakeResult(mojom::ActionResultCode::kTaskWentAway, "Task was stopped"));
  }

  task_state_.reset();
}

void ActorCoordinator::PauseTask() {
  if (!task_state_) {
    return;
  }

  if (task_state_->current_action) {
    CompleteAction(
        MakeResult(mojom::ActionResultCode::kTaskPaused, "Task was paused"));
  }
}

tabs::TabInterface* ActorCoordinator::GetTabOfCurrentTask() const {
  return task_state_ ? task_state_->tab.get() : nullptr;
}

bool ActorCoordinator::HasTask() const {
  return !!task_state_;
}

bool ActorCoordinator::HasTaskForTab(const content::WebContents* tab) const {
  return HasTask() && task_state_->HasTab() &&
         task_state_->tab->GetContents() == tab;
}

void ActorCoordinator::StartTaskForTesting(tabs::TabInterface* tab) {
  CHECK(tab);
  CHECK(!task_state_);
  task_state_ = std::make_unique<Task>(*tab);
}

void ActorCoordinator::Act(const BrowserAction& action,
                           ActionResultCallback callback) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // StartTask must have been called to initialize the tab for action.
  // NOTE: Improve this API by moving Act() to Task:: instead of permitting
  // actions without a Task.
  if (!task_state_) {
    VLOG(1) << "Unable to perform action: task hasn't been started";
    PostTaskForActCallback(std::move(callback),
                           MakeResult(mojom::ActionResultCode::kError,
                                      "Task hasn't been started"));
    return;
  }

  if (!task_state_->HasTab()) {
    VLOG(1) << "Unable to perform action: tab has been destroyed";
    PostTaskForActCallback(std::move(callback),
                           MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  // NOTE: Improve this API by queuing the action instead.
  if (task_state_->HasAction()) {
    VLOG(1) << "Unable to perform action: task already has action in progress";
    PostTaskForActCallback(std::move(callback),
                           MakeResult(mojom::ActionResultCode::kError,
                                      "Task already has action in progress"));

    return;
  }

  task_state_->current_action.emplace(action, std::move(callback));

  content::WebContents& web_contents = *task_state_->tab->GetContents();

  MayActOnTab(
      *task_state_->tab,
      base::BindOnce(
          &ActorCoordinator::OnMayActOnTabResponse, GetWeakPtr(),
          task_state_->id,
          web_contents.GetPrimaryMainFrame()->GetLastCommittedOrigin()));
}

void ActorCoordinator::TryStartNewTask(
    const BrowserAction& action,
    StartTaskCallback callback,
    std::optional<tabs::TabHandle> tab_handle) {
  // Check for a failed attempt to initialize a new task, where there is a new
  // tab observer but it's navigation handle is no longer valid. The expectation
  // is that new tab observer will be notified regardless if the navigation
  // succeeded, failed, or the affected web contents is destroyed.
  CHECK(!initializing_new_task_ || !new_tab_web_contents_observer_ ||
        new_tab_web_contents_observer_->IsNavigationHandleValid());

  // Only a single task, in a single tab, allowed at a time. This includes when
  // initialization of a new task in progress (i.e. creating a new tab).
  if (initializing_new_task_ || task_state_) {
    VLOG(1) << "Cannot start new task: task already in progress";
    PostTaskForStartCallback(std::move(callback), /*tab=*/nullptr);
    return;
  }

  // Ensure that a navigate action was provided.
  //   - Currently, only one action at a time is supported.
  if (action.action_information_size() != 1 ||
      action.action_information().at(0).action_info_case() !=
          ActionInformation::kNavigate) {
    VLOG(1) << "Cannot start new task: first action was not kNavigate";
    PostTaskForStartCallback(std::move(callback), /*tab=*/nullptr);
    return;
  }

  initializing_new_task_ = true;

  // If a tab handle was provided, try to get the tab interface
  if (tab_handle) {
    if (auto* tab_interface = tab_handle->Get()) {
      // Create a new task with the existing tab
      task_state_ = std::make_unique<Task>(*tab_interface);
      initializing_new_task_ = false;
      PostTaskForStartCallback(std::move(callback), task_state_->tab);
      return;
    }
    // If we couldn't get the tab interface, error out
    VLOG(1) << "Could not get tab interface for handle";
    PostTaskForStartInitializationFailed(std::move(callback));
    return;
  }

  // If no tab handle was provided, create a new tab
  VLOG(1) << "No tab handle provided, creating new tab";
  CreateNewTab(std::move(callback));
}

void ActorCoordinator::PostTaskForStartInitializationFailed(
    ActorCoordinator::StartTaskCallback callback) {
  initializing_new_task_ = false;
  PostTaskForStartCallback(std::move(callback), nullptr);
}

void ActorCoordinator::CreateNewTab(StartTaskCallback callback) {
  NavigateParams params(profile_, GURL(url::kAboutBlankURL),
                        ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
  // Only supports foreground execution.
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;

  base::WeakPtr<content::NavigationHandle> navigation_handle =
      Navigate(&params);
  if (!navigation_handle) {
    VLOG(1) << "Cannot start new task: unable to open about:blank";
    PostTaskForStartInitializationFailed(std::move(callback));
    return;
  }

  // `base::Unretained(this)` is safe as `this` owns
  // `new_tab_web_contents_observer_`.
  base::OnceCallback<void(content::WebContents*)> created_callback =
      base::BindOnce(&ActorCoordinator::OnNewTabCreated, base::Unretained(this),
                     std::move(callback));

  new_tab_web_contents_observer_ = std::make_unique<NewTabWebContentsObserver>(
      navigation_handle, std::move(created_callback));
}

void ActorCoordinator::OnNewTabCreated(StartTaskCallback callback,
                                       content::WebContents* web_contents) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  CHECK(initializing_new_task_);
  initializing_new_task_ = false;
  new_tab_web_contents_observer_.reset();

  if (!web_contents) {
    VLOG(1) << "Unable to start new task: failed to create tab";
    std::move(callback).Run(/*tab=*/nullptr);
    return;
  }

  VLOG(1) << "Started new task";
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
  task_state_ = std::make_unique<Task>(*tab);
  std::move(callback).Run(tab->GetWeakPtr());
}

void ActorCoordinator::OnMayActOnTabResponse(
    TaskId task_id,
    const url::Origin& evaluated_origin,
    bool may_act) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!task_state_ || task_state_->id != task_id) {
    // The task for this check has been stopped already so just discard the
    // result.
    return;
  }

  CHECK(task_state_->HasAction());

  if (!task_state_->HasTab()) {
    VLOG(1)
        << "Unable to perform action: Tab closed while checking site policy";
    CompleteAction(MakeResult(mojom::ActionResultCode::kTabWentAway,
                              "Tab closed while checking site policy"));
    return;
  }

  if (!evaluated_origin.IsSameOriginWith(task_state_->tab->GetContents()
                                             ->GetPrimaryMainFrame()
                                             ->GetLastCommittedOrigin())) {
    // A cross-origin navigation occurred before we got permission. The result
    // is no longer applicable. For now just fail.
    // TODO(mcnee): Handle this gracefully.
    NOTIMPLEMENTED() << "Acting after cross-origin navigation occurred";
    CompleteAction(MakeResult(mojom::ActionResultCode::kCrossOriginNavigation,
                              "Acting after cross-origin navigation occurred"));
    return;
  }

  if (!may_act) {
    CompleteAction(MakeResult(mojom::ActionResultCode::kUrlBlocked,
                              "URL blocked for actions"));
    return;
  }

  BrowserAction& proto = task_state_->current_action->proto;

  // Currently, only one action at a time is supported.
  if (proto.action_information_size() != 1) {
    NOTIMPLEMENTED() << "Multi-action BrowserAction";
    CompleteAction(MakeResult(mojom::ActionResultCode::kError,
                              "Multiple actions are not supported"));
    return;
  }

  ToolInvocation invocation(proto.action_information().at(0),
                            *task_state_->tab);
  task_state_->tool_controller.Invoke(
      invocation,
      base::BindOnce(&ActorCoordinator::CompleteAction, GetWeakPtr()));
}

void ActorCoordinator::CompleteAction(mojom::ActionResultPtr result) {
  if (!task_state_ || !task_state_->HasAction()) {
    return;
  }

  PostTaskForActCallback(std::move(task_state_->current_action->callback),
                         std::move(result));
  task_state_->current_action.reset();
}

base::WeakPtr<ActorCoordinator> ActorCoordinator::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace actor
