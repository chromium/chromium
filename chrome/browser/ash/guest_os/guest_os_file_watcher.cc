// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_file_watcher.h"

#include "base/logging.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_service.pb.h"
#include "content/public/browser/browser_thread.h"

namespace guest_os {

GuestOsFileWatcher::GuestOsFileWatcher(std::string owner_id,
                                       GuestId guest_id,
                                       base::FilePath mount_path,
                                       base::FilePath path)
    : owner_id_(std::move(owner_id)),
      guest_id_(std::move(guest_id)),
      mount_path_(std::move(mount_path)),
      path_(std::move(path)) {}

GuestOsFileWatcher::~GuestOsFileWatcher() {
  if (!file_watcher_callback_) {
    return;
  }
  vm_tools::cicerone::RemoveFileWatchRequest req;
  req.set_owner_id(owner_id_);
  req.set_vm_name(guest_id_.vm_name);
  req.set_container_name(guest_id_.container_name);
  req.set_path(path_.value());

  auto* client = ash::CiceroneClient::Get();
  client->RemoveObserver(this);
  client->RemoveFileWatch(req, base::DoNothing());
}

void GuestOsFileWatcher::Watch(
    base::FilePathWatcher::Callback file_watcher_callback,
    file_manager::FileWatcher::BoolCallback callback) {
  auto* client = ash::CiceroneClient::Get();
  DCHECK(!file_watcher_callback_);
  file_watcher_callback_ = std::move(file_watcher_callback);
  vm_tools::cicerone::AddFileWatchRequest req;
  req.set_owner_id(owner_id_);
  req.set_vm_name(guest_id_.vm_name);
  req.set_container_name(guest_id_.container_name);
  req.set_path(path_.value());
  client->AddObserver(this);
  client->AddFileWatch(
      req,
      base::BindOnce(
          [](file_manager::FileWatcher::BoolCallback callback,
             std::optional<vm_tools::cicerone::AddFileWatchResponse> response) {
            if (!response ||
                response->status() !=
                    vm_tools::cicerone::AddFileWatchResponse::SUCCEEDED) {
              LOG(ERROR) << "Error adding file watch: "
                         << (response ? response->failure_reason()
                                      : "empty response");
              std::move(callback).Run(false);
              return;
            }
            std::move(callback).Run(true);
          },
          std::move(callback)));
}

void GuestOsFileWatcher::OnFileWatchTriggered(
    const vm_tools::cicerone::FileWatchTriggeredSignal& signal) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (signal.owner_id() != owner_id_ || signal.vm_name() != guest_id_.vm_name ||
      signal.container_name() != guest_id_.container_name) {
    return;
  }

  DCHECK(file_watcher_callback_);
  file_watcher_callback_.Run(mount_path_.Append(signal.path()),
                             /*error=*/false);
}

}  // namespace guest_os
