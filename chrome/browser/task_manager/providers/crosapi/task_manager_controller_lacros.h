// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CROSAPI_TASK_MANAGER_CONTROLLER_LACROS_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CROSAPI_TASK_MANAGER_CONTROLLER_LACROS_H_

#include <unordered_map>
#include <vector>

#include "base/process/process_handle.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "chromeos/crosapi/mojom/task_manager.mojom.h"

namespace task_manager {

// This class is instanitiated by TaskManagerLacros. It observes the task
// manager running in lacros and return the task data to ash via crosapi.
class TaskManagerControllerLacros : public TaskManagerObserver {
 public:
  TaskManagerControllerLacros();
  TaskManagerControllerLacros(const TaskManagerControllerLacros&) = delete;
  TaskManagerControllerLacros& operator=(const TaskManagerControllerLacros&) =
      delete;
  ~TaskManagerControllerLacros() override;

  // Sets task refreshing flags.
  // |refresh_flags| specifies the enabled resources types to be refreshed.
  void SetRefreshFlags(int64_t refresh_flags);

  using GetTaskManagerTasksCallback =
      base::OnceCallback<void(std::vector<crosapi::mojom::TaskPtr>,
                              std::vector<crosapi::mojom::TaskGroupPtr>,
                              const std::optional<std::string>&)>;
  // Gets task manager's task data.
  void GetTaskManagerTasks(GetTaskManagerTasksCallback callback);

  // Called when task manager is closed in ash.
  void OnTaskManagerClosed();

  // Activates task specified by |task_uuid|.
  void ActivateTask(const std::string& task_uuid);

 private:
  using IdTaskMap = std::unordered_map<TaskId, crosapi::mojom::TaskPtr>;

  crosapi::mojom::TaskPtr ToMojoTask(TaskId id);
  void UpdateTask(TaskId id, crosapi::mojom::TaskPtr& mojo_task);

  // Creates a mojo object for the task group with the specific |pid| by
  // querying the task group data from the observed task manager.
  // |task_id|: id of a task that belongs to the task group.
  // Note: Task group data is accessed by |task_id| from TaskManagerInterface.
  crosapi::mojom::TaskGroupPtr ToMojoTaskGroup(base::ProcessId pid,
                                               TaskId task_id);
  // Updates |mojo_task_group| with the specific |pid| by querying the task
  // group data from the observed task manager.
  // |task_id|: id of a task that belongs to the task group.
  // Note: Task group data is accessed by |task_id| from TaskManagerInterface.
  void UpdateTaskGroup(base::ProcessId pid,
                       TaskId task_id,
                       crosapi::mojom::TaskGroupPtr& mojo_task_group);

  // Cache the latest task data to be sent across crosapi when requested.
  IdTaskMap id_to_tasks_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CROSAPI_TASK_MANAGER_CONTROLLER_LACROS_H_
