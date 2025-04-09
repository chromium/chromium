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
#include "base/memory/weak_ptr.h"
#include "base/notimplemented.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/tools/tool_controller.h"
#include "chrome/browser/actor/tools/tool_invocation.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
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
                            bool success) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), success));
}

}  // namespace

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
                                 StartTaskCallback callback) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Try to start a new actor task, as only a single task is allowed at a time.
  // Posts to a sequence to avoid potential races with multiple attempts to
  // start a task.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ActorCoordinator::TryStartNewTask,
                                GetWeakPtr(), action, std::move(callback)));
}

void ActorCoordinator::StartTaskForTesting(tabs::TabInterface* tab) {
  CHECK(tab);
  CHECK(!task_tab_);
  task_tab_ = tab->GetWeakPtr();
}

void ActorCoordinator::Act(const BrowserAction& action,
                           ActionResultCallback callback) {
  CHECK(base::FeatureList::IsEnabled(features::kGlicActor));
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // StartTask must have been called to initialize the tab for action.
  if (!task_tab_) {
    PostTaskForActCallback(std::move(callback), /*success=*/false);
    return;
  }

  content::WebContents& web_contents = *task_tab_->GetContents();

  MayActOnTab(*task_tab_,
              base::BindOnce(
                  &ActorCoordinator::OnMayActOnTabResponse, GetWeakPtr(),
                  task_tab_->GetWeakPtr(), action,
                  web_contents.GetPrimaryMainFrame()->GetLastCommittedOrigin(),
                  std::move(callback)));
}

void ActorCoordinator::TryStartNewTask(const BrowserAction& action,
                                       StartTaskCallback callback) {
  // Check for a failed attempt to initialize a new task, where there is a new
  // tab observer but it's navigation handle is no longer valid. The expectation
  // is that new tab observer will be notified regardless if the navigation
  // succeeded, failed, or the affected web contents is destroyed.
  CHECK(!initializing_new_task_ || !new_tab_web_contents_observer_ ||
        new_tab_web_contents_observer_->IsNavigationHandleValid());

  // Only a single task, in a single tab, allowed at a time. This includes when
  // initialization of a new task in progress (i.e. creating a new tab).
  if (initializing_new_task_ || task_tab_) {
    PostTaskForStartCallback(std::move(callback), /*tab=*/nullptr);
    return;
  }

  // Ensure that a navigate action was provided.
  //   - Currently, only one action at a time is supported.
  if (action.action_information_size() != 1 ||
      action.action_information().at(0).action_info_case() !=
          ActionInformation::kNavigate) {
    PostTaskForStartCallback(std::move(callback), /*tab=*/nullptr);
    return;
  }

  initializing_new_task_ = true;

  // Force the task to be performed in a new tab.
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
    std::move(callback).Run(/*tab=*/nullptr);
    return;
  }

  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents);
  task_tab_ = tab->GetWeakPtr();
  std::move(callback).Run(tab->GetWeakPtr());
}

void ActorCoordinator::OnMayActOnTabResponse(
    base::WeakPtr<tabs::TabInterface> tab,
    const BrowserAction& action,
    const url::Origin& evaluated_origin,
    ActionResultCallback callback,
    bool may_act) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!tab) {
    PostTaskForActCallback(std::move(callback), /*success=*/false);
    return;
  }

  content::WebContents& web_contents = *tab->GetContents();

  if (!evaluated_origin.IsSameOriginWith(
          web_contents.GetPrimaryMainFrame()->GetLastCommittedOrigin())) {
    // A cross-origin navigation occurred before we got permission. The result
    // is no longer applicable. For now just fail.
    // TODO(mcnee): Handle this gracefully.
    NOTIMPLEMENTED();
    PostTaskForActCallback(std::move(callback), /*success=*/false);
    return;
  }

  if (!may_act) {
    PostTaskForActCallback(std::move(callback), /*success=*/false);
    return;
  }

  // Currently, only one action at a time is supported.
  if (action.action_information_size() != 1) {
    PostTaskForActCallback(std::move(callback), /*success=*/false);
    return;
  }
  ToolInvocation invocation(action.action_information().at(0), *tab);
  tool_controller_.Invoke(invocation, std::move(callback));
}

base::WeakPtr<ActorCoordinator> ActorCoordinator::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace actor
