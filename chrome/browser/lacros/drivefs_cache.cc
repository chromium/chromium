// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/drivefs_cache.h"

#include "chrome/common/chrome_paths_lacros.h"
#include "chromeos/lacros/lacros_service.h"

DriveFsCache::DriveFsCache() = default;
DriveFsCache::~DriveFsCache() = default;

void DriveFsCache::Start() {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (!lacros_service->IsAvailable<crosapi::mojom::DriveIntegrationService>())
    return;

  // Check if Ash is too old to support the `DriveIntegrationServiceObserver`.
  int driveServiceVersion =
      lacros_service
          ->GetInterfaceVersion<crosapi::mojom::DriveIntegrationService>();
  int minRequiredVersion = static_cast<int>(
      crosapi::mojom::DriveIntegrationService::MethodMinVersions::
          kAddDriveIntegrationServiceObserverMinVersion);
  if (driveServiceVersion < minRequiredVersion)
    return;

  lacros_service->GetRemote<crosapi::mojom::DriveIntegrationService>()
      ->AddDriveIntegrationServiceObserver(
          receiver_.BindNewPipeAndPassRemote());
}

void DriveFsCache::OnMountPointPathChanged(const base::FilePath& drivefs) {
  chrome::SetDriveFsMountPointPath(drivefs);
}
