// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/odfs_skyvault_uploader.h"

#include <optional>

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
    bool success,
    storage::FileSystemURL file_url) {
  std::move(upload_callback).Run(success, std::move(file_url));
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
  DCHECK(base::GetTempDir(&tmp_dir) && tmp_dir.IsParent(path));
  auto file_system_url = file_system_context->CreateCrackedFileSystemURL(
      blink::StorageKey(), storage::kFileSystemTypeLocal, path);
  scoped_refptr<OdfsSkyvaultUploader> odfs_skyvault_uploader =
      new OdfsSkyvaultUploader(profile, ++g_id_counter, file_system_url,
                               file_type, std::move(progress_callback));

  // Keep `odfs_skyvault_uploader` alive until the upload is done.
  odfs_skyvault_uploader->Run(base::BindOnce(
      &OnUploadDone, odfs_skyvault_uploader, std::move(upload_callback)));
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
    base::RepeatingCallback<void(int64_t)> progress_callback)
    : profile_(profile),
      file_system_context_(
          file_manager::util::GetFileManagerFileSystemContext(profile)),
      id_(id),
      file_system_url_(file_system_url),
      file_type_(file_type),
      progress_callback_(std::move(progress_callback)) {}

OdfsSkyvaultUploader::~OdfsSkyvaultUploader() {
  // Stop observing IO task updates.
  if (io_task_controller_) {
    io_task_controller_->RemoveObserver(this);
  }
}

void OdfsSkyvaultUploader::Run(
    base::OnceCallback<void(bool, storage::FileSystemURL)> upload_callback) {
  upload_callback_ = std::move(upload_callback);

  if (!profile_) {
    LOG(ERROR) << "No profile";
    OnEndUpload(/*success=*/false, /*url=*/{});
    return;
  }

  file_manager::VolumeManager* volume_manager =
      (file_manager::VolumeManager::Get(profile_));
  if (!volume_manager) {
    LOG(ERROR) << "No volume manager";
    OnEndUpload(/*success=*/false, /*url=*/{});
    return;
  }
  io_task_controller_ = volume_manager->io_task_controller();
  if (!io_task_controller_) {
    LOG(ERROR) << "No task_controller";
    OnEndUpload(/*success=*/false, /*url=*/{});
    return;
  }

  // Observe IO tasks updates.
  io_task_controller_->AddObserver(this);

  GetODFSMetadataAndStartIOTask();
}

void OdfsSkyvaultUploader::OnEndUpload(bool success,
                                       storage::FileSystemURL url) {
  std::move(upload_callback_).Run(success, std::move(url));
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
      OnEndUpload(/*success=*/true, status.outputs[0].url);
      return;
    case file_manager::io_task::State::kCancelled:
    case file_manager::io_task::State::kError:
      OnEndUpload(/*success=*/false, /*url=*/{});
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
    OnEndUpload(/*success=*/false, /*url=*/{});
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
    OnEndUpload(/*success=*/false, /*url=*/{});
    return;
  }

  auto destination_folder_path = file_system->GetFileSystemInfo().mount_path();
  auto destination_folder_url = FilePathToFileSystemURL(
      profile_, file_system_context_, destination_folder_path);
  if (!destination_folder_url.is_valid()) {
    LOG(ERROR) << "Unable to generate destination folder ODFS URL";
    OnEndUpload(/*success=*/false, /*url=*/{});
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
