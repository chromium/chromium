// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/drive_upload_observer.h"

#include "chrome/browser/ash/file_manager/delete_io_task.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/skyvault/histogram_helper.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "storage/browser/file_system/file_system_url.h"

namespace ash::cloud_upload {

namespace {

const int kNoSyncUpdatesTimeOutMs = 60000;  // 1 Minute.

file_manager::io_task::IOTaskController* GetIOTaskController(Profile* profile) {
  DCHECK(profile);

  file_manager::VolumeManager* volume_manager =
      file_manager::VolumeManager::Get(profile);
  if (!volume_manager) {
    LOG(ERROR) << "No Volume Manager";
    return nullptr;
  }

  return volume_manager->io_task_controller();
}

// Runs the upload callback provided to `DriveUploadObserver::Observe`.
void OnUploadDone(scoped_refptr<DriveUploadObserver> drive_upload_observer,
                  base::OnceCallback<void(bool)> upload_callback,
                  bool success) {
  std::move(upload_callback).Run(success);
}

}  // namespace

// static.
void DriveUploadObserver::Observe(
    Profile* profile,
    base::FilePath file_path,
    UploadTrigger trigger,
    int64_t file_bytes,
    base::RepeatingCallback<void(int64_t)> progress_callback,
    base::OnceCallback<void(bool)> upload_callback) {
  scoped_refptr<DriveUploadObserver> drive_upload_observer =
      new DriveUploadObserver(profile, file_path, trigger, file_bytes,
                              std::move(progress_callback));

  // Keep `drive_upload_observer` alive until the upload is done.
  drive_upload_observer->Run(base::BindOnce(
      &OnUploadDone, drive_upload_observer, std::move(upload_callback)));
}

DriveUploadObserver::DriveUploadObserver(
    Profile* profile,
    base::FilePath file_path,
    UploadTrigger trigger,
    int64_t file_bytes,
    base::RepeatingCallback<void(int64_t)> progress_callback)
    : profile_(profile),
      file_system_context_(
          file_manager::util::GetFileManagerFileSystemContext(profile)),
      drive_integration_service_(
          drive::DriveIntegrationServiceFactory::FindForProfile(profile)),
      observed_local_path_(file_path),
      file_bytes_(file_bytes),
      trigger_(trigger),
      progress_callback_(std::move(progress_callback)) {}

DriveUploadObserver::~DriveUploadObserver() = default;

void DriveUploadObserver::Run(base::OnceCallback<void(bool)> upload_callback) {
  upload_callback_ = std::move(upload_callback);
  if (!profile_) {
    LOG(ERROR) << "No profile";
    OnEndUpload(/*success=*/false);
    return;
  }

  if (drive::util::GetDriveConnectionStatus(profile_) !=
      drive::util::ConnectionStatus::kConnected) {
    LOG(ERROR) << "No connection to Drive";
    OnEndUpload(/*success=*/false);
    return;
  }

  if (!drive_integration_service_) {
    LOG(ERROR) << "No Drive integration service";
    OnEndUpload(/*success=*/false);
    return;
  }

  // Observe Drive updates.
  drive::DriveIntegrationService::Observer::Observe(drive_integration_service_);
  drivefs::DriveFsHost::Observer::Observe(
      drive_integration_service_->GetDriveFsHost());

  if (!drive_integration_service_->IsMounted()) {
    LOG(ERROR) << "Google Drive is not mounted";
    OnEndUpload(/*success=*/false);
    return;
  }

  if (observed_local_path_.empty() ||
      !drive_integration_service_->GetRelativeDrivePath(
          observed_local_path_, &observed_drive_path_)) {
    OnEndUpload(/*success=*/false);
    return;
  }

  StartNoSyncUpdateTimer();
}

void DriveUploadObserver::OnEndUpload(bool success) {
  if (no_sync_update_timeout_.IsRunning()) {
    no_sync_update_timeout_.Reset();
  }

  // TODO(b/343879839): Error UMA.
  // If the file sync to to Drive was unsuccessful, delete the file from the
  // Local cache.
  if (!success) {
    if (!observed_delete_task_id_.has_value()) {
      auto* io_task_controller = GetIOTaskController(profile_);

      DCHECK(io_task_controller);
      DCHECK(!io_task_controller_observer_.IsObserving());

      io_task_controller_observer_.Observe(io_task_controller);
      storage::FileSystemURL file_url = FilePathToFileSystemURL(
          profile_, file_system_context_, observed_local_path_);
      std::unique_ptr<file_manager::io_task::IOTask> task =
          std::make_unique<file_manager::io_task::DeleteIOTask>(
              std::vector<storage::FileSystemURL>{file_url},
              file_system_context_,
              /*show_notification=*/false);
      observed_delete_task_id_ = io_task_controller->Add(std::move(task));
    }
  } else {
    std::move(upload_callback_).Run(success);
  }
}

void DriveUploadObserver::OnUnmounted() {
  OnEndUpload(/*success=*/false);
}

void DriveUploadObserver::OnSyncingStatusUpdate(
    const drivefs::mojom::SyncingStatus& syncing_status) {
  for (const auto& item : syncing_status.item_events) {
    if (base::FilePath(item->path) != observed_drive_path_) {
      continue;
    }

    // Restart the timer.
    StartNoSyncUpdateTimer();

    switch (item->state) {
      case drivefs::mojom::ItemEvent::State::kQueued: {
        // Tell Drive to upload the file now. If successful, we will receive a
        // kInProgress or kCompleted event sooner. If this fails, we ignore it.
        // The file will get uploaded eventually.
        drive_integration_service_->ImmediatelyUpload(
            observed_drive_path_,
            base::BindOnce(&DriveUploadObserver::OnImmediatelyUploadDone,
                           weak_ptr_factory_.GetWeakPtr(),
                           item->bytes_to_transfer));
        return;
      }
      case drivefs::mojom::ItemEvent::State::kInProgress:
        if (item->bytes_transferred > 0) {
          progress_callback_.Run(item->bytes_transferred);
        }
        return;
      case drivefs::mojom::ItemEvent::State::kCompleted:
        progress_callback_.Run(item->bytes_transferred);
        OnEndUpload(/*success=*/true);
        return;
      case drivefs::mojom::ItemEvent::State::kFailed:
        LOG(ERROR) << "Drive sync error: failed";
        OnEndUpload(/*success=*/false);
        return;
      case drivefs::mojom::ItemEvent::State::kCancelledAndDeleted:
      case drivefs::mojom::ItemEvent::State::kCancelledAndTrashed:
        LOG(ERROR) << "Drive sync error: cancelled and deleted/trashed";
        OnEndUpload(/*success=*/false);
        return;
    }
  }
}

void DriveUploadObserver::OnError(const drivefs::mojom::DriveError& error) {
  if (base::FilePath(error.path) != observed_drive_path_) {
    return;
  }

  OnEndUpload(/*success=*/false);
}

void DriveUploadObserver::OnDriveConnectionStatusChanged(
    drive::util::ConnectionStatus status) {
  if (status != drive::util::ConnectionStatus::kConnected) {
    LOG(ERROR) << "Lost connection to Drive during upload";
    OnEndUpload(/*success=*/false);
  }
}

void DriveUploadObserver::OnIOTaskStatus(
    const ::file_manager::io_task::ProgressStatus& status) {
  if (status.task_id != observed_delete_task_id_) {
    return;
  }

  // Only log in case of final state.
  if (status.state == file_manager::io_task::State::kError) {
    policy::local_user_files::SkyVaultDeleteErrorHistogram(
        trigger_, policy::local_user_files::CloudProvider::kGoogleDrive, true);
  }
  if (status.state == file_manager::io_task::State::kSuccess) {
    policy::local_user_files::SkyVaultDeleteErrorHistogram(
        trigger_, policy::local_user_files::CloudProvider::kGoogleDrive, false);
  }

  switch (status.state) {
    case file_manager::io_task::State::kCancelled:
      NOTREACHED_IN_MIGRATION()
          << "Deletion of source or destination file should not have "
             "been cancelled.";
      ABSL_FALLTHROUGH_INTENDED;
    case file_manager::io_task::State::kError:
      LOG(ERROR) << "Deleting the file from the local cache failed.";
      ABSL_FALLTHROUGH_INTENDED;
    case file_manager::io_task::State::kSuccess:
      std::move(upload_callback_).Run(false);
      return;
    default:
      return;
  }
}

void DriveUploadObserver::OnImmediatelyUploadDone(int64_t bytes_transferred,
                                                  drive::FileError error) {
  LOG_IF(ERROR, error != drive::FileError::FILE_ERROR_OK)
      << "ImmediatelyUpload failed with status: " << error;
  if (error != drive::FileError::FILE_ERROR_OK) {
    OnEndUpload(/*success=*/false);
  } else {
    if (bytes_transferred == file_bytes_) {
      // The file is successfully uploaded.
      OnEndUpload(/*success=*/true);
    } else if (bytes_transferred < file_bytes_) {
      // This the first event for just creating the file.
      progress_callback_.Run(bytes_transferred);
    }
  }
}

void DriveUploadObserver::StartNoSyncUpdateTimer() {
  no_sync_update_timeout_.Start(
      FROM_HERE, base::Milliseconds(kNoSyncUpdatesTimeOutMs),
      base::BindOnce(&DriveUploadObserver::NoSyncTimedOut,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveUploadObserver::NoSyncTimedOut() {
  drive_integration_service_->GetDriveFsInterface()->GetMetadata(
      observed_drive_path_,
      base::BindOnce(&DriveUploadObserver::OnGetDriveMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DriveUploadObserver::OnGetDriveMetadata(
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  if (error != drive::FileError::FILE_ERROR_OK || !metadata) {
    OnEndUpload(/*success=*/false);
    return;
  }

  GURL download_url(metadata->download_url);
  if (download_url.is_valid()) {
    // The file was already uploaded.
    OnEndUpload(/*success=*/true);
    return;
  }

  OnEndUpload(/*success=*/false);
}

}  // namespace ash::cloud_upload
