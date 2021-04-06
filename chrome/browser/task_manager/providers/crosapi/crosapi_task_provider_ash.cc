// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/crosapi/crosapi_task_provider_ash.h"

#include <set>

#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/task_manager/providers/crosapi/crosapi_task.h"

namespace task_manager {

CrosapiTaskProviderAsh::CrosapiTaskProviderAsh() {
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->task_manager_ash()
      ->SetObserver(this);
}

CrosapiTaskProviderAsh::~CrosapiTaskProviderAsh() {
  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->task_manager_ash()
      ->RemoveObserver();
}

Task* CrosapiTaskProviderAsh::GetTaskOfUrlRequest(int child_id, int route_id) {
  return nullptr;
}

void CrosapiTaskProviderAsh::StartUpdating() {
  refresh_timer_.Start(
      FROM_HERE, base::TimeDelta::FromSeconds(1),
      base::BindRepeating(&CrosapiTaskProviderAsh::GetCrosapiTaskManagerTasks,
                          base::Unretained(this)));
}

void CrosapiTaskProviderAsh::StopUpdating() {
  // Task manager has stopped updating (Task manager UI closed). Clean up cached
  // data and notify lacros about the event.
  refresh_timer_.Stop();
  CleanupCachedData();

  crosapi::CrosapiManager::Get()
      ->crosapi_ash()
      ->task_manager_ash()
      ->OnTaskManagerClosed();
}

void CrosapiTaskProviderAsh::OnTaskManagerProviderDisconnected() {
  // No more task manager clients (lacros has been shut down), remove all
  // lacros tasks and clean up.
  for (const auto& item : uuid_to_task_)
    NotifyObserverTaskRemoved(item.second.get());

  CleanupCachedData();
}

void CrosapiTaskProviderAsh::GetCrosapiTaskManagerTasks() {
  // Get lacros tasks if there is any task manager provider registered and ash
  // task manager has been set up to refresh with a valid refresh interval.
  auto* task_manager_ash =
      crosapi::CrosapiManager::Get()->crosapi_ash()->task_manager_ash();
  if (task_manager_ash->HasRegisteredProviders()) {
    task_manager_ash->GetTaskManagerTasks(
        base::BindOnce(&CrosapiTaskProviderAsh::OnGetTaskManagerTasks,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void CrosapiTaskProviderAsh::OnGetTaskManagerTasks(
    std::vector<crosapi::mojom::TaskPtr> task_results,
    std::vector<crosapi::mojom::TaskGroupPtr> task_group_results) {
  std::set<std::string> uuid_to_remove;
  for (const auto& item : uuid_to_task_)
    uuid_to_remove.insert(item.first);

  for (const auto& mojo_task : task_results) {
    std::unique_ptr<CrosapiTask>& task = uuid_to_task_[mojo_task->task_uuid];
    if (uuid_to_remove.erase(mojo_task->task_uuid) == 0) {
      // New lacros task.
      DCHECK(!task.get());
      task = std::make_unique<CrosapiTask>(mojo_task);
      NotifyObserverTaskAdded(task.get());
    } else {
      // Update existing lacros task.
      task->Update(mojo_task);
      if (task->process_id() != mojo_task->process_id) {
        UpdateTaskProcessInfoAndNotifyObserver(
            task.get(), mojo_task->process_id, mojo_task->process_id);
      }
    }
  }

  // Remove the stale lacros tasks.
  for (const auto& uuid : uuid_to_remove) {
    NotifyObserverTaskRemoved(uuid_to_task_[uuid].get());
    uuid_to_task_.erase(uuid);
  }
}

void CrosapiTaskProviderAsh::CleanupCachedData() {
  uuid_to_task_.clear();
}

}  // namespace task_manager
