// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/odfs_skyvault_uploader.h"

#include <optional>

#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/skyvault/histogram_helper.h"
#include "chrome/browser/ash/policy/skyvault/migration_notification_manager.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ash/policy/skyvault/signin_notification_helper.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "storage/browser/file_system/file_system_url.h"

namespace ash::cloud_upload {

namespace {

// A factory that can be injected in tests.
static OdfsMigrationUploader::FactoryCallback g_testing_factory_ =
    OdfsMigrationUploader::FactoryCallback();

// Runs the upload callback provided to `OdfsSkyvaultUploader::Upload`.
void OnUploadDone(
    scoped_refptr<OdfsSkyvaultUploader> odfs_skyvault_uploader,
    base::OnceCallback<void(bool, storage::FileSystemURL)> upload_callback,
    storage::FileSystemURL file_url,
    std::optional<MigrationUploadError> error,
    base::FilePath upload_root_path) {
  std::move(upload_callback).Run(!error.has_value(), std::move(file_url));
}

// Runs the upload callback provided to `OdfsSkyvaultUploader::Upload`.
void OnUploadDoneWithError(
    scoped_refptr<OdfsSkyvaultUploader> odfs_skyvault_uploader,
    OdfsMigrationUploader::UploadDoneCallback upload_callback,
    storage::FileSystemURL file_url,
    std::optional<MigrationUploadError> error,
    base::FilePath upload_root_path) {
  std::move(upload_callback).Run(std::move(file_url), error, upload_root_path);
}

static int64_t g_id_counter = 0;

}  // namespace

// static.
base::WeakPtr<OdfsSkyvaultUploader> OdfsSkyvaultUploader::Upload(
    Profile* profile,
    const base::FilePath& path,
    UploadTrigger trigger,
    base::RepeatingCallback<void(int64_t)> progress_callback,
    base::OnceCallback<void(bool, storage::FileSystemURL)> upload_callback,
    std::optional<const gfx::Image> thumbnail) {
  auto* file_system_context =
      file_manager::util::GetFileManagerFileSystemContext(profile);
  DCHECK(file_system_context);
  base::FilePath tmp_dir;
  CHECK((base::GetTempDir(&tmp_dir) && tmp_dir.IsParent(path)) ||
        trigger == UploadTrigger::kMigration);
  auto file_system_url = file_system_context->CreateCrackedFileSystemURL(
      blink::StorageKey(), storage::kFileSystemTypeLocal, path);
  scoped_refptr<OdfsSkyvaultUploader> odfs_skyvault_uploader =
      new OdfsSkyvaultUploader(profile, ++g_id_counter, file_system_url,
                               trigger, std::move(progress_callback),
                               thumbnail);

  // Keep `odfs_skyvault_uploader` alive until the upload is done.
  odfs_skyvault_uploader->Run(base::BindOnce(
      &OnUploadDone, odfs_skyvault_uploader, std::move(upload_callback)));
  return odfs_skyvault_uploader->GetWeakPtr();
}

// static.
base::WeakPtr<OdfsSkyvaultUploader> OdfsSkyvaultUploader::Upload(
    Profile* profile,
    const base::FilePath& path,
    const base::FilePath& relative_source_path,
    const std::string& upload_root,
    UploadTrigger trigger,
    base::RepeatingCallback<void(int64_t)> progress_callback,
    UploadDoneCallback upload_callback_with_error) {
  auto* file_system_context =
      file_manager::util::GetFileManagerFileSystemContext(profile);
  DCHECK(file_system_context);
  base::FilePath tmp_dir;
  auto file_system_url = file_system_context->CreateCrackedFileSystemURL(
      blink::StorageKey(), storage::kFileSystemTypeLocal, path);

  scoped_refptr<OdfsSkyvaultUploader> odfs_skyvault_uploader;
  switch (trigger) {
    case UploadTrigger::kDownload:
    case UploadTrigger::kScreenCapture:
      CHECK(base::GetTempDir(&tmp_dir) && tmp_dir.IsParent(path));
      odfs_skyvault_uploader = new OdfsSkyvaultUploader(
          profile, ++g_id_counter, file_system_url, trigger,
          std::move(progress_callback), std::nullopt);
      break;
    case UploadTrigger::kMigration:
      odfs_skyvault_uploader = OdfsMigrationUploader::Create(
          profile, ++g_id_counter, file_system_url, relative_source_path,
          upload_root);
      break;
  }

  // Keep `odfs_skyvault_uploader` alive until the upload is done.
  odfs_skyvault_uploader->Run(
      base::BindOnce(&OnUploadDoneWithError, odfs_skyvault_uploader,
                     std::move(upload_callback_with_error)));
  return odfs_skyvault_uploader->GetWeakPtr();
}

base::WeakPtr<OdfsSkyvaultUploader> OdfsSkyvaultUploader::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void OdfsSkyvaultUploader::Cancel() {
  cancelled_ = true;
  if (observed_task_id_.has_value()) {
    io_task_controller_->Cancel(observed_task_id_.value());
  }
}

OdfsSkyvaultUploader::OdfsSkyvaultUploader(
    Profile* profile,
    int64_t id,
    const storage::FileSystemURL& file_system_url,
    UploadTrigger trigger,
    base::RepeatingCallback<void(int64_t)> progress_callback,
    std::optional<const gfx::Image> thumbnail)
    : profile_(profile),
      file_system_context_(
          file_manager::util::GetFileManagerFileSystemContext(profile)),
      id_(id),
      file_system_url_(file_system_url),
      trigger_(trigger),
      progress_callback_(std::move(progress_callback)),
      thumbnail_(thumbnail) {}

OdfsSkyvaultUploader::~OdfsSkyvaultUploader() {
  // Stop observing IO task updates.
  if (io_task_controller_) {
    io_task_controller_->RemoveObserver(this);
  }
}

base::FilePath OdfsSkyvaultUploader::GetDestinationFolderPath(
    file_system_provider::ProvidedFileSystemInterface* file_system) {
  return file_system->GetFileSystemInfo().mount_path();
}

void OdfsSkyvaultUploader::RequestSignIn(
    base::OnceCallback<void(base::File::Error)> on_sign_in_cb) {
  policy::skyvault_ui_utils::ShowSignInNotification(
      profile_, id_, trigger_, file_system_url_.path(),
      std::move(on_sign_in_cb), thumbnail_);
}

void OdfsSkyvaultUploader::Run(UploadDoneCallback upload_callback) {
  upload_callback_ = std::move(upload_callback);

  if (cancelled_) {
    OnEndUpload(/*url=*/{}, MigrationUploadError::kCancelled);
    return;
  }

  if (!profile_) {
    LOG(ERROR) << "No profile";
    OnEndUpload(/*url=*/{}, MigrationUploadError::kUnexpectedError);
    return;
  }

  file_manager::VolumeManager* volume_manager =
      (file_manager::VolumeManager::Get(profile_));
  if (!volume_manager) {
    LOG(ERROR) << "No volume manager";
    OnEndUpload(/*url=*/{}, MigrationUploadError::kUnexpectedError);
    return;
  }
  io_task_controller_ = volume_manager->io_task_controller();
  if (!io_task_controller_) {
    LOG(ERROR) << "No task_controller";
    OnEndUpload(/*url=*/{}, MigrationUploadError::kUnexpectedError);
    return;
  }

  // Observe IO tasks updates.
  io_task_controller_->AddObserver(this);

  GetODFSMetadataAndStartIOTask();
}

void OdfsSkyvaultUploader::OnEndUpload(
    storage::FileSystemURL url,
    std::optional<MigrationUploadError> error) {
  // TODO(b/343879839): Error UMA.
  if (upload_callback_) {
    std::move(upload_callback_).Run(std::move(url), error, upload_root_path_);
  }
}

void OdfsSkyvaultUploader::GetODFSMetadataAndStartIOTask() {
  file_system_provider::ProvidedFileSystemInterface* file_system =
      GetODFS(profile_);
  if (!file_system) {
    RequestSignIn(base::BindOnce(&OdfsSkyvaultUploader::OnMountResponse,
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
    RequestSignIn(base::BindOnce(&OdfsSkyvaultUploader::OnMountResponse,
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
      OnEndUpload(/*url=*/{}, MigrationUploadError::kCancelled);
      return;
    case file_manager::io_task::State::kError:
      ProcessError(status);
      return;
    case file_manager::io_task::State::kNeedPassword:
      NOTREACHED() << "Encrypted file should not need password to be copied or "
                      "moved. Case should not be reached.";
  }
}

void OdfsSkyvaultUploader::ProcessError(
    const ::file_manager::io_task::ProgressStatus& status) {
  // It's always one file.
  DCHECK_EQ(status.sources.size(), 1u);
  DCHECK_EQ(status.outputs.size(), 1u);
  DCHECK_EQ(status.state, file_manager::io_task::State::kError);

  base::File::Error error =
      status.outputs.front().error.value_or(base::File::FILE_ERROR_FAILED);
  MigrationUploadError upload_error = MigrationUploadError::kMoveFailed;

  switch (error) {
    case base::File::FILE_ERROR_NOT_FOUND:
      upload_error = MigrationUploadError::kFileNotFound;
      break;
    case base::File::FILE_ERROR_ACCESS_DENIED:
      // TODO(aidazolic): Maybe ask for reauth again.
      upload_error = MigrationUploadError::kAuthRequired;
      break;
    case base::File::FILE_ERROR_NO_SPACE:
      upload_error = MigrationUploadError::kCloudQuotaFull;
      break;
    case base::File::FILE_ERROR_INVALID_URL:
      upload_error = MigrationUploadError::kInvalidURL;
      break;
    default:
      break;
  }

  OnEndUpload(/*url=*/{}, upload_error);
}

void OdfsSkyvaultUploader::OnMountResponse(base::File::Error result) {
  if (cancelled_) {
    OnEndUpload(/*url=*/{}, MigrationUploadError::kCancelled);
    return;
  }

  const bool sign_in_error = result != base::File::Error::FILE_OK;
  policy::local_user_files::SkyVaultOneDriveSignInErrorHistogram(trigger_,
                                                                 sign_in_error);

  if (sign_in_error) {
    LOG(ERROR) << "Failed to mount ODFS: " << result;
    OnEndUpload(/*url=*/{}, MigrationUploadError::kServiceUnavailable);
    return;
  }

  StartIOTask();
}

void OdfsSkyvaultUploader::StartIOTask() {
  if (observed_task_id_.has_value()) {
    NOTREACHED()
        << "The IOTask was already triggered. Case should not be reached.";
  }

  if (cancelled_) {
    OnEndUpload(/*url=*/{}, MigrationUploadError::kCancelled);
    return;
  }

  file_system_provider::ProvidedFileSystemInterface* file_system =
      GetODFS(profile_);
  if (!file_system) {
    // If the file system doesn't exist at this point, then just fail.
    OnEndUpload(/*url=*/{}, MigrationUploadError::kServiceUnavailable);
    return;
  }

  auto destination_folder_path = GetDestinationFolderPath(file_system);

  auto destination_folder_url = FilePathToFileSystemURL(
      profile_, file_system_context_, destination_folder_path);
  if (!destination_folder_url.is_valid()) {
    LOG(ERROR) << "Unable to generate destination folder ODFS URL";
    OnEndUpload(/*url=*/{}, MigrationUploadError::kServiceUnavailable);
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

// =========
// MIGRATION
// =========

// static
scoped_refptr<OdfsMigrationUploader> OdfsMigrationUploader::Create(
    Profile* profile,
    int64_t id,
    const storage::FileSystemURL& file_system_url,
    const base::FilePath& relative_source_path,
    const std::string& upload_root) {
  if (g_testing_factory_) {
    CHECK_IS_TEST();
    return g_testing_factory_.Run(profile, id, file_system_url,
                                  relative_source_path);
  }
  return new OdfsMigrationUploader(profile, id, file_system_url,
                                   relative_source_path, upload_root);
}

// static
void OdfsMigrationUploader::SetFactoryForTesting(FactoryCallback factory) {
  CHECK_IS_TEST();
  g_testing_factory_ = factory;
}

OdfsMigrationUploader::OdfsMigrationUploader(
    Profile* profile,
    int64_t id,
    const storage::FileSystemURL& file_system_url,
    const base::FilePath& relative_source_path,
    const std::string& upload_root)
    : OdfsSkyvaultUploader(profile,
                           id,
                           file_system_url,
                           UploadTrigger::kMigration,
                           /*progress_callback=*/base::DoNothing(),
                           std::nullopt),
      relative_source_path_(relative_source_path),
      upload_root_(upload_root) {}

OdfsMigrationUploader::~OdfsMigrationUploader() = default;

base::FilePath OdfsMigrationUploader::GetDestinationFolderPath(
    file_system_provider::ProvidedFileSystemInterface* file_system) {
  upload_root_path_ =
      OdfsSkyvaultUploader::GetDestinationFolderPath(file_system)
          .Append(upload_root_);
  return upload_root_path_.Append(relative_source_path_);
}

void OdfsMigrationUploader::RequestSignIn(
    base::OnceCallback<void(base::File::Error)> on_sign_in_cb) {
  policy::local_user_files::MigrationNotificationManager* notification_manager =
      policy::local_user_files::MigrationNotificationManagerFactory::
          GetForBrowserContext(profile_);
  CHECK(notification_manager);
  subscription_ = notification_manager->ShowOneDriveSignInNotification(
      std::move(on_sign_in_cb));
}

}  // namespace ash::cloud_upload
