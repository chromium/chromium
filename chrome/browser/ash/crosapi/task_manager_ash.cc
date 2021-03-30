// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/task_manager_ash.h"

namespace crosapi {

TaskManagerAsh::TaskManagerAsh() = default;

TaskManagerAsh::~TaskManagerAsh() = default;

void TaskManagerAsh::BindReceiver(
    mojo::PendingReceiver<mojom::TaskManager> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void TaskManagerAsh::RegisterTaskManagerProvider(
    mojo::PendingRemote<mojom::TaskManagerProvider> provider,
    const base::UnguessableToken& token) {
  mojo::Remote<mojom::TaskManagerProvider> remote(std::move(provider));
  remote.set_disconnect_handler(
      base::BindOnce(&TaskManagerAsh::TaskManagerProviderDisconnected,
                     weak_factory_.GetWeakPtr(), token));
  task_manager_providers_[token] = std::move(remote);

  // Sets refresh args for the new provider.
  task_manager_providers_[token]->SetRefreshArgs(refresh_interval_,
                                                 refresh_flags_);
}

void TaskManagerAsh::TaskManagerProviderDisconnected(
    const base::UnguessableToken& token) {
  task_manager_providers_.erase(token);

  if (observer_)
    observer_->OnTaskManagerProviderDisconnected();
}

void TaskManagerAsh::SetRefreshArgs(base::TimeDelta refresh_interval,
                                    int64_t refresh_flags) {
  refresh_interval_ = refresh_interval;
  refresh_flags_ = refresh_flags;

  for (auto& pair : task_manager_providers_)
    pair.second->SetRefreshArgs(refresh_interval_, refresh_flags_);
}

void TaskManagerAsh::GetTaskManagerTasks(GetTaskManagerTasksCallback callback) {
  // TODO(crbug.com/1188426): Although the task manager model supports multiple
  // task manager providers, currently, there will only be one lacros instance
  // running. The task manager logic supports only one provider. We will add
  // support to handle multiple providers in the future when multiple lacros
  // instances case becomes true.
  DCHECK_EQ(task_manager_providers_.size(), 1);
  task_manager_providers_.begin()->second->GetTaskManagerTasks(std::move(callback));
}

void TaskManagerAsh::OnTaskManagerClosed() {
  for (auto& pair : task_manager_providers_)
    pair.second->OnTaskManagerClosed();
}

void TaskManagerAsh::RemoveObserver() {
  observer_ = nullptr;
}

void TaskManagerAsh::SetObserver(Observer* observer) {
  DCHECK(!observer_);
  observer_ = observer;
}

}  // namespace crosapi
