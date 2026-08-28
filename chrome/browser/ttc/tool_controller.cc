// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ttc/tool_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/actor/actor_task_metadata.h"
#include "chrome/browser/actor/enterprise_policy_checker.h"
#include "chrome/browser/actor/tab_observation_strategy.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/actor/action_result.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "url/gurl.h"
#else
#include "base/notimplemented.h"
#endif

namespace ttc {

ToolController::ToolController(Profile* profile) : profile_(profile) {}

ToolController::~ToolController() {
  if (!task_id_.is_null()) {
    auto* actor_service = actor::ActorKeyedService::Get(profile_);
    CHECK(actor_service);
    actor_service->StopTask(task_id_,
                            actor::ActorTask::StoppedReason::kTaskComplete);
  }
}

void ToolController::OpenUrl(BrowserWindowInterface* browser,
                             const std::string& url_string,
                             bool new_tab,
                             OpenUrlCallback callback) {
#if !BUILDFLAG(IS_ANDROID)
  // TODO(b/544823467): Add support for opening in a new tab.
  if (new_tab) {
    std::move(callback).Run(base::unexpected("New tab not supported yet"));
    return;
  }

  actor::ActorKeyedService* actor_service =
      actor::ActorKeyedService::Get(profile_);
  if (!actor_service) {
    std::move(callback).Run(base::unexpected("Something went wrong"));
    return;
  }

  EnsureTaskCreated(actor_service);

  tabs::TabInterface* active_tab = browser->GetTabStripModel()->GetActiveTab();
  if (!active_tab) {
    std::move(callback).Run(base::unexpected("No active tab"));
    return;
  }
  std::vector<std::unique_ptr<actor::ToolRequest>> actions;
  actions.push_back(std::make_unique<actor::NavigateToolRequest>(
      active_tab->GetHandle(), GURL(url_string)));

  actor_service->PerformActions(
      task_id_, std::move(actions), actor::ActorTaskMetadata(),
      base::BindOnce(&ToolController::OnNavigateActionsFinished,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
#else
  NOTIMPLEMENTED();
  std::move(callback).Run(base::unexpected("Not supported"));
#endif
}

void ToolController::EnsureTaskCreated(
    actor::ActorKeyedService* actor_service) {
  // TODO(b/552544497): Ideally the task could only be stopped by `this`, but
  // there are currently a few ways for tasks to be stopped outside of this
  // class.
  if (!task_id_.is_null() && actor_service->GetTask(task_id_)) {
    return;
  }

  // TODO(b/544821996): Create and use a new Client enum value.
  // TODO(b/544821996): Provide an ActorTaskDelegate.
  task_id_ = actor_service->CreateTask(
      actor::TaskSourceInfo(actor::TaskSourceInfo::Client::kExperimentalActor,
                            "ai_overlay_dialog"),
      actor::GetNullEnterprisePolicyChecker());
}

void ToolController::OnNavigateActionsFinished(
    OpenUrlCallback callback,
    std::vector<actor::ActionResultWithLatencyInfo> results,
    actor::TabObservationStrategy strategy) {
  CHECK(!results.empty());

  if (actor::IsOk(*results[0].result)) {
    std::move(callback).Run(std::monostate());
  } else {
    std::string error_message = results[0].result->message;
    if (error_message.empty()) {
      error_message =
          "Action failed with code: " +
          base::NumberToString(static_cast<int>(results[0].result->code));
    }
    std::move(callback).Run(base::unexpected(error_message));
  }
}

}  // namespace ttc
