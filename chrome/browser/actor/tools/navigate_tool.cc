// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/navigate_tool.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/site_policy.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tab_management_tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/actor/tools/validate_url_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/core/journal_details_builder.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::NavigationHandle;
using content::WebContents;

namespace actor {

NavigateTool::NavigateTool(TaskId task_id,
                           ToolDelegate& tool_delegate,
                           ActorSurface& actor_surface,
                           const GURL& url)
    : Tool(task_id, tool_delegate),
      WebContentsObserver(actor_surface.GetWebContents()),
      url_(url),
      actor_surface_handle_(actor_surface.GetHandle()) {}

NavigateTool::NavigateTool(TaskId task_id,
                           ToolDelegate& tool_delegate,
                           SessionID window_id,
                           const GURL& url)
    : Tool(task_id, tool_delegate), url_(url), window_id_(window_id) {}

NavigateTool::~NavigateTool() = default;

void NavigateTool::Validate(ToolCallback callback) {
  ValidateUrlIsAcceptableNavigationDestination(url_, tool_delegate(),
                                               std::move(callback));
}

mojom::ActionResultPtr NavigateTool::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation) {
  if (window_id_.has_value()) {
    return ValidateWindowId(*window_id_);
  }
  return MakeOkResult();
}

void NavigateTool::Invoke(ToolCallback callback) {
  if (window_id_.has_value()) {
    create_tab_tool_ = std::make_unique<TabManagementTool>(
        task_id(), tool_delegate(), window_id_->id(),
        ::ui::mojom::WindowOpenDisposition::kNewForegroundTab);
    create_tab_tool_->Invoke(base::BindOnce(&NavigateTool::OnTabCreated,
                                            weak_ptr_factory_.GetWeakPtr(),
                                            std::move(callback)));
    return;
  }

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
  content::OpenURLParams params =
      content::OpenURLParams::CreateBrowserInitiated(
          url_, ::ui::mojom::WindowOpenDisposition::kCurrentTab,
          ::ui::PageTransition::PAGE_TRANSITION_AUTO_TOPLEVEL);

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

void NavigateTool::UpdateTaskAfterInvoke(ActorTask& task,
                                         mojom::ActionResultPtr result,
                                         ToolCallback callback) const {
  if (!window_id_.has_value() || !IsOk(*result)) {
    Tool::UpdateTaskAfterInvoke(task, std::move(result), std::move(callback));
    return;
  }
  CHECK(!actor_surface_handle_.is_null());
  // Note: This runs before the follow-up navigation action starts.
  task.AddActorSurface(actor_surface_handle_, /*stop_task_on_detach=*/true,
                       std::move(callback));
}

void NavigateTool::OnTabCreated(ToolCallback callback,
                                mojom::ActionResultPtr result) {
  if (!IsOk(*result)) {
    std::move(callback).Run(std::move(result));
    return;
  }

  actor_surface_handle_ = create_tab_tool_->GetTargetActorSurface();
  CHECK(!actor_surface_handle_.is_null());

  tool_delegate().EnqueueFollowupAction(
      std::make_unique<NavigateToolRequest>(actor_surface_handle_, url_));
  std::move(callback).Run(MakeOkResult(/*requires_page_stabilization=*/false));
}

std::string NavigateTool::DebugString() const {
  return absl::StrFormat("NavigateTool[%s]", url_.spec());
}

std::string NavigateTool::JournalEvent() const {
  return "Navigate";
}

std::unique_ptr<ObservationDelayController> NavigateTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  if (window_id_.has_value()) {
    // The follow-up `NavigateTool` waits for the page to settle.
    return nullptr;
  }
  return std::make_unique<ObservationDelayController>(
      *web_contents()->GetPrimaryMainFrame(), task_id(), journal(),
      page_stability_config);
}

ActorSurfaceHandle NavigateTool::GetTargetActorSurface() const {
  return actor_surface_handle_;
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
