// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/task_manager_lacros.h"

#include "chromeos/lacros/lacros_chrome_service_impl.h"

namespace crosapi {

TaskManagerLacros::TaskManagerLacros() {
  chromeos::LacrosChromeServiceImpl* impl =
      chromeos::LacrosChromeServiceImpl::Get();
  if (!impl->IsTaskManagerAvailable())
    return;
  id_ = base::UnguessableToken::Create();
  impl->task_manager_remote()->RegisterTaskManagerProvider(
      receiver_.BindNewPipeAndPassRemote(), id_);

  // TODO(crbug.com/1148572): create crosapi_task_manager_controller_.
}

TaskManagerLacros::~TaskManagerLacros() = default;

void TaskManagerLacros::SetRefreshArgs(base::TimeDelta refresh_interval,
                                       int64_t refresh_flags) {
  // TODO(crbug.com/1148572): Let CrosapiTaskManagerController SetRefreshArgs.
}

void TaskManagerLacros::GetTaskManagerTasks(
    GetTaskManagerTasksCallback callback) {
  // TODO(crbug.com/1148572): Get task data from CrosapiTaskManagerController.
  std::vector<crosapi::mojom::TaskPtr> task_results;
  std::vector<crosapi::mojom::TaskGroupPtr> task_group_results;
  std::move(callback).Run(std::move(task_results),
                          std::move(task_group_results));
}

void TaskManagerLacros::OnTaskManagerClosed() {
  // TODO(crbug.com/1148572): Notify CrosapiTaskManagerController.
}

}  // namespace crosapi
