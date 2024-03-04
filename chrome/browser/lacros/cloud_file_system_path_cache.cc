// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/cloud_file_system_path_cache.h"

#include "chrome/common/chrome_paths_lacros.h"
#include "chromeos/lacros/lacros_service.h"

CloudFileSystemPathCache::CloudFileSystemPathCache() = default;
CloudFileSystemPathCache::~CloudFileSystemPathCache() = default;

void CloudFileSystemPathCache::Start() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::DriveIntegrationService>()) {
    // Check if Ash is too old to support the `DriveIntegrationServiceObserver`.
    int driveServiceVersion =
        lacros_service
            ->GetInterfaceVersion<crosapi::mojom::DriveIntegrationService>();
    int minRequiredVersion = static_cast<int>(
        crosapi::mojom::DriveIntegrationService::MethodMinVersions::
            kAddDriveIntegrationServiceObserverMinVersion);
    if (driveServiceVersion >= minRequiredVersion) {
      lacros_service->GetRemote<crosapi::mojom::DriveIntegrationService>()
          ->AddDriveIntegrationServiceObserver(
              drivefs_receiver_.BindNewPipeAndPassRemote());
    }
  }

  if (lacros_service
          ->IsAvailable<crosapi::mojom::OneDriveIntegrationService>()) {
    lacros_service->GetRemote<crosapi::mojom::OneDriveIntegrationService>()
        ->AddOneDriveMountObserver(
            onedrive_receiver_.BindNewPipeAndPassRemote());
  }
}

void CloudFileSystemPathCache::OnMountPointPathChanged(
    const base::FilePath& drivefs) {
  chrome::SetDriveFsMountPointPath(drivefs);
}

void CloudFileSystemPathCache::OnOneDriveMountPointPathChanged(
    const base::FilePath& onedrive) {
  chrome::SetOneDriveMountPointPath(onedrive);
}
