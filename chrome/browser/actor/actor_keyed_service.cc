// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_keyed_service.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "base/types/pass_key.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/actor/task_id.h"
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
  return options;
}

#endif

void RunLater(base::OnceClosure task) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(FROM_HERE,
                                                              std::move(task));
}

}  // namespace

namespace actor {

ActorKeyedService::ActorKeyedService(Profile* profile) : profile_(profile) {}

ActorKeyedService::~ActorKeyedService() = default;

// static
ActorKeyedService* ActorKeyedService::Get(content::BrowserContext* context) {
  return ActorKeyedServiceFactory::GetActorKeyedService(context);
}

TaskId ActorKeyedService::AddTask(std::unique_ptr<ActorTask> task) {
  TaskId task_id = next_task_id_.GenerateNextId();
  task->SetId(base::PassKey<ActorKeyedService>(), task_id);
  tasks_[task_id] = std::move(task);
  return task_id;
}

const std::map<TaskId, std::unique_ptr<ActorTask>>&
ActorKeyedService::GetTasks() {
  return tasks_;
}

void ActorKeyedService::ExecuteAction(
    optimization_guide::proto::BrowserAction action,
    base::OnceCallback<void(optimization_guide::proto::BrowserActionResult)>
        callback) {
  auto* task = GetTask(actor::TaskId(action.task_id()));
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
  task->GetExecutionEngine()->Act(
      std::move(action), base::BindOnce(&ActorKeyedService::OnActionFinished,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        std::move(callback), action.task_id()));
#endif
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
                       weak_ptr_factory_.GetWeakPtr(), handle, std::move(task),
                       std::move(callback)),
        kDelayForNewTab);
    return;
  }

  FinishStartTask(handle, std::move(task), std::move(callback));
}

void ActorKeyedService::FinishStartTask(
    tabs::TabHandle handle,
    optimization_guide::proto::BrowserStartTask task,
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

  auto actor_task =
      std::make_unique<actor::ActorTask>(std::move(execution_engine));
  actor::TaskId task_id = AddTask(std::move(actor_task));

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
    actor::mojom::ActionResultPtr action_result) {
  auto* task = GetTask(actor::TaskId(task_id));
  CHECK(task);
  tabs::TabInterface* tab = task->GetExecutionEngine()->GetTabOfCurrentTask();
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
      tab, DefaultOptions(), /*include_actionable_data=*/true,
      base::BindOnce(&ActorKeyedService::ConvertToBrowserActionResult,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     task_id, tab_id, std::move(action_result)));
}
#endif

void ActorKeyedService::PerformActions(
    optimization_guide::proto::Actions actions,
    base::OnceCallback<void(optimization_guide::proto::ActionsResult)>
        callback) {
  auto* task = GetTask(actor::TaskId(actions.task_id()));
  if (!task) {
    VLOG(1) << "PerformActions failed: Task not found.";
    optimization_guide::proto::ActionsResult result;
    result.set_action_result(
        static_cast<int32_t>(mojom::ActionResultCode::kTaskWentAway));
    RunLater(base::BindOnce(std::move(callback), std::move(result)));
    return;
  }
  task->GetExecutionEngine()->Act(
      std::move(actions),
      base::BindOnce(&ActorKeyedService::OnActionsFinished,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ActorKeyedService::OnActionsFinished(
    base::OnceCallback<void(optimization_guide::proto::ActionsResult)> callback,
    optimization_guide::proto::ActionsResult result) {
  RunLater(base::BindOnce(std::move(callback), std::move(result)));
}

void ActorKeyedService::StopTask(TaskId task_id) {
  auto task = tasks_.find(task_id);
  if (task != tasks_.end()) {
    task->second->Stop();
  }
}

ActorTask* ActorKeyedService::GetTask(TaskId task_id) {
  auto task = tasks_.find(task_id);
  if (task != tasks_.end()) {
    return task->second.get();
  }
  return nullptr;
}

}  // namespace actor
