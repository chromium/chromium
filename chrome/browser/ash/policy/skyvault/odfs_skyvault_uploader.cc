// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/odfs_skyvault_uploader.h"

#include <optional>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/skyvault/signin_notification_helper.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"

namespace ash::cloud_upload {

namespace {

// Runs the upload callback provided to `OdfsSkyvaultUploader::Upload`.
void OnUploadDone(
    scoped_refptr<OdfsSkyvaultUploader> odfs_skyvault_uploader,
    base::OnceCallback<void(bool, storage::FileSystemURL)> upload_callback,
    storage::FileSystemURL file_url,
    std::optional<MigrationUploadError> error) {
  std::move(upload_callback).Run(!error.has_value(), std::move(file_url));
}

// Runs the upload callback provided to `OdfsSkyvaultUploader::Upload`.
void OnUploadDoneWithError(
    scoped_refptr<OdfsSkyvaultUploader> odfs_skyvault_uploader,
    base::OnceCallback<void(storage::FileSystemURL,
                            std::optional<MigrationUploadError>)>
        upload_callback,
    storage::FileSystemURL file_url,
    std::optional<MigrationUploadError> error) {
  std::move(upload_callback).Run(std::move(file_url), error);
}

static int64_t g_id_counter = 0;

}  // namespace

// static.
base::WeakPtr<OdfsSkyvaultUploader> OdfsSkyvaultUploader::Upload(
    Profile* profile,
    const base::FilePath& path,
    FileType file_type,
    base::RepeatingCallback<void(int64_t)> progress_callback,
    base::OnceCallback<void(bool, storage::FileSystemURL)> upload_callback) {
  auto* file_system_context =
      file_manager::util::GetFileManagerFileSystemContext(profile);
  DCHECK(file_system_context);
  base::FilePath tmp_dir;
  CHECK((base::GetTempDir(&tmp_dir) && tmp_dir.IsParent(path)) ||
        file_type == FileType::kMigration);
  auto file_system_url = file_system_context->CreateCrackedFileSystemURL(
      blink::StorageKey(), storage::kFileSystemTypeLocal, path);
  scoped_refptr<OdfsSkyvaultUploader> odfs_skyvault_uploader =
      new OdfsSkyvaultUploader(profile, ++g_id_counter, file_system_url,
                               file_type, std::move(progress_callback),
                               /*target_path=*/std::nullopt);

  // Keep `odfs_skyvault_uploader` alive until the upload is done.
  odfs_skyvault_uploader->Run(base::BindOnce(
      &OnUploadDone, odfs_skyvault_uploader, std::move(upload_callback)));
  return odfs_skyvault_uploader->GetWeakPtr();
}

// static.
base::WeakPtr<OdfsSkyvaultUploader> OdfsSkyvaultUploader::Upload(
    Profile* profile,
    const base::FilePath& path,
    FileType file_type,
    base::RepeatingCallback<void(int64_t)> progress_callback,
    UploadDoneCallback upload_callback_with_error,
    const base::FilePath& target_path) {
  auto* file_system_context =
      file_manager::util::GetFileManagerFileSystemContext(profile);
  DCHECK(file_system_context);
  base::FilePath tmp_dir;
  CHECK((base::GetTempDir(&tmp_dir) && tmp_dir.IsParent(path)) ||
        file_type == FileType::kMigration);
  auto file_system_url = file_system_context->CreateCrackedFileSystemURL(
      blink::StorageKey(), storage::kFileSystemTypeLocal, path);
  scoped_refptr<OdfsSkyvaultUploader> odfs_skyvault_uploader =
      new OdfsSkyvaultUploader(profile, ++g_id_counter, file_system_url,
                               file_type, std::move(progress_callback),
                               target_path);

  // Keep `odfs_skyvault_uploader` alive until the upload is done.
  odfs_skyvault_uploader->Run(
      base::BindOnce(&OnUploadDoneWithError, odfs_skyvault_uploader,
                     std::move(upload_callback_with_error)));
  return odfs_skyvault_uploader->GetWeakPtr();
}

base::WeakPtr<OdfsSkyvaultUploader> OdfsSkyvaultUploader::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

OdfsSkyvaultUploader::OdfsSkyvaultUploader(
    Profile* profile,
    int64_t id,
    const storage::FileSystemURL& file_system_url,
    FileType file_type,
    base::RepeatingCallback<void(int64_t)> progress_callback,
    std::optional<base::FilePath> target_path)
    : profile_(profile),
      file_system_context_(
          file_manager::util::GetFileManagerFileSystemContext(profile)),
      id_(id),
      file_system_url_(file_system_url),
      target_path_(target_path),
      file_type_(file_type),
      progress_callback_(std::move(progress_callback)) {}

OdfsSkyvaultUploader::~OdfsSkyvaultUploader() {
  // Stop observing IO task updates.
  if (io_task_controller_) {
    io_task_controller_->RemoveObserver(this);
  }
}

void OdfsSkyvaultUploader::Run(UploadDoneCallback upload_callback) {
  upload_callback_ = std::move(upload_callback);

  if (!profile_) {
    LOG(ERROR) << "No profile";
    OnEndUpload(/*url=*/{}, MigrationUploadError::kOther);
    return;
  }

  file_manager::VolumeManager* volume_manager =
      (file_manager::VolumeManager::Get(profile_));
  if (!volume_manager) {
    LOG(ERROR) << "No volume manager";
    OnEndUpload(/*url=*/{}, MigrationUploadError::kOther);
    return;
  }
  io_task_controller_ = volume_manager->io_task_controller();
  if (!io_task_controller_) {
    LOG(ERROR) << "No task_controller";
    OnEndUpload(/*url=*/{}, MigrationUploadError::kOther);
    return;
  }

  // Observe IO tasks updates.
  io_task_controller_->AddObserver(this);

  GetODFSMetadataAndStartIOTask();
}

void OdfsSkyvaultUploader::OnEndUpload(
    storage::FileSystemURL url,
    std::optional<MigrationUploadError> error) {
  std::move(upload_callback_).Run(std::move(url), error);
}

void OdfsSkyvaultUploader::GetODFSMetadataAndStartIOTask() {
  file_system_provider::ProvidedFileSystemInterface* file_system =
      GetODFS(profile_);
  if (!file_system) {
    policy::skyvault_ui_utils::ShowSignInNotification(
        profile_, id_, file_type_, file_system_url_.path().BaseName().value(),
        base::BindOnce(&OdfsSkyvaultUploader::OnMountResponse,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // First check that ODFS is not in the "ReauthenticationRequired" state.
  GetODFSMetadata(
      file_system,
      base::BindOnce(&OdfsSkyvaultUploader::CheckReauthenticationAndStartIOTask,
                     weak_ptr_factory_.GetWeakPtr()));
}

void OdfsSkyvaultUploader::CheckReauthenticationAndStartIOTask(
    base::expected<ODFSMetadata, base::File::Error> metadata_or_error) {
  if (!metadata_or_error.has_value()) {
    // Try the move anyway.
    LOG(ERROR) << "Failed to get reauthentication required state: "
               << metadata_or_error.error();
  } else if (metadata_or_error->reauthentication_required ||
             (metadata_or_error->account_state.has_value() &&
              metadata_or_error->account_state.value() ==
                  OdfsAccountState::kReauthenticationRequired)) {
    // TODO(b/330786891): Only query account_state once
    // reauthentication_required is no longer needed for backwards compatibility
    // with ODFS.
    policy::skyvault_ui_utils::ShowSignInNotification(
        profile_, id_, file_type_, file_system_url_.path().BaseName().value(),
        base::BindOnce(&OdfsSkyvaultUploader::OnMountResponse,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  StartIOTask();
}

void OdfsSkyvaultUploader::OnIOTaskStatus(
    const file_manager::io_task::ProgressStatus& status) {
  if (status.task_id != observed_task_id_) {
    return;
  }
  switch (status.state) {
    case file_manager::io_task::State::kInProgress:
      if (status.bytes_transferred > 0) {
        progress_callback_.Run(status.bytes_transferred);
      }
      return;
    case file_manager::io_task::State::kPaused:
    case file_manager::io_task::State::kScanning:
    case file_manager::io_task::State::kQueued:
      return;
    case file_manager::io_task::State::kSuccess:
      progress_callback_.Run(status.bytes_transferred);
      OnEndUpload(status.outputs[0].url);
      return;
    case file_manager::io_task::State::kCancelled:
    case file_manager::io_task::State::kError:
      OnEndUpload(/*url=*/{}, MigrationUploadError::kCopyFailed);
      return;
    case file_manager::io_task::State::kNeedPassword:
      NOTREACHED_IN_MIGRATION()
          << "Encrypted file should not need password to be copied or "
             "moved. Case should not be reached.";
      return;
  }
}

void OdfsSkyvaultUploader::OnMountResponse(base::File::Error result) {
  if (result != base::File::Error::FILE_OK) {
    LOG(ERROR) << "Failed to mount ODFS: " << result;
    OnEndUpload(/*url=*/{}, MigrationUploadError::kServiceUnavailable);
    return;
  }

  StartIOTask();
}

void OdfsSkyvaultUploader::StartIOTask() {
  if (observed_task_id_.has_value()) {
    NOTREACHED_IN_MIGRATION()
        << "The IOTask was already triggered. Case should not be "
           "reached.";
  }

  file_system_provider::ProvidedFileSystemInterface* file_system =
      GetODFS(profile_);
  if (!file_system) {
    // If the file system doesn't exist at this point, then just fail.
    OnEndUpload(/*url=*/{}, MigrationUploadError::kServiceUnavailable);
    return;
  }

  auto destination_folder_path = file_system->GetFileSystemInfo().mount_path();
  if (target_path_.has_value()) {
    CHECK(file_type_ == FileType::kMigration);
    destination_folder_path =
        destination_folder_path.Append(target_path_->value());
  }

  auto destination_folder_url = FilePathToFileSystemURL(
      profile_, file_system_context_, destination_folder_path);
  if (!destination_folder_url.is_valid()) {
    LOG(ERROR) << "Unable to generate destination folder ODFS URL";
    OnEndUpload(/*url=*/{}, MigrationUploadError::kCopyFailed);
    return;
  }

  std::unique_ptr<file_manager::io_task::IOTask> task =
      std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
          file_manager::io_task::OperationType::kMove,
          std::vector<storage::FileSystemURL>{file_system_url_},
          std::move(destination_folder_url), profile_, file_system_context_,
          /*show_notification=*/false);

  observed_task_id_ = io_task_controller_->Add(std::move(task));
}

}  // namespace ash::cloud_upload
