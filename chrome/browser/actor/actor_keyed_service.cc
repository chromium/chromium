// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_keyed_service.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/browser_action_util.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/ui/actor_ui_state_manager.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"

namespace {

void RunLater(base::OnceClosure task) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                              std::move(task));
}

}  // namespace

namespace actor {

using ui::ActorUiStateManagerInterface;

ActorKeyedService::ActorKeyedService(Profile* profile) : profile_(profile) {
  actor_ui_state_manager_ = std::make_unique<ui::ActorUiStateManager>(*this);
}

ActorKeyedService::~ActorKeyedService() = default;

// static
ActorKeyedService* ActorKeyedService::Get(content::BrowserContext* context) {
  return ActorKeyedServiceFactory::GetActorKeyedService(context);
}

void ActorKeyedService::SetActorUiStateManagerForTesting(
    std::unique_ptr<ui::ActorUiStateManagerInterface> ausm) {
  actor_ui_state_manager_ = std::move(ausm);
}

TaskId ActorKeyedService::AddActiveTask(std::unique_ptr<ActorTask> task) {
  TaskId task_id = next_task_id_.GenerateNextId();
  last_created_task_id_ = task_id;
  task->SetId(base::PassKey<ActorKeyedService>(), task_id);
  task->GetExecutionEngine()->SetOwner(task.get());
  active_tasks_[task_id] = std::move(task);
  return task_id;
}

const std::map<TaskId, const ActorTask*> ActorKeyedService::GetActiveTasks()
    const {
  std::map<TaskId, const ActorTask*> active_tasks;
  for (const auto& [id, task] : active_tasks_) {
    CHECK_NE(task->GetState(), actor::ActorTask::State::kFinished);
    active_tasks[id] = task.get();
  }
  return active_tasks;
}

const std::map<TaskId, const ActorTask*> ActorKeyedService::GetInactiveTasks()
    const {
  std::map<TaskId, const ActorTask*> inactive_tasks;
  for (const auto& [id, task] : inactive_tasks_) {
    inactive_tasks[id] = task.get();
  }
  return inactive_tasks;
}

void ActorKeyedService::ResetForTesting() {
  for (auto it = active_tasks_.begin(); it != active_tasks_.end();) {
    StopTask((it++)->first);
  }
  active_tasks_.clear();
  inactive_tasks_.clear();
}

void ActorKeyedService::ExecuteAction(
    TaskId task_id,
    std::vector<std::unique_ptr<ToolRequest>>&& actions,
    base::OnceCallback<void(optimization_guide::proto::BrowserActionResult)>
        callback) {
  auto* task = GetTask(task_id);
  if (!task) {
    VLOG(1) << "Execute Action failed: Task not found.";
    optimization_guide::proto::BrowserActionResult result;
    result.set_action_result(0);
    RunLater(base::BindOnce(std::move(callback), std::move(result)));
    return;
  }
  task->Act(std::move(actions),
            base::BindOnce(&ActorKeyedService::OnActionFinished,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           task_id.value()));
}

TaskId ActorKeyedService::CreateTask() {
  auto execution_engine = std::make_unique<ExecutionEngine>(profile_.get());
  auto actor_task =
      std::make_unique<ActorTask>(profile_.get(), std::move(execution_engine));
  return AddActiveTask(std::move(actor_task));
}

void ActorKeyedService::RequestTabObservation(
    const tabs::TabInterface& tab,
    base::OnceCallback<void(TabObservationResult)> callback) {
  page_content_annotations::FetchPageContextOptions options;
  options.include_viewport_screenshot = true;
  options.annotated_page_content_options =
      optimization_guide::ActionableAIPageContentOptions();
  page_content_annotations::FetchPageContext(
      *tab.GetContents(), options,
      base::BindOnce(&ActorKeyedService::OnTabOservationResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ActorKeyedService::OnTabOservationResult(
    base::OnceCallback<void(TabObservationResult)> callback,
    page_content_annotations::FetchPageContextResultCallbackArg fetch_result) {
  if (!fetch_result.has_value()) {
    std::move(callback).Run(base::unexpected(fetch_result.error()));
    return;
  }
  auto& page_context = **fetch_result;
  if (!page_context.screenshot_result.has_value() ||
      !page_context.annotated_page_content_result.has_value()) {
    std::move(callback).Run(
        base::unexpected<std::string>("Failed Observation"));
    return;
  }
  std::unique_ptr<optimization_guide::proto::TabObservation> result =
      std::make_unique<optimization_guide::proto::TabObservation>();
  if (page_context.screenshot_result) {
    auto& data = page_context.screenshot_result->jpeg_data;
    if (data.size() != 0) {
      result->set_screenshot_mime_type("image/jpeg");
      result->set_screenshot(data.data(), data.size());
    }
  }
  if (page_context.annotated_page_content_result) {
    result->mutable_annotated_page_content()->Swap(
        &page_context.annotated_page_content_result->proto);
  }
  std::move(callback).Run(std::move(result));
}

void ActorKeyedService::ConvertToBrowserActionResult(
    base::OnceCallback<void(optimization_guide::proto::BrowserActionResult)>
        callback,
    int task_id,
    int32_t tab_id,
    actor::mojom::ActionResultPtr action_result,
    base::expected<std::unique_ptr<optimization_guide::proto::TabObservation>,
                   std::string> context_result) {
  optimization_guide::proto::BrowserActionResult browser_action_result;
  if (!context_result.has_value()) {
    VLOG(1) << "Execute Action failed: Error fetching context.";
    browser_action_result.set_action_result(0);
    RunLater(
        base::BindOnce(std::move(callback), std::move(browser_action_result)));
    return;
  }
  auto& tab_observation = **context_result;
  if (tab_observation.has_annotated_page_content()) {
    browser_action_result.mutable_annotated_page_content()->Swap(
        tab_observation.mutable_annotated_page_content());
  }
  if (tab_observation.has_screenshot()) {
    browser_action_result.set_screenshot(tab_observation.screenshot().data(),
                                         tab_observation.screenshot().size());
    browser_action_result.set_screenshot_mime_type(
        tab_observation.screenshot_mime_type());
  }
  browser_action_result.set_task_id(task_id);
  browser_action_result.set_tab_id(tab_id);
  browser_action_result.set_action_result(actor::IsOk(*action_result) ? 1 : 0);
  RunLater(
      base::BindOnce(std::move(callback), std::move(browser_action_result)));
}

void ActorKeyedService::OnActionFinished(
    base::OnceCallback<void(optimization_guide::proto::BrowserActionResult)>
        callback,
    int task_id,
    actor::mojom::ActionResultPtr action_result,
    std::optional<size_t> index_of_failed_action) {
  auto* task = GetTask(actor::TaskId(task_id));
  CHECK(task);
  tabs::TabInterface* tab = task->GetTabForObservation();
  if (!tab) {
    VLOG(1) << "Execute Action failed: Tab not found.";
    optimization_guide::proto::BrowserActionResult result;
    result.set_action_result(0);
    RunLater(base::BindOnce(std::move(callback), std::move(result)));
    return;
  }
  int32_t tab_id = tab->GetHandle().raw_value();
  RequestTabObservation(
      *tab, base::BindOnce(&ActorKeyedService::ConvertToBrowserActionResult,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           task_id, tab_id, std::move(action_result)));
}

void ActorKeyedService::PerformActions(
    TaskId task_id,
    std::vector<std::unique_ptr<ToolRequest>>&& actions,
    PerformActionsCallback callback) {
  auto* task = GetTask(task_id);
  if (!task) {
    VLOG(1) << "PerformActions failed: Task not found.";
    RunLater(base::BindOnce(std::move(callback),
                            mojom::ActionResultCode::kTaskWentAway,
                            std::nullopt));
    return;
  }

  if (actions.empty()) {
    VLOG(1) << "PerformActions failed: No actions provided.";
    RunLater(base::BindOnce(std::move(callback),
                            mojom::ActionResultCode::kEmptyActionSequence,
                            std::nullopt));
    return;
  }

  task->Act(
      std::move(actions),
      base::BindOnce(&ActorKeyedService::OnActionsFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ActorKeyedService::OnActionsFinished(
    PerformActionsCallback callback,
    mojom::ActionResultPtr result,
    std::optional<size_t> index_of_failed_action) {
  // If the result if Ok then we must not have a failed action.
  CHECK(!IsOk(*result) || !index_of_failed_action);
  RunLater(base::BindOnce(std::move(callback), result->code,
                          index_of_failed_action));
}

void ActorKeyedService::StopTask(TaskId task_id) {
  if (task_id == last_created_task_id_) {
    last_created_task_id_ = TaskId();
  }

  auto task = active_tasks_.extract(task_id);
  if (!task.empty()) {
    auto ret = inactive_tasks_.insert(std::move(task));
    ret.position->second->Stop();
  }
}

ActorTask* ActorKeyedService::GetTask(TaskId task_id) {
  auto task = active_tasks_.find(task_id);
  if (task != active_tasks_.end()) {
    return task->second.get();
  }
  task = inactive_tasks_.find(task_id);
  if (task != inactive_tasks_.end()) {
    return task->second.get();
  }
  return nullptr;
}

ActorTask* ActorKeyedService::GetMostRecentTask() {
  return GetTask(last_created_task_id_);
}

ActorUiStateManagerInterface* ActorKeyedService::GetActorUiStateManager() {
  return actor_ui_state_manager_.get();
}

bool ActorKeyedService::IsAnyTaskActingOnTab(
    const tabs::TabInterface& tab) const {
  tabs::TabHandle handle = tab.GetHandle();
  for (auto task_pair : GetActiveTasks()) {
    if (task_pair.second->HasActedOnTab(handle)) {
      return true;
    }
  }

  return false;
}

Profile* ActorKeyedService::GetProfile() {
  return profile_;
}

}  // namespace actor
