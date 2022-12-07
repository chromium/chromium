// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper/wallpaper_drivefs_delegate_impl.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/account_id/account_id.h"
#include "components/drive/file_errors.h"

namespace ash {

namespace {

// Gets a pointer to `DriveIntegrationService` to interact with DriveFS.  If
// DriveFS is not enabled or mounted for this `account_id`, responds with
// `nullptr`. Caller must check null safety carefully, as DriveFS can crash,
// disconnect, or unmount itself and this function will start returning
// `nullptr`.
// If the pointer to `DriveIntegrationService` is held for a long duration, the
// owner must implement
// `DriveIntegrationServiceObserver` and listen for
// `OnDriveIntegrationServiceDestroyed` to avoid use-after-free.
drive::DriveIntegrationService* GetDriveIntegrationService(
    const AccountId& account_id) {
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  if (!profile) {
    VLOG(1) << "No profile for account_id";
    return nullptr;
  }

  drive::DriveIntegrationService* drive_integration_service =
      drive::util::GetIntegrationServiceByProfile(profile);

  if (!drive_integration_service || !drive_integration_service->is_enabled() ||
      !drive_integration_service->IsMounted()) {
    return nullptr;
  }

  return drive_integration_service;
}

base::Time GetModificationTimeFromDriveMetadata(
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  if (error != drive::FILE_ERROR_OK || !metadata) {
    VLOG(1) << "Unable to get metadata for DriveFs wallpaper file. Error: "
            << error;
    return base::Time();
  }
  return metadata->modification_time;
}

}  // namespace

WallpaperDriveFsDelegateImpl::WallpaperDriveFsDelegateImpl() = default;

WallpaperDriveFsDelegateImpl::~WallpaperDriveFsDelegateImpl() = default;

void WallpaperDriveFsDelegateImpl::GetWallpaperModificationTime(
    const AccountId& account_id,
    base::OnceCallback<void(base::Time modification_time)> callback) {
  auto* drive_integration_service = GetDriveIntegrationService(account_id);
  if (!drive_integration_service) {
    std::move(callback).Run(base::Time());
    return;
  }
  // `wallpaper_path` is guaranteed to be non-empty if
  // `drive_integration_service` is initialized.
  const base::FilePath wallpaper_path =
      WallpaperControllerClientImpl::Get()->GetWallpaperPathFromDriveFs(
          account_id);
  DCHECK(!wallpaper_path.empty());
  drive_integration_service->GetMetadata(
      wallpaper_path, base::BindOnce(&GetModificationTimeFromDriveMetadata)
                          .Then(std::move(callback)));
}

}  // namespace ash
