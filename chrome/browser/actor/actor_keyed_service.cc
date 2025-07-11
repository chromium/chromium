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
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#endif

namespace {

// TODO(crbug.com/411462297): This is a short term hack. This code will be
// deleted soon once StartTask stops creating new tabs implicitly. This adds a
// 1-second delay to wait for about:blank to load. This can be replaced by ~100
// lines of complex code that tries to precisely wait for navigation commit, but
// that would be overkill.
constexpr base::TimeDelta kDelayForNewTab = base::Seconds(1);

#if BUILDFLAG(ENABLE_GLIC)
glic::mojom::GetTabContextOptions DefaultOptions() {
  glic::mojom::GetTabContextOptions options;
  options.include_annotated_page_content = true;
  options.include_viewport_screenshot = true;
  options.annotated_page_content_mode = optimization_guide::proto::
      ANNOTATED_PAGE_CONTENT_MODE_ACTIONABLE_ELEMENTS;
  return options;
}

#endif

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
  bool always_fail = false;
#if !BUILDFLAG(ENABLE_GLIC)
  // The current implementation relies on glic::FetchPageContext().
  always_fail = true;
#endif
  if (!task || always_fail) {
    VLOG(1) << "Execute Action failed: Task not found.";
    optimization_guide::proto::BrowserActionResult result;
    result.set_action_result(0);
    RunLater(base::BindOnce(std::move(callback), std::move(result)));
    return;
  }
#if BUILDFLAG(ENABLE_GLIC)
  task->Act(std::move(actions),
            base::BindOnce(&ActorKeyedService::OnActionFinished,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           task_id.value()));
#endif
}

TaskId ActorKeyedService::CreateTask() {
  auto execution_engine = std::make_unique<ExecutionEngine>(profile_.get());
  auto actor_task =
      std::make_unique<ActorTask>(profile_.get(), std::move(execution_engine));
  TaskId task_id = AddActiveTask(std::move(actor_task));
  actor_task_subscriptions_.emplace(
      task_id, GetTask(task_id)->RegisterTaskStateChange(base::BindRepeating(
                   &ActorKeyedService::OnActorTaskStateChanged,
                   weak_ptr_factory_.GetWeakPtr())));
  return task_id;
}

void ActorKeyedService::StartTask(
    optimization_guide::proto::BrowserStartTask task,
    base::OnceCallback<void(optimization_guide::proto::BrowserStartTaskResult)>
        callback) {
  // TODO(crbug.com/411462297): This is a short term hack. This code will be
  // deleted soon once tab_id is removed.
  tabs::TabHandle handle(task.tab_id());
  if (!task.tab_id()) {
    // Get the most recently active browser for this profile.
    Browser* browser =
        chrome::FindTabbedBrowser(profile_, /*match_original_profiles=*/false);
    // If no browser exists create one.
    if (!browser) {
      browser = Browser::Create(
          Browser::CreateParams(profile_, /*user_gesture=*/false));
    }
    // Create a new tab.
    browser->OpenGURL(GURL(url::kAboutBlankURL),
                      WindowOpenDisposition::NEW_FOREGROUND_TAB);
    handle = browser->GetActiveTabInterface()->GetHandle();
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&ActorKeyedService::FinishStartTask,
                       weak_ptr_factory_.GetWeakPtr(), handle,
                       std::move(callback)),
        kDelayForNewTab);
    return;
  }

  FinishStartTask(handle, std::move(callback));
}

void ActorKeyedService::FinishStartTask(
    tabs::TabHandle handle,
    base::OnceCallback<void(optimization_guide::proto::BrowserStartTaskResult)>
        callback) {
  tabs::TabInterface* tab = handle.Get();
  std::unique_ptr<actor::ExecutionEngine> execution_engine;
  if (tab) {
    execution_engine =
        std::make_unique<actor::ExecutionEngine>(profile_.get(), tab);
  } else {
    execution_engine = std::make_unique<actor::ExecutionEngine>(profile_.get());
  }

  auto actor_task = std::make_unique<actor::ActorTask>(
      profile_.get(), std::move(execution_engine));
  actor::TaskId task_id = AddActiveTask(std::move(actor_task));
  actor_task_subscriptions_.emplace(
      task_id, GetTask(task_id)->RegisterTaskStateChange(base::BindRepeating(
                   &ActorKeyedService::OnActorTaskStateChanged,
                   weak_ptr_factory_.GetWeakPtr())));

  optimization_guide::proto::BrowserStartTaskResult result;
  result.set_task_id(task_id.value());
  result.set_tab_id(handle.raw_value());
  result.set_status(optimization_guide::proto::BrowserStartTaskResult::SUCCESS);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
}

#if BUILDFLAG(ENABLE_GLIC)
void ActorKeyedService::ConvertToBrowserActionResult(
    base::OnceCallback<void(optimization_guide::proto::BrowserActionResult)>
        callback,
    int task_id,
    int32_t tab_id,
    actor::mojom::ActionResultPtr action_result,
    glic::mojom::GetContextResultPtr context_result) {
  optimization_guide::proto::BrowserActionResult browser_action_result;
  if (context_result->is_error_reason()) {
    VLOG(1) << "Execute Action failed: Error fetching context.";
    browser_action_result.set_action_result(0);
    RunLater(
        base::BindOnce(std::move(callback), std::move(browser_action_result)));
    return;
  }
  if (context_result->get_tab_context() &&
      context_result->get_tab_context()->annotated_page_data &&
      context_result->get_tab_context()
          ->annotated_page_data->annotated_page_content) {
    auto apc = context_result->get_tab_context()
                   ->annotated_page_data->annotated_page_content.value()
                   .As<optimization_guide::proto::AnnotatedPageContent>();
    if (apc.has_value()) {
      auto apc_value = *std::move(apc);
      browser_action_result.mutable_annotated_page_content()->Swap(&apc_value);
    }
  }
  if (context_result->get_tab_context()->viewport_screenshot &&
      context_result->get_tab_context()->viewport_screenshot->data.size() !=
          0) {
    auto& data = context_result->get_tab_context()->viewport_screenshot->data;
    browser_action_result.set_screenshot(data.data(), data.size());
    browser_action_result.set_screenshot_mime_type(
        context_result->get_tab_context()->viewport_screenshot->mime_type);
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
  // TODO(https://crbug.com/398271171): Remove when the actor coordinator
  // handles getting a new observation.
  int32_t tab_id = tab->GetHandle().raw_value();
  glic::FetchPageContext(
      tab, DefaultOptions(),
      base::BindOnce(&ActorKeyedService::ConvertToBrowserActionResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     task_id, tab_id, std::move(action_result)));
}
#endif

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

  actor_task_subscriptions_.erase(task_id);
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

void ActorKeyedService::OnActorTaskStateChanged(TaskId task_id,
                                                ActorTask::State task_state) {
  GetActorUiStateManager()->OnActorTaskStateChange(task_id, task_state);
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
