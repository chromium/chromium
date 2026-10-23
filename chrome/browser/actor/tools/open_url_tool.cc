// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/open_url_tool.h"

#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tab_management_tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/actor/tools/validate_url_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace actor {

OpenUrlTool::OpenUrlTool(TaskId task_id,
                         ToolDelegate& tool_delegate,
                         const ActorSurface& actor_surface,
                         const GURL& url,
                         bool open_in_new_tab)
    : Tool(task_id, tool_delegate),
      url_(url),
      open_in_new_tab_(open_in_new_tab),
      actor_surface_handle_(actor_surface.GetHandle()) {}

OpenUrlTool::~OpenUrlTool() = default;

void OpenUrlTool::Validate(ToolCallback callback) {
  if (!open_in_new_tab_) {
    // The follow-up `NavigateTool` validates the URL.
    PostResponseTask(std::move(callback), MakeOkResult());
    return;
  }

  // The follow-up `NavigateTool` validates the URL again, but checking it here
  // avoids opening a new tab for a URL that can't be navigated to.
  ValidateUrlIsAcceptableNavigationDestination(url_, tool_delegate(),
                                               std::move(callback));
}

mojom::ActionResultPtr OpenUrlTool::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation) {
  if (!open_in_new_tab_) {
    return MakeOkResult();
  }
  return ValidateBrowserWindow(GetSurfaceBrowserWindow());
}

void OpenUrlTool::Invoke(ToolCallback callback) {
  if (!open_in_new_tab_) {
    EnqueueNavigation(actor_surface_handle_);
    PostResponseTask(std::move(callback),
                     MakeOkResult(/*requires_page_stabilization=*/false));
    return;
  }

  // Validated in `TimeOfUseValidation()`, which runs synchronously before this.
  BrowserWindowInterface* browser = GetSurfaceBrowserWindow();
  CHECK(browser);

  // Open a blank foreground tab; the navigation to `url_` is enqueued once
  // it's been created.
  // TODO(b/555808355): Replace this delegation once composite tools are
  // supported.
  create_tab_tool_ = std::make_unique<TabManagementTool>(
      task_id(), tool_delegate(), browser->GetSessionID().id(),
      WindowOpenDisposition::kNewForegroundTab);
  create_tab_tool_->Invoke(base::BindOnce(&OpenUrlTool::OnTabCreated,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          std::move(callback)));
}

void OpenUrlTool::UpdateTaskAfterInvoke(ActorTask& task,
                                        mojom::ActionResultPtr result,
                                        ToolCallback callback) const {
  if (!open_in_new_tab_) {
    std::move(callback).Run(std::move(result));
    return;
  }
  CHECK(!new_tab_surface_.is_null());
  // Note: This runs before the follow-up navigation action starts.
  task.AddActorSurface(new_tab_surface_, /*stop_task_on_detach=*/true,
                       std::move(callback));
}

void OpenUrlTool::OnTabCreated(ToolCallback callback,
                               mojom::ActionResultPtr result) {
  if (!IsOk(*result)) {
    std::move(callback).Run(std::move(result));
    return;
  }

  new_tab_surface_ = create_tab_tool_->GetTargetActorSurface();
  CHECK(!new_tab_surface_.is_null());

  EnqueueNavigation(new_tab_surface_);
  std::move(callback).Run(MakeOkResult(/*requires_page_stabilization=*/false));
}

void OpenUrlTool::EnqueueNavigation(ActorSurfaceHandle target) {
  // The navigation runs as a follow-up action, so it goes through the full
  // tool pipeline (target validation, URL validation, and its own observation
  // delay) and its result replaces this tool's result.
  tool_delegate().EnqueueFollowupAction(
      std::make_unique<NavigateToolRequest>(target, url_));
}

std::string OpenUrlTool::DebugString() const {
  return absl::StrFormat("OpenUrlTool[%s, new_tab=%v]", url_.spec(),
                         open_in_new_tab_);
}

std::string OpenUrlTool::JournalEvent() const {
  return "OpenUrl";
}

std::unique_ptr<ObservationDelayController> OpenUrlTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  // The follow-up `NavigateTool` waits for the page to settle.
  return nullptr;
}

ActorSurfaceHandle OpenUrlTool::GetTargetActorSurface() const {
  // Until the new tab is created, report the source tab so the target stays
  // non-null and is still validated.
  return new_tab_surface_.is_null() ? actor_surface_handle_ : new_tab_surface_;
}

BrowserWindowInterface* OpenUrlTool::GetSurfaceBrowserWindow() {
  tabs::TabInterface* tab = actor_surface_handle_.GetTabHandle().Get();
  return tab ? tab->GetBrowserWindowInterface() : nullptr;
}

}  // namespace actor
