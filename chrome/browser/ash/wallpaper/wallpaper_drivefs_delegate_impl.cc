// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/wallpaper/wallpaper_drivefs_delegate_impl.h"

#include <map>

#include "ash/public/cpp/image_downloader.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chromeos/ash/components/drivefs/drivefs_host.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/account_id/account_id.h"
#include "components/drive/file_errors.h"
#include "components/drive/file_system_core_util.h"
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

constexpr char kDriveFsWallpaperDirName[] = "Chromebook Wallpaper";
constexpr char kDriveFsWallpaperFileName[] = "wallpaper.jpg";
constexpr char kDriveFsTempWallpaperFileName[] = "wallpaper-tmp.jpg";

// Gets a pointer to `DriveIntegrationService` to interact with DriveFS.  If
// DriveFS is not enabled or mounted for this `account_id`, responds with
// `nullptr`. Caller must check null safety carefully, as DriveFS can crash,
// disconnect, or unmount itself and this function will start returning
// `nullptr`.
// If the pointer to `DriveIntegrationService` is held for a long duration, the
// owner must implement `DriveIntegrationService::Observer` to avoid
// use-after-free.
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

// Gets a relative path from the DriveFS mount point to the wallpaper file.
base::FilePath GetWallpaperRelativePath() {
  return base::FilePath(drive::util::kDriveMyDriveRootDirName)
      .Append(kDriveFsWallpaperDirName)
      .Append(kDriveFsWallpaperFileName);
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

// Copies file `source` from outside DriveFS, to `destination` inside DriveFS.
// `destination` must be the path to the DriveFS wallpaper file. Copies the
// file to a temporary file first, and then swaps it to the final file path, in
// order to avoid partial writes corrupting the wallpaper image saved to
// DriveFS. Must be run on a blocking task runner.
bool CopyFileToDriveFsBlocking(const base::FilePath& source,
                               const base::FilePath& destination) {
  DCHECK_EQ(destination.BaseName().value(), kDriveFsWallpaperFileName);
  const base::FilePath directory = destination.DirName();
  DCHECK_EQ(directory.BaseName().value(), kDriveFsWallpaperDirName);
  if (!base::DirectoryExists(directory) && !base::CreateDirectory(directory)) {
    DVLOG(1) << "Failed to create DriveFS '" << kDriveFsWallpaperDirName
             << "' directory";
    return false;
  }

  base::FilePath temp_file_path =
      directory.Append(base::UnguessableToken::Create().ToString().append(
          kDriveFsTempWallpaperFileName));

  if (!base::CopyFile(source, temp_file_path)) {
    DVLOG(1) << "Failed to copy wallpaper file to DriveFs";
    base::DeleteFile(temp_file_path);
    return false;
  }

  base::File::Error error;
  if (!base::ReplaceFile(temp_file_path, destination, &error)) {
    DVLOG(1) << "Failed to move temp wallpaper file with error '" << error
             << "'";
    base::DeleteFile(temp_file_path);
    return false;
  }
  return true;
}

}  // namespace

WallpaperChangeWaiter::WallpaperChangeWaiter(
    const AccountId& account_id,
    base::OnceCallback<void(bool success)> callback)
    : account_id_(account_id),
      path_to_watch_(base::FilePath(base::FilePath::kSeparators)
                         .Append(GetWallpaperRelativePath())),
      callback_(std::move(callback)) {
  DCHECK(callback_);
  auto* drive_integration_service = GetDriveIntegrationService(account_id);
  if (!drive_integration_service) {
    std::move(callback_).Run(/*success=*/false);
    return;
  }

  Observe(drive_integration_service->GetDriveFsHost());
}

WallpaperChangeWaiter::~WallpaperChangeWaiter() {
  if (callback_) {
    std::move(callback_).Run(/*success=*/false);
  }
}

void WallpaperChangeWaiter::OnUnmounted() {
  if (callback_) {
    std::move(callback_).Run(/*success=*/false);
  }
}

void WallpaperChangeWaiter::OnError(const drivefs::mojom::DriveError& error) {
  if (error.path == path_to_watch_ && callback_) {
    LOG(WARNING) << "DriveFS wallpaper error: " << error.type;
    std::move(callback_).Run(/*success=*/false);
  }
}

void WallpaperChangeWaiter::OnFilesChanged(
    const std::vector<drivefs::mojom::FileChange>& changes) {
  for (const auto& change : changes) {
    if (change.path == path_to_watch_ && callback_) {
      std::move(callback_).Run(/*success=*/true);
      return;
    }
  }
}

WallpaperDriveFsDelegateImpl::WallpaperDriveFsDelegateImpl()
    : blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})) {}

WallpaperDriveFsDelegateImpl::~WallpaperDriveFsDelegateImpl() = default;

base::FilePath WallpaperDriveFsDelegateImpl::GetWallpaperPath(
    const AccountId& account_id) {
  auto* drive_integration_service = GetDriveIntegrationService(account_id);
  if (!drive_integration_service) {
    VLOG(1)
        << "Cannot get DriveFS Wallpaper path because DriveFS is unavailable";
    return base::FilePath();
  }
  auto mount_path = drive_integration_service->GetMountPointPath();
  DCHECK(!mount_path.empty());
  return mount_path.Append(GetWallpaperRelativePath());
}

void WallpaperDriveFsDelegateImpl::SaveWallpaper(
    const AccountId& account_id,
    const base::FilePath& source,
    base::OnceCallback<void(bool)> callback) {
  auto* drive_integration_service = GetDriveIntegrationService(account_id);
  if (!drive_integration_service) {
    DVLOG(1)
        << "Not saving wallpaper to DriveFS because DriveFS is unavailable";
    std::move(callback).Run(/*success=*/false);
    return;
  }
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CopyFileToDriveFsBlocking, source,
                     GetWallpaperPath(account_id)),
      std::move(callback));
}

void WallpaperDriveFsDelegateImpl::WaitForWallpaperChange(
    const AccountId& account_id,
    WaitForWallpaperChangeCallback callback) {
  // Remove any old `WallpaperChangeWaiter` and create a new one. The old one
  // will run any pending callbacks on deletion.
  wallpaper_change_waiters_.erase(account_id);
  wallpaper_change_waiters_.try_emplace(
      account_id, account_id,
      base::BindOnce(&WallpaperDriveFsDelegateImpl::OnDriveFsWallpaperChanged,
                     weak_ptr_factory_.GetWeakPtr(), account_id,
                     std::move(callback)));
}

void WallpaperDriveFsDelegateImpl::GetWallpaperModificationTime(
    const AccountId& account_id,
    base::OnceCallback<void(base::Time modification_time)> callback) {
  auto* drive_integration_service = GetDriveIntegrationService(account_id);
  if (!drive_integration_service) {
    std::move(callback).Run(base::Time());
    return;
  }

  drive_integration_service->GetMetadata(
      GetWallpaperPath(account_id),
      base::BindOnce(&GetModificationTimeFromDriveMetadata)
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

  drive_integration_service->GetMetadata(
      GetWallpaperPath(account_id),
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
      weak_ptr_factory_.GetWeakPtr(), account_id, std::move(callback),
      GURL(metadata->download_url)));
}

void WallpaperDriveFsDelegateImpl::OnGetDownloadUrlAndAuthentication(
    const AccountId& account_id,
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
                                   account_id, std::move(headers),
                                   std::move(callback));
}

void WallpaperDriveFsDelegateImpl::OnDriveFsWallpaperChanged(
    const AccountId& account_id,
    WaitForWallpaperChangeCallback callback,
    bool success) {
  std::move(callback).Run(success);
  wallpaper_change_waiters_.erase(account_id);
}

}  // namespace ash
