// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/odfs_skyvault_uploader.h"

#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"

namespace ash::cloud_upload {

namespace {

// Runs the upload callback provided to `OdfsSkyvaultUploader::Upload`.
void OnUploadDone(scoped_refptr<OdfsSkyvaultUploader> odfs_skyvault_uploader,
                  base::OnceCallback<void(bool)> upload_callback,
                  bool success) {
  std::move(upload_callback).Run(success);
}

}  // namespace

// static.
void OdfsSkyvaultUploader::Upload(
    Profile* profile,
    const base::FilePath& file_path,
    FileType file_type,
    base::RepeatingCallback<void(int)> progress_callback,
    base::OnceCallback<void(bool)> upload_callback) {
  scoped_refptr<OdfsSkyvaultUploader> odfs_skyvault_uploader =
      new OdfsSkyvaultUploader(profile, file_path, file_type,
                               std::move(progress_callback));

  // Keep `odfs_skyvault_uploader` alive until the upload is done.
  odfs_skyvault_uploader->Run(base::BindOnce(
      &OnUploadDone, odfs_skyvault_uploader, std::move(upload_callback)));
}

OdfsSkyvaultUploader::OdfsSkyvaultUploader(
    Profile* profile,
    const base::FilePath& file_path,
    FileType file_type,
    base::RepeatingCallback<void(int)> progress_callback)
    : profile_(profile),
      file_system_context_(
          file_manager::util::GetFileManagerFileSystemContext(profile)),
      local_file_path_(file_path),
      file_type_(file_type),
      progress_callback_(std::move(progress_callback)) {}

OdfsSkyvaultUploader::~OdfsSkyvaultUploader() {
  // Stop observing IO task updates.
  if (io_task_controller_) {
    io_task_controller_->RemoveObserver(this);
  }
}

void OdfsSkyvaultUploader::Run(base::OnceCallback<void(bool)> upload_callback) {
  upload_callback_ = std::move(upload_callback);

  if (!profile_) {
    LOG(ERROR) << "No profile";
    OnEndUpload(/*success=*/false);
    return;
  }

  file_manager::VolumeManager* volume_manager =
      (file_manager::VolumeManager::Get(profile_));
  if (!volume_manager) {
    LOG(ERROR) << "No volume manager";
    OnEndUpload(/*success=*/false);
    return;
  }
  io_task_controller_ = volume_manager->io_task_controller();
  if (!io_task_controller_) {
    LOG(ERROR) << "No task_controller";
    OnEndUpload(/*success=*/false);
    return;
  }

  // Observe IO tasks updates.
  io_task_controller_->AddObserver(this);

  GetODFSMetadataAndStartIOTask();
}

void OdfsSkyvaultUploader::OnEndUpload(bool success) {
  std::move(upload_callback_).Run(success);
}

void OdfsSkyvaultUploader::GetODFSMetadataAndStartIOTask() {
  file_system_provider::ProvidedFileSystemInterface* file_system =
      GetODFS(profile_);
  if (!file_system) {
    LOG(ERROR) << "ODFS not found";
    OnEndUpload(/*success=*/false);
    return;
  }

  auto destination_folder_path = file_system->GetFileSystemInfo().mount_path();
  storage::FileSystemURL destination_folder_url = FilePathToFileSystemURL(
      profile_, file_system_context_, destination_folder_path);
  if (!destination_folder_url.is_valid()) {
    LOG(ERROR) << "Unable to generate destination folder ODFS URL";
    OnEndUpload(/*success=*/false);
    return;
  }

  // First check that ODFS is not in the "ReauthenticationRequired" state.
  GetODFSMetadata(
      file_system,
      base::BindOnce(&OdfsSkyvaultUploader::CheckReauthenticationAndStartIOTask,
                     weak_ptr_factory_.GetWeakPtr(), destination_folder_url));
}

void OdfsSkyvaultUploader::CheckReauthenticationAndStartIOTask(
    const storage::FileSystemURL& destination_folder_url,
    base::expected<ODFSMetadata, base::File::Error> metadata_or_error) {
  if (!metadata_or_error.has_value()) {
    // Try the move anyway.
    LOG(ERROR) << "Failed to get reauthentication required state: "
               << metadata_or_error.error();
  } else if (metadata_or_error->reauthentication_required) {
    // TODO(b/340451159): Show notification asking the user to mount or sign-in.
  }

  storage::FileSystemURL source_url =
      FilePathToFileSystemURL(profile_, file_system_context_, local_file_path_);
  std::vector<storage::FileSystemURL> source_urls{source_url};
  std::unique_ptr<file_manager::io_task::IOTask> task =
      std::make_unique<file_manager::io_task::CopyOrMoveIOTask>(
          file_manager::io_task::OperationType::kMove, std::move(source_urls),
          std::move(destination_folder_url), profile_, file_system_context_,
          /*show_notification=*/false);

  observed_task_id_ = io_task_controller_->Add(std::move(task));
}

void OdfsSkyvaultUploader::OnIOTaskStatus(
    const file_manager::io_task::ProgressStatus& status) {
  if (status.task_id != observed_task_id_) {
    return;
  }
  switch (status.state) {
    case file_manager::io_task::State::kInProgress:
      if (status.bytes_transferred > 0) {
        progress_callback_.Run(100 * status.bytes_transferred /
                               status.total_bytes);
      }
      return;
    case file_manager::io_task::State::kPaused:
    case file_manager::io_task::State::kScanning:
    case file_manager::io_task::State::kQueued:
      return;
    case file_manager::io_task::State::kSuccess:
      progress_callback_.Run(100);
      OnEndUpload(/*success=*/true);
      return;
    case file_manager::io_task::State::kCancelled:
    case file_manager::io_task::State::kError:
      OnEndUpload(/*success=*/false);
      return;
    case file_manager::io_task::State::kNeedPassword:
      NOTREACHED_IN_MIGRATION()
          << "Encrypted file should not need password to be copied or "
             "moved. Case should not be reached.";
      return;
  }
}

}  // namespace ash::cloud_upload
