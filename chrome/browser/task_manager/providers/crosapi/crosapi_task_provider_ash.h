// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CROSAPI_CROSAPI_TASK_PROVIDER_ASH_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CROSAPI_CROSAPI_TASK_PROVIDER_ASH_H_

#include <unordered_map>

#include "base/gtest_prod_util.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/crosapi/task_manager_ash.h"
#include "chrome/browser/task_manager/providers/task_provider.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "chromeos/crosapi/mojom/task_manager.mojom.h"

namespace task_manager {

class CrosapiTask;
class TaskGroup;

// This provides the tasks which are retrieved from lacros via crosapi.
class CrosapiTaskProviderAsh : public TaskProvider,
                               public crosapi::TaskManagerAsh::Observer {
 public:
  CrosapiTaskProviderAsh();
  CrosapiTaskProviderAsh(const CrosapiTaskProviderAsh&) = delete;
  CrosapiTaskProviderAsh& operator=(const CrosapiTaskProviderAsh&) = delete;
  ~CrosapiTaskProviderAsh() override;

  // task_manager::TaskProvider:
  Task* GetTaskOfUrlRequest(int child_id, int route_id) override;

  // Refreshes |task_group| with the most recent data received from crosapi.
  void RefreshTaskGroup(TaskGroup* task_group);

  // Gets the list of task IDs currently tracked and sorted by Lacros
  // task manager, which should be the order Lacros tasks displayed in
  // ash task manager UI, if no sorting order is specified by user.
  // The sorting logic is documented by TaskManagerInterface::GetSortedTaskIds.
  const TaskIdList& GetSortedTaskIds();

 private:
  friend class CrosapiTaskProviderAshTest;
  FRIEND_TEST_ALL_PREFIXES(CrosapiTaskProviderAshTest, OnGetTaskManagerTasks);

  using UuidTaskMap =
      std::unordered_map<std::string, std::unique_ptr<CrosapiTask>>;
  using PidToTaskGroupMap =
      std::unordered_map<base::ProcessId, crosapi::mojom::TaskGroupPtr>;

  // task_manager::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  // crosapi::TaskManagerAsh::Observer.
  void OnTaskManagerProviderDisconnected() override;

  // Gets the lacros tasks by calling crosapi.
  void GetCrosapiTaskManagerTasks();
  void OnGetTaskManagerTasks(
      std::vector<crosapi::mojom::TaskPtr> task_results,
      std::vector<crosapi::mojom::TaskGroupPtr> task_group_results,
      const std::optional<std::string>& active_task_uuid);

  // Cleans up cached tasks and refresh arguments.
  void CleanupCachedData();

  // The timer used to schedule refreshing tasks from lacros.
  base::RepeatingTimer refresh_timer_;

  UuidTaskMap uuid_to_task_;

  // A cached sorted list of the task IDs.
  std::vector<TaskId> sorted_task_ids_;

  // Cached task group data received from crosapi.
  PidToTaskGroupMap pid_to_task_groups_;

  base::WeakPtrFactory<CrosapiTaskProviderAsh> weak_ptr_factory_{this};
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CROSAPI_CROSAPI_TASK_PROVIDER_ASH_H_
