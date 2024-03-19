// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_TASK_MANAGER_LACROS_H_
#define CHROME_BROWSER_LACROS_TASK_MANAGER_LACROS_H_

#include "base/memory/weak_ptr.h"
#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/task_manager.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace task_manager {

class TaskManagerControllerLacros;

}  // namespace task_manager

namespace crosapi {

// This class receives the task manager api calls from ash, and send lacros
// task data to ash. It can only be used on the main thread.
class TaskManagerLacros : public crosapi::mojom::TaskManagerProvider {
 public:
  TaskManagerLacros();
  TaskManagerLacros(const TaskManagerLacros&) = delete;
  TaskManagerLacros& operator=(const TaskManagerLacros&) = delete;
  ~TaskManagerLacros() override;

 private:
  // crosapi::mojom::TaskManagerProvider:
  void DeprecatedSetRefreshArgs(base::TimeDelta refresh_interval,
                                int64_t refresh_flags) override;
  using GetTaskManagerTasksCallback =
      base::OnceCallback<void(std::vector<crosapi::mojom::TaskPtr>,
                              std::vector<crosapi::mojom::TaskGroupPtr>,
                              const std::optional<std::string>&)>;
  void GetTaskManagerTasks(GetTaskManagerTasksCallback callback) override;
  void OnTaskManagerClosed() override;
  void SetRefreshFlags(int64_t refresh_flags) override;
  void ActivateTask(const std::string& task_uuid) override;

  // A unique id that identifies this instance of Lacros.
  base::UnguessableToken id_;
  mojo::Receiver<crosapi::mojom::TaskManagerProvider> receiver_{this};

  std::unique_ptr<task_manager::TaskManagerControllerLacros>
      task_manager_controller_;

  base::WeakPtrFactory<TaskManagerLacros> weak_ptr_factory_{this};
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_LACROS_TASK_MANAGER_LACROS_H_
