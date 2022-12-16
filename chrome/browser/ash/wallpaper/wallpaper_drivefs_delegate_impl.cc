// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper/wallpaper_drivefs_delegate_impl.h"

#include "ash/public/cpp/image_downloader.h"
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
#include "google_apis/common/api_error_codes.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace ash {

namespace {

constexpr net::NetworkTrafficAnnotationTag kDriveFsDownloadWallpaperTag =
    net::DefineNetworkTrafficAnnotation("wallpaper_drivefs_delegate", R"(
      semantics {
        sender: "Wallpaper DriveFs"
        description: "Download wallpaper images from Google Drive."
        trigger:
          "Triggered when another device owned by the same user has uploaded a "
          "wallpaper image to Google Drive to sync to this device."
        data: "Readonly OAuth token for Google Drive"
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "Users can disable downloading wallpapers from drive by toggling "
          "Disconnect Google Drive account in chrome://os-settings/files, or "
          "by turning off wallpaper sync in chrome://os-settings/osSync"
        chrome_policy: {
          DriveDisabled {
            DriveDisabled: true
          }
          SyncDisabled {
            SyncDisabled: true
          }
          WallpaperImage {
            WallpaperImage: "png or jpg encoded image data"
          }
        }
      }
    )");

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

void WallpaperDriveFsDelegateImpl::DownloadAndDecodeWallpaper(
    const AccountId& account_id,
    ImageDownloader::DownloadCallback callback) {
  auto* drive_integration_service = GetDriveIntegrationService(account_id);
  if (!drive_integration_service) {
    VLOG(1) << "Skip downloading custom wallpaper because DriveFS is not "
               "available.";
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  // `wallpaper_path` is guaranteed to be non-empty if
  // `drive_integration_service` is initialized.
  const base::FilePath wallpaper_path =
      WallpaperControllerClientImpl::Get()->GetWallpaperPathFromDriveFs(
          account_id);
  DCHECK(!wallpaper_path.empty());

  drive_integration_service->GetMetadata(
      wallpaper_path,
      base::BindOnce(&WallpaperDriveFsDelegateImpl::OnGetDownloadUrlMetadata,
                     weak_ptr_factory_.GetWeakPtr(), account_id,
                     std::move(callback)));
}

void WallpaperDriveFsDelegateImpl::OnGetDownloadUrlMetadata(
    const AccountId& account_id,
    ImageDownloader::DownloadCallback callback,
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  if (error != drive::FileError::FILE_ERROR_OK || !metadata ||
      metadata->download_url.empty()) {
    VLOG(1) << "Unable to get download url for DriveFS wallpaper";
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  auto* drive_integration_service = GetDriveIntegrationService(account_id);
  if (!drive_integration_service) {
    VLOG(1) << "DriveFS no longer mounted, cannot fetch authentication token";
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }

  drive_integration_service->GetReadOnlyAuthenticationToken(base::BindOnce(
      &WallpaperDriveFsDelegateImpl::OnGetDownloadUrlAndAuthentication,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback),
      GURL(metadata->download_url)));
}

void WallpaperDriveFsDelegateImpl::OnGetDownloadUrlAndAuthentication(
    ImageDownloader::DownloadCallback callback,
    const GURL& download_url,
    google_apis::ApiErrorCode error_code,
    const std::string& authentication_token) {
  if (error_code != google_apis::HTTP_SUCCESS || authentication_token.empty()) {
    VLOG(1) << "Unable to fetch authentication token for DriveFS with error "
            << error_code;
    std::move(callback).Run(gfx::ImageSkia());
    return;
  }
  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                    "Bearer " + authentication_token);
  ImageDownloader::Get()->Download(download_url, kDriveFsDownloadWallpaperTag,
                                   std::move(headers), absl::nullopt,
                                   std::move(callback));
}

}  // namespace ash
