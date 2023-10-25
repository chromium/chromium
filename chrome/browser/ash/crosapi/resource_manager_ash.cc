// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/resource_manager_ash.h"

#include "mojo/public/cpp/bindings/remote.h"

namespace crosapi {

// ResourceManagerAsh

ResourceManagerAsh::ResourceManagerAsh() {
  ash::ResourcedClient* client = ash::ResourcedClient::Get();
  if (client)
    client->AddObserver(this);
}

ResourceManagerAsh::~ResourceManagerAsh() {
  ash::ResourcedClient* client = ash::ResourcedClient::Get();
  if (client)
    client->RemoveObserver(this);
}

void ResourceManagerAsh::BindReceiver(
    mojo::PendingReceiver<mojom::ResourceManager> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void ResourceManagerAsh::OnMemoryPressure(
    ash::ResourcedClient::PressureLevel level,
    uint64_t reclaim_target_kb) {
  for (auto& observer : observers_) {
    mojom::MemoryPressurePtr pressure = mojom::MemoryPressure::New();
    pressure->level = static_cast<mojom::MemoryPressureLevel>(level);
    pressure->reclaim_target_kb = reclaim_target_kb;
    observer->MemoryPressure(std::move(pressure));
  }
}

void ResourceManagerAsh::AddMemoryPressureObserver(
    mojo::PendingRemote<mojom::MemoryPressureObserver> observer) {
  mojo::Remote<mojom::MemoryPressureObserver> remote(std::move(observer));
  observers_.Add(std::move(remote));
}

void ResourceManagerAsh::DEPRECATED_ReportBackgroundProcesses(
    const std::vector<int32_t>& pids) {
  NOTREACHED();
}

void ResourceManagerAsh::ReportPageProcesses(
    std::vector<mojom::PageProcessPtr> page_processes) {
  ash::ResourcedClient* client = ash::ResourcedClient::Get();
  if (!client) {
    return;
  }

  std::vector<ash::ResourcedClient::Process> processes;
  for (auto& page_process : page_processes) {
    processes.emplace_back(page_process->pid, page_process->host_protected_page,
                           page_process->host_visible_page,
                           page_process->host_focused_page);
  }

  client->ReportBrowserProcesses(ash::ResourcedClient::Component::kLacros,
                                 processes);
}

}  // namespace crosapi
