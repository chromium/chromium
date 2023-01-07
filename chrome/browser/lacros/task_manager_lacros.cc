// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/task_manager_lacros.h"

#include "base/notreached.h"
#include "chrome/browser/task_manager/providers/crosapi/task_manager_controller_lacros.h"
#include "chromeos/lacros/lacros_service.h"

namespace crosapi {

TaskManagerLacros::TaskManagerLacros() {
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::TaskManager>())
    return;
  id_ = base::UnguessableToken::Create();
  service->GetRemote<crosapi::mojom::TaskManager>()
      ->RegisterTaskManagerProvider(
          receiver_.BindNewPipeAndPassRemoteWithVersion(), id_);

  task_manager_controller_ =
      std::make_unique<task_manager::TaskManagerControllerLacros>();
}

TaskManagerLacros::~TaskManagerLacros() = default;

void TaskManagerLacros::DeprecatedSetRefreshArgs(
    base::TimeDelta refresh_interval,
    int64_t refresh_flags) {
  NOTIMPLEMENTED();
}

void TaskManagerLacros::GetTaskManagerTasks(
    GetTaskManagerTasksCallback callback) {
  task_manager_controller_->GetTaskManagerTasks(std::move(callback));
}

void TaskManagerLacros::OnTaskManagerClosed() {
  task_manager_controller_->OnTaskManagerClosed();
}

void TaskManagerLacros::SetRefreshFlags(int64_t refresh_flags) {
  task_manager_controller_->SetRefreshFlags(refresh_flags);
}

void TaskManagerLacros::ActivateTask(const std::string& task_uuid) {
  task_manager_controller_->ActivateTask(task_uuid);
}

}  // namespace crosapi
