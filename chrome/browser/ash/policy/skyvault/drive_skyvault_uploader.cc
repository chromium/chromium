// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/drive_skyvault_uploader.h"

#include <optional>

#include "base/check_is_test.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"
#include "chrome/browser/ash/file_manager/delete_io_task.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/skyvault/histogram_helper.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/drivefs/drivefs_host.h"
#include "components/drive/file_errors.h"

using storage::FileSystemURL;

namespace policy::local_user_files {
namespace {
// Creates a directory at `dir_path`, if it doesn't already exist. Returns true
// if directory exists or is created successfully.
bool CreateDirectoryIfNeeded(const base::FilePath& dir_path) {
  base::File::Error error = base::File::FILE_OK;
  if (base::DirectoryExists(dir_path)) {
    return true;
  }
  if (!base::CreateDirectoryAndGetError(dir_path, &error)) {
    PLOG(ERROR) << "Failed to create directory: "
                << base::File::ErrorToString(error);
    return false;
  }
  return true;
}
}  // namespace

DriveSkyvaultUploader::DriveSkyvaultUploader(Profile* profile,
                                             const base::FilePath& file_path,
                                             const base::FilePath& target_path,
                                             UploadCallback callback)
    : profile_(profile),
      file_system_context_(
          file_manager::util::GetFileManagerFileSystemContext(profile)),
      drive_integration_service_(
          drive::DriveIntegrationServiceFactory::FindForProfile(profile)),
      source_url_(file_system_context_->CreateCrackedFileSystemURL(
          blink::StorageKey(),
          storage::kFileSystemTypeLocal,
          file_path)),
      target_path_(target_path),
      callback_(std::move(callback)) {}

DriveSkyvaultUploader::~DriveSkyvaultUploader() = default;

void DriveSkyvaultUploader::Run() {
  DCHECK(callback_);

  // TODO(aidazolic): Handle different errors.
  if (!profile_) {
    LOG(ERROR) << "No profile";
    OnEndCopy(MigrationUploadError::kOther);
    return;
  }

  file_manager::VolumeManager* volume_manager =
      file_manager::VolumeManager::Get(profile_);
  if (!volume_manager) {
    LOG(ERROR) << "No volume manager";
    OnEndCopy(MigrationUploadError::kOther);
    return;
  }
  io_task_controller_ = volume_manager->io_task_controller();
  if (!io_task_controller_) {
    LOG(ERROR) << "No task_controller";
    OnEndCopy(MigrationUploadError::kOther);
    return;
  }

  if (!drive_integration_service_) {
    LOG(ERROR) << "No Drive integration service";
    OnEndCopy(MigrationUploadError::kServiceUnavailable);
    return;
  }

  if (drive::util::GetDriveConnectionStatus(profile_) !=
      drive::util::ConnectionStatus::kConnected) {
    LOG(ERROR) << "No connection to Drive";
    OnEndCopy(MigrationUploadError::kServiceUnavailable);
    return;
  }

  // Observe IO tasks updates.
  io_task_controller_observer_.Observe(io_task_controller_);

  // Observe Drive updates.
  drive::DriveIntegrationService::Observer::Observe(drive_integration_service_);
  drivefs::DriveFsHost::Observer::Observe(
      drive_integration_service_->GetDriveFsHost());

  if (!drive_integration_service_->IsMounted()) {
    LOG(ERROR) << "Google Drive is not mounted";
    OnEndCopy(MigrationUploadError::kServiceUnavailable);
    return;
  }

  base::FilePath destination_folder_path =
      drive_integration_service_->GetMountPointPath()
          .AppendASCII("root")
          .Append(target_path_);
  // Copy will fail if the full path doesn't already exist in drive, so first
  // create the destination folder if needed.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CreateDirectoryIfNeeded, destination_folder_path),
      base::BindOnce(&DriveSkyvaultUploader::CreateCopyIOTask,
                     weak_ptr_factory_.GetWeakPtr(), destination_folder_path));
}

void DriveSkyvaultUploader::CreateCopyIOTask(
    const base::FilePath& destination_folder_path,
    bool created) {
  if (observed_copy_task_id_) {
    NOTREACHED_IN_MIGRATION()
        << "The Copy IOTask was already triggered. Case should not be reached.";
  }

  if (!created) {
    OnEndCopy(MigrationUploadError::kCopyFailed);
    return;
  }

  // Destination url.
  FileSystemURL destination_folder_url =
      ash::cloud_upload::FilePathToFileSystemURL(profile_, file_system_context_,
                                                 destination_folder_path);
  // TODO (b/243095484) Define error behavior.
  if (!destination_folder_url.is_valid()) {
    LOG(ERROR) << "Unable to generate destination folder Drive URL";
    OnEndCopy(MigrationUploadError::kCopyFailed);
    return;
  }

  std::vector<FileSystemURL> source_urls{source_url_};
  // Always use a copy task. Will convert to a move upon success.
  std::unique_ptr<file_manager::io_task::IOTask> copy_task =
      std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
          file_manager::io_task::OperationType::kCopy, std::move(source_urls),
          std::move(destination_folder_url), profile_, file_system_context_,
          /*show_notification=*/false);

  observed_copy_task_id_ = io_task_controller_->Add(std::move(copy_task));
}

void DriveSkyvaultUploader::SetFailDeleteForTesting(bool fail) {
  CHECK_IS_TEST();
  fail_delete_for_testing_ = fail;
}

void DriveSkyvaultUploader::OnEndCopy(
    std::optional<MigrationUploadError> error) {
  if (copy_ended_) {
    // Prevent loops in case Copy IO task and Drive sync fail separately.
    return;
  }
  copy_ended_ = true;
  CHECK(!error_);
  error_ = error;

  // If destination file doesn't exist, no delete is required.
  base::FilePath rel_path;
  bool destination_file_exists =
      !observed_absolute_dest_path_.empty() &&
      drive_integration_service_->GetRelativeDrivePath(
          observed_absolute_dest_path_, &rel_path);
  if (!destination_file_exists) {
    OnEndUpload();
    return;
  }

  if (observed_delete_task_id_) {
    NOTREACHED_IN_MIGRATION() << "The delete IOTask was already triggered. "
                                 "Case should not be reached.";
  }

  std::vector<FileSystemURL> file_urls;
  if (!error.has_value()) {
    // If copy to Drive was successful, delete source file to convert the upload
    // to a move to Drive.
    file_urls.push_back(source_url_);
  } else {
    // If copy to Drive was unsuccessful, delete destination file to undo the
    // copy to Drive.
    FileSystemURL dest_url = ash::cloud_upload::FilePathToFileSystemURL(
        profile_, file_system_context_, observed_absolute_dest_path_);
    file_urls.push_back(dest_url);
  }

  std::unique_ptr<file_manager::io_task::IOTask> task =
      std::make_unique<file_manager::io_task::DeleteIOTask>(
          std::move(file_urls), file_system_context_,
          /*show_notification=*/false);
  observed_delete_task_id_ = io_task_controller_->Add(std::move(task));
}

void DriveSkyvaultUploader::OnEndUpload() {
  observed_relative_drive_path_.clear();
  // TODO(b/343879839): Error UMA.
  SkyVaultDeleteErrorHistogram(UploadTrigger::kMigration,
                               CloudProvider::kGoogleDrive,
                               error_ == MigrationUploadError::kDeleteFailed);
  std::move(callback_).Run(error_);
}

void DriveSkyvaultUploader::OnIOTaskStatus(
    const file_manager::io_task::ProgressStatus& status) {
  if (status.task_id == observed_copy_task_id_) {
    OnCopyStatus(status);
    return;
  }
  if (status.task_id == observed_delete_task_id_) {
    OnDeleteStatus(status);
    return;
  }
}

void DriveSkyvaultUploader::OnCopyStatus(
    const file_manager::io_task::ProgressStatus& status) {
  switch (status.state) {
    case file_manager::io_task::State::kScanning:
    case file_manager::io_task::State::kQueued:
    case file_manager::io_task::State::kPaused:
      return;
    case file_manager::io_task::State::kInProgress:
      if (observed_relative_drive_path_.empty() && !status.outputs.empty()) {
        // It's always one file.
        DCHECK_EQ(status.sources.size(), 1u);
        DCHECK_EQ(status.outputs.size(), 1u);

        if (!drive_integration_service_) {
          LOG(ERROR) << "No Drive integration service";
          OnEndCopy(MigrationUploadError::kServiceUnavailable);
          return;
        }

        // Get the output path from the IOTaskController's ProgressStatus. The
        // destination file name is not known in advance, given that it's
        // generated from the IOTaskController which resolves potential name
        // clashes.
        observed_absolute_dest_path_ = status.outputs[0].url.path();
        drive_integration_service_->GetRelativeDrivePath(
            observed_absolute_dest_path_, &observed_relative_drive_path_);
        // scoped_suppress_drive_notifications_for_path_ = std::make_unique<
        //     file_manager::ScopedSuppressDriveNotificationsForPath>(
        //     profile_, observed_relative_drive_path_);
      }
      return;
    case file_manager::io_task::State::kSuccess:
      DCHECK_EQ(status.outputs.size(), 1u);
      return;
    case file_manager::io_task::State::kCancelled:
      LOG(ERROR) << "Upload to Google Drive cancelled";
      OnEndCopy(MigrationUploadError::kCopyFailed);
      return;
    case file_manager::io_task::State::kError:
      // TODO(aidazolic): Potentially handle different IOTask errors as in
      // DriveUploadHandler::ShowIOTaskError.
      OnEndCopy(MigrationUploadError::kCopyFailed);
      return;
    case file_manager::io_task::State::kNeedPassword:
      NOTREACHED_IN_MIGRATION()
          << "Encrypted file should not need password to be copied or "
             "moved. Case should not be reached.";
      return;
  }
}

void DriveSkyvaultUploader::OnDeleteStatus(
    const file_manager::io_task::ProgressStatus& status) {
  switch (status.state) {
    case file_manager::io_task::State::kCancelled:
      NOTREACHED_IN_MIGRATION()
          << "Deletion of source or destination file should not have "
             "been cancelled.";
      ABSL_FALLTHROUGH_INTENDED;
    case file_manager::io_task::State::kError:
      if (!error_) {
        // Don't override errors occurred during the copy.
        error_ = MigrationUploadError::kDeleteFailed;
      }
      break;
    case file_manager::io_task::State::kSuccess:
      break;
    default:
      return;
  }

  if (fail_delete_for_testing_) {
    CHECK_IS_TEST();
    if (!error_) {
      error_ = MigrationUploadError::kDeleteFailed;
    }
  }

  OnEndUpload();
}

void DriveSkyvaultUploader::OnUnmounted() {}

void DriveSkyvaultUploader::ImmediatelyUploadDone(drive::FileError error) {
  LOG_IF(ERROR, error != drive::FileError::FILE_ERROR_OK)
      << "ImmediatelyUpload failed with status: " << error;
}

void DriveSkyvaultUploader::OnSyncingStatusUpdate(
    const drivefs::mojom::SyncingStatus& syncing_status) {
  for (const auto& item : syncing_status.item_events) {
    if (base::FilePath(item->path) != observed_relative_drive_path_) {
      continue;
    }
    if (item->state == drivefs::mojom::ItemEvent::State::kCancelledAndDeleted) {
      continue;
    }
    switch (item->state) {
      case drivefs::mojom::ItemEvent::State::kQueued: {
        // Tell Drive to upload the file now. If successful, we will receive a
        // kInProgress or kCompleted event sooner. If this fails, we ignore it.
        // The file will get uploaded eventually.
        drive_integration_service_->ImmediatelyUpload(
            observed_relative_drive_path_,
            base::BindOnce(&DriveSkyvaultUploader::ImmediatelyUploadDone,
                           weak_ptr_factory_.GetWeakPtr()));
        return;
      }
      case drivefs::mojom::ItemEvent::State::kInProgress:
        return;
      case drivefs::mojom::ItemEvent::State::kCompleted:
        // The file has fully synced.
        OnEndCopy();
        return;
      case drivefs::mojom::ItemEvent::State::kFailed:
        LOG(ERROR) << "Drive sync error: failed";
        OnEndCopy(MigrationUploadError::kCopyFailed);
        return;
      case drivefs::mojom::ItemEvent::State::kCancelledAndDeleted:
        NOTREACHED_IN_MIGRATION();
        return;
      case drivefs::mojom::ItemEvent::State::kCancelledAndTrashed:
        LOG(ERROR) << "Drive sync error: cancelled and trashed";
        OnEndCopy(MigrationUploadError::kCopyFailed);
        return;
    }
  }
}

void DriveSkyvaultUploader::OnError(const drivefs::mojom::DriveError& error) {
  if (base::FilePath(error.path) != observed_relative_drive_path_) {
    return;
  }

  // TODO(aidazolic): Potentially handle different errors, as in
  // DriveUploadHandler::OnError.
  OnEndCopy(MigrationUploadError::kCopyFailed);
}

void DriveSkyvaultUploader::OnDriveConnectionStatusChanged(
    drive::util::ConnectionStatus status) {
  if (status != drive::util::ConnectionStatus::kConnected) {
    LOG(ERROR) << "Lost connection to Drive during upload";
    OnEndCopy(MigrationUploadError::kServiceUnavailable);
  }
}

}  // namespace policy::local_user_files
