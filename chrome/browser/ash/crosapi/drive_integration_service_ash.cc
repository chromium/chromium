// Copyright 2021 The Chromium Authors. All rights reserved.
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

}  // namespace

DriveIntegrationServiceAsh::DriveIntegrationServiceAsh() = default;
DriveIntegrationServiceAsh::~DriveIntegrationServiceAsh() = default;

void DriveIntegrationServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::DriveIntegrationService> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void DriveIntegrationServiceAsh::GetMountPointPath(
    GetMountPointPathCallback callback) {
  drive::DriveIntegrationService* drive_service =
      drive::DriveIntegrationServiceFactory::GetForProfile(GetProfile());
  const base::FilePath drive_mount =
      drive_service && drive_service->IsMounted()
          ? drive_service->GetMountPointPath().Append(
                drive::util::kDriveMyDriveRootDirName)
          : base::FilePath();
  std::move(callback).Run(drive_mount);
}

}  // namespace crosapi
