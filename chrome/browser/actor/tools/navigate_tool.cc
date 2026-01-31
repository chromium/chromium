// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/navigate_tool.h"

#include "base/feature_list.h"
#include "chrome/browser/actor/actor_features.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/journal_details_builder.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::NavigationHandle;
using content::WebContents;
using tabs::TabInterface;

namespace actor {

namespace {

mojom::ActionResultPtr UrlCheckToActionResult(MayActOnUrlBlockReason reason) {
  return reason == MayActOnUrlBlockReason::kAllowed
             ? MakeOkResult()
             : MakeResult(mojom::ActionResultCode::kUrlBlocked);
}

}  // namespace

NavigateTool::NavigateTool(TaskId task_id,
                           ToolDelegate& tool_delegate,
                           TabInterface& tab,
                           const GURL& url)
    : Tool(task_id, tool_delegate),
      WebContentsObserver(tab.GetContents()),
      url_(url),
      tab_handle_(tab.GetHandle()) {}

NavigateTool::~NavigateTool() = default;

void NavigateTool::Validate(ToolCallback callback) {
  if (!url_.is_valid()) {
    // URL is invalid.
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kNavigateInvalidUrl));
    return;
  }

  tool_delegate().IsAcceptableNavigationDestination(
      url_, base::BindOnce(&UrlCheckToActionResult).Then(std::move(callback)));
}

void NavigateTool::Invoke(ToolCallback callback) {
  CHECK(web_contents());
  invoke_callback_ = std::move(callback);

  if (base::FeatureList::IsEnabled(kGlicNavigateUsingLoadURL)) {
    content::NavigationController::LoadURLParams params(url_);

    if (base::FeatureList::IsEnabled(kGlicNavigateToolUseOpaqueInitiator)) {
      params.initiator_origin = url::Origin();
    }

    params.transition_type = ::ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
    params.is_renderer_initiated = false;
    params.has_user_gesture = true;
    base::WeakPtr<content::NavigationHandle> handle =
        web_contents()->GetController().LoadURLWithParams(params);
    if (handle) {
      NavigationHandleCallback(*handle);
    } else {
      PostResponseTask(
          std::move(invoke_callback_),
          MakeResult(mojom::ActionResultCode::kNavigateFailedToStart));
    }
    return;
  }

  // TODO(b/460113906): Legacy code path - remove once the
  // NavigateUsingLoadURL path lands safely.
  content::OpenURLParams params(
      url_, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ::ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL,
      false /* is_renderer_initiated */);

  // TODO(b/460113906): Alternate to the NavigateUsingLoadURL path to fix for
  // this bug. Unfortunately, OpenURL has the side effect that a navigation
  // having a user gesture will force the navigating window to be activated.
  // Setting this to false fixes the issue but may have other consequences...
  if (base::FeatureList::IsEnabled(kGlicNavigateWithoutUserGesture)) {
    params.user_gesture = false;
  }

  // TODO(crbug.com/406545255): If the page has a BeforeUnload handler the user
  // may be prompted to confirm/abort the navigation, what should we do in those
  // cases?
  web_contents()->OpenURL(
      params, base::BindOnce(&NavigateTool::NavigationHandleCallback,
                             weak_ptr_factory_.GetWeakPtr()));
}

std::string NavigateTool::DebugString() const {
  return absl::StrFormat("NavigateTool[%s]", url_.spec());
}

std::string NavigateTool::JournalEvent() const {
  return "Navigate";
}

std::unique_ptr<ObservationDelayController> NavigateTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  return std::make_unique<ObservationDelayController>(
      *web_contents()->GetPrimaryMainFrame(), task_id(), journal(),
      page_stability_config);
}

void NavigateTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                          ToolCallback callback) const {
  task.AddTab(tab_handle_, std::move(callback));
}

tabs::TabHandle NavigateTool::GetTargetTab() const {
  return tab_handle_;
}

void NavigateTool::DidFinishNavigation(NavigationHandle* navigation_handle) {
  if (pending_navigation_handle_id_ &&
      navigation_handle->GetNavigationId() == *pending_navigation_handle_id_) {
    journal().Log(url_, task_id(), "NavigateTool::DidFinishNavigation",
                  JournalDetailsBuilder()
                      .Add("id", navigation_handle->GetNavigationId())
                      .Build());
    auto result =
        navigation_handle->HasCommitted() && !navigation_handle->IsErrorPage()
            ? MakeOkResult()
            : MakeResult(mojom::ActionResultCode::kNavigateCommittedErrorPage);

    if (invoke_callback_) {
      PostResponseTask(std::move(invoke_callback_), std::move(result));
      return;
    }
  }
}

void NavigateTool::NavigationHandleCallback(NavigationHandle& handle) {
  journal().Log(
      url_, task_id(), "NavigateTool::NavigationHandleCallback",
      JournalDetailsBuilder().Add("id", handle.GetNavigationId()).Build());
  pending_navigation_handle_id_ = handle.GetNavigationId();
}

}  // namespace actor
