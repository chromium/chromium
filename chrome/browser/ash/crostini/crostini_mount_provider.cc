// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crostini/crostini_mount_provider.h"

#include <memory>

#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/ash/profiles/profile_helper.h"

namespace crostini {

CrostiniMountProvider::CrostiniMountProvider(Profile* profile,
                                             guest_os::GuestId container_id)
    : profile_(profile), container_id_(container_id) {}

CrostiniMountProvider::~CrostiniMountProvider() = default;

// GuestOsMountProvider overrides
Profile* CrostiniMountProvider::profile() {
  return profile_;
}

std::string CrostiniMountProvider::DisplayName() {
  return FormatForUi(container_id_);
}

guest_os::GuestId CrostiniMountProvider::GuestId() {
  return container_id_;
}

guest_os::VmType CrostiniMountProvider::vm_type() {
  return guest_os::VmType::TERMINA;
}

void CrostiniMountProvider::Prepare(PrepareCallback callback) {
  auto* manager = CrostiniManager::GetForProfile(profile_);
  manager->RestartCrostini(
      container_id_,
      base::BindOnce(&CrostiniMountProvider::OnRestarted,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void CrostiniMountProvider::OnRestarted(PrepareCallback callback,
                                        CrostiniResult result) {
  if (result != CrostiniResult::SUCCESS) {
    std::move(callback).Run(false, 0, 0, base::FilePath());
    return;
  }
  auto* manager = CrostiniManager::GetForProfile(profile_);
  auto vm_info = manager->GetVmInfo(container_id_.vm_name);
  auto container_info = manager->GetContainerInfo(container_id_);
  if (!container_shutdown_observer_.IsObserving()) {
    container_shutdown_observer_.Observe(manager);
  }

  std::move(callback).Run(true, vm_info->info.cid(),
                          container_info->sftp_vsock_port,
                          container_info->homedir);
}

std::unique_ptr<guest_os::GuestOsFileWatcher>
CrostiniMountProvider::CreateFileWatcher(base::FilePath mount_path,
                                         base::FilePath relative_path) {
  return std::make_unique<guest_os::GuestOsFileWatcher>(
      ash::ProfileHelper::GetUserIdHashFromProfile(profile_), container_id_,
      std::move(mount_path), std::move(relative_path));
}

void CrostiniMountProvider::OnContainerShutdown(
    const guest_os::GuestId& container_id) {
  if (container_id != container_id_) {
    return;
  }
  // No-op if we're not mounted.
  Unmount();
  container_shutdown_observer_.Reset();
}

}  // namespace crostini
