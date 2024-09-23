// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/one_drive_integration_service_ash.h"

#include "chrome/browser/ash/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/common/extensions/extension_constants.h"

namespace crosapi {

OneDriveIntegrationServiceAsh::OneDriveIntegrationServiceAsh() = default;
OneDriveIntegrationServiceAsh::~OneDriveIntegrationServiceAsh() = default;

void OneDriveIntegrationServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::OneDriveIntegrationService> receiver) {
  one_drive_service_set_.Add(this, std::move(receiver));
}

void OneDriveIntegrationServiceAsh::AddOneDriveMountObserver(
    mojo::PendingRemote<mojom::OneDriveMountObserver> observer) {
  auto* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile) {
    return;
  }

  mojo::Remote<mojom::OneDriveMountObserver> remote(std::move(observer));
  remote->OnOneDriveMountPointPathChanged(
      ash::cloud_upload::GetODFSFuseboxMount(profile));
  observers_.Add(std::move(remote));

  // Observe FSP.
  if (file_system_provider_observation_.IsObserving()) {
    return;
  }
  ash::file_system_provider::Service* service =
      ash::file_system_provider::Service::Get(profile);
  DCHECK(service);

  file_system_provider_observation_.Observe(service);
}

void OneDriveIntegrationServiceAsh::OnProvidedFileSystemMount(
    const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
    ash::file_system_provider::MountContext context,
    base::File::Error error) {
  const ash::file_system_provider::ProviderId odfs_provider_id =
      ash::file_system_provider::ProviderId::CreateFromExtensionId(
          extension_misc::kODFSExtensionId);
  // Only observe successful mount events for ODFS.
  if (file_system_info.provider_id() != odfs_provider_id ||
      error != base::File::FILE_OK) {
    return;
  }

  auto* profile = ProfileManager::GetPrimaryUserProfile();
  if (!profile) {
    return;
  }

  const auto fusebox_path = ash::cloud_upload::GetODFSFuseboxMount(profile);
  if (fusebox_path.empty()) {
    return;
  }

  for (auto& observer : observers_) {
    observer->OnOneDriveMountPointPathChanged(fusebox_path);
  }
}

void OneDriveIntegrationServiceAsh::OnProvidedFileSystemUnmount(
    const ash::file_system_provider::ProvidedFileSystemInfo& file_system_info,
    base::File::Error error) {
  ash::file_system_provider::ProviderId odfs_provider_id =
      ash::file_system_provider::ProviderId::CreateFromExtensionId(
          extension_misc::kODFSExtensionId);
  // Only observe successful unmount events for ODFS.
  if (file_system_info.provider_id() != odfs_provider_id ||
      error != base::File::FILE_OK) {
    return;
  }

  for (auto& observer : observers_) {
    observer->OnOneDriveMountPointPathChanged(base::FilePath());
  }
}

void OneDriveIntegrationServiceAsh::OnShutDown() {
  if (!file_system_provider_observation_.IsObserving()) {
    return;
  }
  file_system_provider_observation_.Reset();
}

}  // namespace crosapi
