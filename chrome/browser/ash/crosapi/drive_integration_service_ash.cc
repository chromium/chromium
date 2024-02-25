// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/drive_integration_service_ash.h"

#include <utility>

#include "base/files/file_path.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chromeos/components/drivefs/mojom/drivefs_native_messaging.mojom.h"

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
             ? GetDriveService()->GetMountPointPath()
             : base::FilePath();
}

}  // namespace

DriveIntegrationServiceAsh::DriveIntegrationServiceAsh() = default;
DriveIntegrationServiceAsh::~DriveIntegrationServiceAsh() = default;

void DriveIntegrationServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::DriveIntegrationService> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void DriveIntegrationServiceAsh::DeprecatedGetMountPointPath(
    DeprecatedGetMountPointPathCallback callback) {
  std::move(callback).Run(GetMountPoint());
}

void DriveIntegrationServiceAsh::AddDriveIntegrationServiceObserver(
    mojo::PendingRemote<mojom::DriveIntegrationServiceObserver> observer) {
  Observe(GetDriveService());
  mojo::Remote<mojom::DriveIntegrationServiceObserver> remote(
      std::move(observer));
  observers_.Add(std::move(remote));
  // Fire the observer with the initial value.
  for (auto& registered_observer : observers_)
    registered_observer->OnMountPointPathChanged(GetMountPoint());
}

void DriveIntegrationServiceAsh::CreateNativeHostSession(
    drivefs::mojom::ExtensionConnectionParamsPtr params,
    mojo::PendingReceiver<drivefs::mojom::NativeMessagingHost> drivefs_receiver,
    mojo::PendingRemote<drivefs::mojom::NativeMessagingPort> extension_remote) {
  if (!GetDriveService() || !GetDriveService()->GetDriveFsInterface()) {
    // Mojo uses unsigned int for disconnect reason, but file errors are
    // negative, so negate the error to pass as it a positive int.
    extension_remote.ResetWithReason(-drive::FILE_ERROR_SERVICE_UNAVAILABLE,
                                     "DriveFS is unavailable.");
    return;
  }
  GetDriveService()->GetDriveFsInterface()->CreateNativeHostSession(
      std::move(params), std::move(drivefs_receiver),
      std::move(extension_remote));
}

void DriveIntegrationServiceAsh::RegisterDriveFsNativeMessageHostBridge(
    mojo::PendingRemote<crosapi::mojom::DriveFsNativeMessageHostBridge>
        bridge) {
  if (GetDriveService()) {
    GetDriveService()->RegisterDriveFsNativeMessageHostBridge(
        std::move(bridge));
  }
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

}  // namespace crosapi
