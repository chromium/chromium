// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CROSAPI_TASK_MANAGER_CONTROLLER_LACROS_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CROSAPI_TASK_MANAGER_CONTROLLER_LACROS_H_

#include <unordered_map>
#include <vector>

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
                              std::vector<crosapi::mojom::TaskGroupPtr>)>;
  // Gets task manager's task data.
  void GetTaskManagerTasks(GetTaskManagerTasksCallback callback);

  // Called when task manager is closed in ash.
  void OnTaskManagerClosed();

 private:
  using IdTaskMap = std::unordered_map<TaskId, crosapi::mojom::TaskPtr>;

  // task_manager::TaskManagerObserver:
  void OnTaskAdded(TaskId id) override;
  void OnTaskToBeRemoved(TaskId id) override;
  void OnTasksRefreshed(const TaskIdList& task_ids) override;

  crosapi::mojom::TaskPtr ToMojoTask(TaskId id);
  void UpdateTask(TaskId id, crosapi::mojom::TaskPtr& mojo_task);

  // Cache the latest task data to be sent across crosapi when requested.
  IdTaskMap id_to_tasks_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CROSAPI_TASK_MANAGER_CONTROLLER_LACROS_H_
