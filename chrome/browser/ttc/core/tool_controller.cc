// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/core/tool_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/actor/tab_observation_strategy.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/core/ttc_actor_ui_state_manager.h"
#include "chrome/browser/ttc/core/ttc_keyed_service.h"
#include "chrome/common/actor/action_result.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "url/gurl.h"
#endif

namespace ttc {

ToolController::ToolController(TtcKeyedService& service) : service_(service) {}

ToolController::~ToolController() {
  if (!task_id_.is_null()) {
    auto* actor_service = actor::ActorKeyedService::Get(GetProfile());
    CHECK(actor_service);
    actor_service->StopTask(task_id_,
                            actor::ActorTask::StoppedReason::kTaskComplete);
  }
}

void ToolController::ProcessToolCall(const ToolRequest& tool_request,
                                     ToolResponseCallback callback) {
#if !BUILDFLAG(IS_ANDROID)
  if (tool_request.name == "open_url") {
    OpenUrl(tool_request.arguments, std::move(callback));
    return;
  }
#endif

  std::move(callback).Run(ToolResponse::Error(
      actor::mojom::ActionResultCode::kToolUnknown, "Unsupported tool"));
}

std::vector<ToolDefinition> ToolController::GetToolDefinitions() {
  std::vector<ToolDefinition> tools;

#if !BUILDFLAG(IS_ANDROID)
  ToolDefinition open_url;
  open_url.name = "open_url";
  open_url.description = "Opens a URL in the browser.";
  open_url.parameters_json_schema =
      base::DictValue()
          .Set("type", "object")
          .Set("properties",
               base::DictValue()
                   .Set("url", base::DictValue()
                                   .Set("type", "string")
                                   .Set("description",
                                        "The complete URL to open (e.g. "
                                        "\"https://example.com\")."))
                   .Set("new_tab",
                        base::DictValue()
                            .Set("type", "boolean")
                            .Set("description",
                                 "If true, opens the URL in a new tab; "
                                 "otherwise, navigates the current tab.")))
          .Set("required", base::ListValue().Append("url").Append("new_tab"));
  open_url.behavior = ToolDefinition::Behavior::kBlocking;
  open_url.verbalization = ToolDefinition::Verbalization::kSilentAction;
  tools.push_back(std::move(open_url));
#endif

  return tools;
}

Profile* ToolController::GetProfile() {
  return service_->profile();
}

void ToolController::EnsureTaskCreated(
    actor::ActorKeyedService* actor_service) {
  // TODO(b/552544497): Ideally the task could only be stopped by `this`, but
  // there are currently a few ways for tasks to be stopped outside of this
  // class.
  if (!task_id_.is_null() && actor_service->GetTask(task_id_)) {
    return;
  }

  // TODO(b/544821996): Provide an ActorTaskDelegate.
  task_id_ = actor_service->CreateTaskWithOptions(
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kTtc, "ttc"),
      actor::GetNullEnterprisePolicyChecker(), /*options=*/nullptr,
      /*delegate=*/nullptr, &service_->actor_ui_state_manager());
}

#if !BUILDFLAG(IS_ANDROID)
void ToolController::OpenUrl(const base::DictValue& arguments,
                             ToolResponseCallback callback) {
  const std::string* url = arguments.FindString("url");
  if (!url) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kArgumentsInvalid,
                            "Missing url argument"));
    return;
  }

  bool new_tab = arguments.FindBool("new_tab").value_or(false);

  // TODO(b/544823467): Add support for opening in a new tab.
  if (new_tab) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kNotImplemented,
                            "New tab not supported yet"));
    return;
  }

  // TODO(b/561651267): Get BrowserWindowInterface* from SessionControllerImpl
  // (or a class that manages the active window for the session).
  BrowserWindowInterface* browser = nullptr;
  if (auto* collection =
          ProfileBrowserCollection::GetForProfile(GetProfile())) {
    browser = collection->GetLastActiveBrowser();
  }
  if (!browser) {
    std::move(callback).Run(
        ToolResponse::Error(actor::mojom::ActionResultCode::kWindowWentAway,
                            "No active browser window"));
    return;
  }

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(GetProfile());
  if (!actor_service) {
    // No actor error code describes an unavailable ActorKeyedService.
    std::move(callback).Run(ToolResponse::Error("Something went wrong"));
    return;
  }

  EnsureTaskCreated(actor_service);

  tabs::TabInterface* active_tab = browser->GetTabStripModel()->GetActiveTab();
  if (!active_tab) {
    std::move(callback).Run(ToolResponse::Error(
        actor::mojom::ActionResultCode::kTabWentAway, "No active tab"));
    return;
  }
  std::vector<std::unique_ptr<actor::ToolRequest>> actions;
  actions.push_back(std::make_unique<actor::NavigateToolRequest>(
      active_tab->GetHandle(), GURL(*url)));

  actor_service->PerformActions(
      task_id_, std::move(actions), actor::ActorTaskMetadata(),
      base::BindOnce(&ToolController::OnNavigateActionsFinished,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ToolController::OnNavigateActionsFinished(
    ToolResponseCallback callback,
    std::vector<actor::ActionResultWithLatencyInfo> results,
    actor::TabObservationStrategy strategy) {
  CHECK(!results.empty());

  const actor::mojom::ActionResult& result = *results[0].result;
  if (!actor::IsOk(result)) {
    std::move(callback).Run(ToolResponse::Error(result.code, result.message));
    return;
  }
  std::move(callback).Run(ToolResponse::Success());
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace ttc
