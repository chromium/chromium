// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_keyed_service.h"

#include <utility>

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/actor/actor_coordinator.h"
#include "chrome/browser/actor/actor_keyed_service_factory.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"

namespace {

// TODO(crbug.com/411462297): This is a short term hack. This code will be
// deleted soon once StartTask stops creating new tabs implicitly. This adds a
// 1-second delay to wait for about:blank to load. This can be replaced by ~100
// lines of complex code that tries to precisely wait for navigation commit, but
// that would be overkill.
constexpr base::TimeDelta kDelayForNewTab = base::Seconds(1);

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
  tasks_[task_id] = std::move(task);
  return task_id;
}

const std::map<TaskId, std::unique_ptr<ActorTask>>&
ActorKeyedService::GetTasks() {
  return tasks_;
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
    Browser* browser = chrome::FindBrowserWithProfile(profile_.get());
    // If no browser exists create one.
    if (!browser) {
      browser = Browser::Create(
          Browser::CreateParams(profile_.get(), /*user_gesture=*/false));
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
  std::unique_ptr<actor::ActorCoordinator> actor_coordinator;
  if (tab) {
    actor_coordinator =
        std::make_unique<actor::ActorCoordinator>(profile_.get(), tab);
  } else {
    actor_coordinator =
        std::make_unique<actor::ActorCoordinator>(profile_.get());
  }

  auto actor_task =
      std::make_unique<actor::ActorTask>(std::move(actor_coordinator));
  actor::TaskId task_id = AddTask(std::move(actor_task));

  optimization_guide::proto::BrowserStartTaskResult result;
  result.set_task_id(task_id.value());
  result.set_tab_id(handle.raw_value());
  result.set_status(optimization_guide::proto::BrowserStartTaskResult::SUCCESS);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), std::move(result)));
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
