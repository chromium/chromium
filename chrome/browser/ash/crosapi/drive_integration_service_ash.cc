// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/drive_integration_service_ash.h"

#include <utility>

#include "base/files/file_path.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace crosapi {

namespace {

Profile* GetProfile() {
  return ProfileManager::GetPrimaryUserProfile();
}

drive::DriveIntegrationService* GetDriveService() {
  return GetProfile() ? drive::DriveIntegrationServiceFactory::GetForProfile(
                            GetProfile())
                      : nullptr;
}

base::FilePath GetMountPoint() {
  return GetDriveService() && GetDriveService()->IsMounted()
             ? GetDriveService()->GetMountPointPath().Append(
                   drive::util::kDriveMyDriveRootDirName)
             : base::FilePath();
}

}  // namespace

DriveIntegrationServiceAsh::DriveIntegrationServiceAsh() = default;
DriveIntegrationServiceAsh::~DriveIntegrationServiceAsh() = default;

void DriveIntegrationServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::DriveIntegrationService> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void DriveIntegrationServiceAsh::GetMountPointPath(
    GetMountPointPathCallback callback) {
  std::move(callback).Run(GetMountPoint());
}

void DriveIntegrationServiceAsh::AddDriveIntegrationServiceObserver(
    mojo::PendingRemote<mojom::DriveIntegrationServiceObserver> observer) {
  DCHECK(GetDriveService());
  drive_service_observation_.Reset();
  drive_service_observation_.Observe(GetDriveService());
  mojo::Remote<mojom::DriveIntegrationServiceObserver> remote(
      std::move(observer));
  observers_.Add(std::move(remote));
  // Fire the observer with the initial value.
  for (auto& registered_observer : observers_)
    registered_observer->OnMountPointPathChanged(GetMountPoint());
}

void DriveIntegrationServiceAsh::OnFileSystemMounted() {
  for (auto& observer : observers_)
    observer->OnMountPointPathChanged(GetMountPoint());
}
void DriveIntegrationServiceAsh::OnFileSystemBeingUnmounted() {
  for (auto& observer : observers_)
    observer->OnMountPointPathChanged(base::FilePath());
}
void DriveIntegrationServiceAsh::OnFileSystemMountFailed() {
  for (auto& observer : observers_)
    observer->OnMountPointPathChanged(base::FilePath());
}
void DriveIntegrationServiceAsh::OnDriveIntegrationServiceDestroyed() {
  drive_service_observation_.Reset();
}

}  // namespace crosapi
