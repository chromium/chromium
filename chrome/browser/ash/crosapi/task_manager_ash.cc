// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/task_manager_ash.h"

#include "chrome/browser/ui/browser_commands.h"

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

  auto new_remote = std::make_unique<mojo::Remote<mojom::TaskManagerProvider>>(
      std::move(remote));
  // Preserve the pointer, because new_remote will be bound to the callback.
  auto* remote_ptr = new_remote.get();
  remote_ptr->QueryVersion(
      base::BindOnce(&TaskManagerAsh::OnProviderVersionReady,
                     weak_factory_.GetWeakPtr(), token, std::move(new_remote)));
}

void TaskManagerAsh::ShowTaskManager() {
  chrome::OpenTaskManager(/*browser=*/nullptr);
}

void TaskManagerAsh::TaskManagerProviderDisconnected(
    const base::UnguessableToken& token) {
  task_manager_providers_.erase(token);

  if (observer_)
    observer_->OnTaskManagerProviderDisconnected();
}

void TaskManagerAsh::OnProviderVersionReady(
    const base::UnguessableToken& token,
    std::unique_ptr<mojo::Remote<mojom::TaskManagerProvider>> provider,
    uint32_t interface_version) {
  if (interface_version < 1U) {
    LOG(ERROR) << "Unsupported lacros version";
    return;
  }
  const auto pair =
      task_manager_providers_.emplace(token, std::move(*provider));
  DCHECK(pair.second);
  provider_version_ = interface_version;
  pair.first->second->SetRefreshFlags(refresh_flags_);
}

void TaskManagerAsh::SetRefreshFlags(int64_t refresh_flags) {
  refresh_flags_ = refresh_flags;
  for (auto& pair : task_manager_providers_)
    pair.second->SetRefreshFlags(refresh_flags);
}

void TaskManagerAsh::GetTaskManagerTasks(GetTaskManagerTasksCallback callback) {
  // TODO(crbug.com/40173304): Although the task manager model supports multiple
  // task manager providers, currently, there will only be one lacros instance
  // running. The task manager logic supports only one provider. We will add
  // support to handle multiple providers in the future when multiple lacros
  // instances case becomes true.
  DCHECK_EQ(task_manager_providers_.size(), 1U);
  if (refresh_flags_ != task_manager::REFRESH_TYPE_NONE) {
    task_manager_providers_.begin()->second->GetTaskManagerTasks(
        std::move(callback));
  }
}

void TaskManagerAsh::OnTaskManagerClosed() {
  for (auto& pair : task_manager_providers_)
    pair.second->OnTaskManagerClosed();
}

void TaskManagerAsh::ActivateTask(const std::string& task_uuid) {
  if (provider_version_ < 2U) {
    LOG(WARNING) << "Unsupported lacros task manager provider version: "
                 << provider_version_;
    return;
  }

  for (auto& pair : task_manager_providers_)
    pair.second->ActivateTask(task_uuid);
}

void TaskManagerAsh::RemoveObserver() {
  observer_ = nullptr;
}

void TaskManagerAsh::SetObserver(Observer* observer) {
  DCHECK(!observer_);
  observer_ = observer;
}

bool TaskManagerAsh::HasRegisteredProviders() const {
  return !task_manager_providers_.empty();
}

}  // namespace crosapi
