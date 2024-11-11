// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/migration_coordinator.h"

#include <memory>
#include <optional>

#include "base/base_paths.h"
#include "base/check_is_test.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/message_formatter.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/skyvault/drive_skyvault_uploader.h"
#include "chrome/browser/ash/policy/skyvault/histogram_helper.h"
#include "chrome/browser/ash/policy/skyvault/local_files_migration_constants.h"
#include "chrome/browser/ash/policy/skyvault/odfs_skyvault_uploader.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "storage/browser/file_system/file_system_url.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy::local_user_files {

namespace {

// Called after `uploader` is fully stopped.
void OnMigrationStopped(std::unique_ptr<MigrationCloudUploader> uploader,
                        base::OnceClosure cb) {
  VLOG(1) << "Local files migration stopped";
  if (cb) {
    std::move(cb).Run();
  }
}

// Returns the file's path relative to MyFiles.
base::FilePath GetPathRelativeToMyFiles(Profile* profile,
                                        const base::FilePath& file_path) {
  base::FilePath my_files_path = GetMyFilesPath(profile);
  base::FilePath rel_path;
  my_files_path.AppendRelativePath(file_path.DirName(), &rel_path);
  return rel_path;
}

bool ErrorCanBeIgnored(MigrationUploadError error) {
  return error == MigrationUploadError::kDeleteFailed ||
         error == MigrationUploadError::kFileNotFound;
}

std::string FormatErrorMessage(CloudProvider provider,
                               MigrationUploadError error) {
  switch (error) {
    case MigrationUploadError::kCloudQuotaFull:
      return base::UTF16ToUTF8(
          base::i18n::MessageFormatter::FormatWithNumberedArgs(
              l10n_util::GetStringUTF16(
                  IDS_OFFICE_UPLOAD_ERROR_FREE_UP_SPACE_TO_MOVE),
              1,
              l10n_util::GetStringUTF16(
                  provider == CloudProvider::kGoogleDrive
                      ? IDS_OFFICE_CLOUD_PROVIDER_GOOGLE_DRIVE_SHORT
                      : IDS_OFFICE_CLOUD_PROVIDER_ONEDRIVE_SHORT)));
    case MigrationUploadError::kFileNotFound:
      return l10n_util::GetStringUTF8(
          IDS_OFFICE_UPLOAD_ERROR_FILE_NOT_EXIST_TO_MOVE);
    case MigrationUploadError::kInvalidURL:
      return l10n_util::GetStringUTF8(IDS_OFFICE_UPLOAD_ERROR_REJECTED);
    case MigrationUploadError::kAuthRequired:
      return l10n_util::GetStringUTF8(
          IDS_OFFICE_UPLOAD_ERROR_REAUTHENTICATION_REQUIRED);
    case MigrationUploadError::kCopyFailed:
    case MigrationUploadError::kCreateFolderFailed:
    case MigrationUploadError::kSyncFailed:
    case MigrationUploadError::kDeleteFailed:  // should not be logged
    case MigrationUploadError::kMoveFailed:
    case MigrationUploadError::kCancelled:  // should not be logged
    case MigrationUploadError::kUnexpectedError:
    case MigrationUploadError::kServiceUnavailable:
      return l10n_util::GetStringUTF8(IDS_OFFICE_UPLOAD_ERROR_GENERIC);
  }
}

void CloseFile(base::File file) {
  file.Close();
}

base::File CreateOrOpenLogFile(Profile* profile, base::FilePath path) {
  base::File error_log_file_ =
      base::File(path, base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);
  if (!error_log_file_.IsValid()) {
    PLOG(ERROR) << "Failed to open migration error log file.";
  }
  return error_log_file_;
}

void LogError(base::File& error_log_file,
              CloudProvider provider,
              base::FilePath file_path,
              MigrationUploadError error) {
  if (!error_log_file.IsValid()) {
    LOG(ERROR) << "Cannot log error: log file invalid.";
    return;
  }

  std::string log_entry = absl::StrFormat("%s - %s\n", file_path.AsUTF8Unsafe(),
                                          FormatErrorMessage(provider, error));
  error_log_file.WriteAtCurrentPos(log_entry.c_str(), log_entry.size());
}

}  // namespace

MigrationCoordinator::MigrationCoordinator(Profile* profile)
    : profile_(profile) {
  error_log_path_ =
      base::FilePath(kErrorLogFileBasePath).Append(kErrorLogFileName);
}

MigrationCoordinator::~MigrationCoordinator() = default;

void MigrationCoordinator::Run(CloudProvider cloud_provider,
                               std::vector<base::FilePath> files,
                               const std::string& upload_root,
                               MigrationDoneCallback callback) {
  CHECK(!uploader_);

  MigrationDoneCallback wrapped_callback =
      base::BindOnce(&MigrationCoordinator::OnMigrationDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  switch (cloud_provider) {
    case CloudProvider::kGoogleDrive:
      uploader_ = std::make_unique<GoogleDriveMigrationUploader>(
          profile_, std::move(files), upload_root, error_log_path_,
          std::move(wrapped_callback));
      break;
    case CloudProvider::kOneDrive:
      uploader_ = std::make_unique<OneDriveMigrationUploader>(
          profile_, std::move(files), upload_root, error_log_path_,
          std::move(wrapped_callback));
      break;
    case CloudProvider::kNotSpecified:
      NOTREACHED()
          << "Run() should only be called if cloud_provider is specified";
  }
  uploader_->Run();
}

void MigrationCoordinator::Cancel(MigrationStoppedCallback callback) {
  if (uploader_) {
    MigrationCloudUploader* uploader_ptr = uploader_.get();
    uploader_ptr->Cancel(base::BindOnce(&OnMigrationStopped,
                                        std::move(uploader_),
                                        std::move(cancelled_cb_for_testing_)));
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::DeleteFile, error_log_path_), std::move(callback));
}

bool MigrationCoordinator::IsRunning() const {
  return uploader_ != nullptr;
}

void MigrationCoordinator::SetCancelledCallbackForTesting(
    base::OnceClosure cb) {
  CHECK_IS_TEST();
  cancelled_cb_for_testing_ = std::move(cb);
}

void MigrationCoordinator::SetErrorLogPathForTesting(
    const base::FilePath& path) {
  CHECK_IS_TEST();
  error_log_path_ = path;
}

void MigrationCoordinator::OnMigrationDone(
    MigrationDoneCallback callback,
    std::map<base::FilePath, MigrationUploadError> errors,
    base::FilePath upload_root_path,
    base::FilePath error_log_path) {
  uploader_.reset();
  std::move(callback).Run(std::move(errors), upload_root_path, error_log_path);
}

MigrationCloudUploader::MigrationCloudUploader(
    Profile* profile,
    std::vector<base::FilePath> files,
    const std::string& upload_root,
    const base::FilePath& error_log_path,
    MigrationDoneCallback callback)
    : profile_(profile),
      files_(std::move(files)),
      upload_root_(upload_root),
      done_callback_(std::move(callback)),
      error_log_path_(error_log_path),
      log_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {}

MigrationCloudUploader::~MigrationCloudUploader() {
  log_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&CloseFile, std::move(error_log_file_)));
}

void MigrationCloudUploader::Run() {
  if (files_.empty()) {
    if (done_callback_) {
      std::move(done_callback_).Run({}, base::FilePath(), base::FilePath());
    }
    return;
  }

  log_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&CreateOrOpenLogFile, profile_, error_log_path_),
      base::BindOnce(&MigrationCloudUploader::OnLogFileReady,
                     weak_ptr_factory_.GetWeakPtr()));
}

OneDriveMigrationUploader::OneDriveMigrationUploader(
    Profile* profile,
    std::vector<base::FilePath> files,
    const std::string& upload_root,
    const base::FilePath& error_log_path,
    MigrationDoneCallback callback)
    : MigrationCloudUploader(profile,
                             std::move(files),
                             upload_root,
                             error_log_path,
                             std::move(callback)) {}

OneDriveMigrationUploader::~OneDriveMigrationUploader() = default;

void OneDriveMigrationUploader::OnLogFileReady(base::File log_file) {
  error_log_file_ = std::move(log_file);
  // TODO(aidazolic): Consider if we can start all jobs at the same time, or we
  // need chunking.
  for (const auto& file_path : files_) {
    // TODO(aidazolic): Ignore files that failed previously.
    base::FilePath relative_path =
        GetPathRelativeToMyFiles(profile_, file_path);
    auto uploader = ash::cloud_upload::OdfsSkyvaultUploader::Upload(
        profile_, file_path, relative_path, upload_root_,
        UploadTrigger::kMigration,
        // No need to show progress updates.
        /*progress_callback=*/base::DoNothing(),
        /*upload_callback=*/
        base::BindOnce(&OneDriveMigrationUploader::OnUploadDone,
                       weak_ptr_factory_.GetWeakPtr(), file_path));
    uploaders_.insert({file_path, std::move(uploader)});
  }
}

void OneDriveMigrationUploader::Cancel(base::OnceClosure callback) {
  cancelled_callback_ = std::move(callback);
  cancelled_ = true;
  // Create a copy of the keys to iterate over. This is necessary because
  // calling Cancel() on the uploader may trigger OnUploadDone(), which
  // modifies the |uploaders_| map, potentially invalidating iterators.
  std::vector<base::FilePath> file_paths;
  for (const auto& uploader : uploaders_) {
    file_paths.push_back(uploader.first);
  }

  for (const auto& path : file_paths) {
    uploaders_[path]->Cancel();
  }
}

void OneDriveMigrationUploader::OnUploadDone(
    const base::FilePath& file_path,
    storage::FileSystemURL url,
    std::optional<MigrationUploadError> error,
    base::FilePath upload_root_path) {
  if (upload_root_path_.empty()) {
    upload_root_path_ = upload_root_path;
  }

  if (!error.has_value()) {
    OnErrorLogged(file_path);
    return;
  }

  SkyVaultMigrationUploadErrorHistogram(CloudProvider::kOneDrive,
                                        error.value());

  if (!ErrorCanBeIgnored(error.value())) {
    errors_.insert({file_path, error.value()});
  }

  if (!error_log_file_.IsValid()) {
    LOG(ERROR) << "Cannot log error: log file invalid";
    OnErrorLogged(file_path);
    return;
  }

  // TODO(aidazolic): UMA.
  log_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&LogError, std::ref(error_log_file_),
                     CloudProvider::kOneDrive, file_path, error.value()),
      base::BindOnce(&OneDriveMigrationUploader::OnErrorLogged,
                     weak_ptr_factory_.GetWeakPtr(), file_path));
}

void OneDriveMigrationUploader::OnErrorLogged(const base::FilePath& file_path) {
  uploaders_.erase(file_path);

  if (!uploaders_.empty()) {
    // Some files are still being uploaded.
    return;
  }

  if (cancelled_) {
    CHECK(cancelled_callback_);
    std::move(cancelled_callback_).Run();
    return;
  }
  CHECK(done_callback_);
  // TODO(aidazolic): What to do if the log file isn't valid?
  std::move(done_callback_)
      .Run(std::move(errors_), upload_root_path_, error_log_path_);
}

GoogleDriveMigrationUploader::GoogleDriveMigrationUploader(
    Profile* profile,
    std::vector<base::FilePath> files,
    const std::string& upload_root,
    const base::FilePath& error_log_path,
    MigrationDoneCallback callback)
    : MigrationCloudUploader(profile,
                             std::move(files),
                             upload_root,
                             error_log_path,
                             std::move(callback)) {}

GoogleDriveMigrationUploader::~GoogleDriveMigrationUploader() = default;

void GoogleDriveMigrationUploader::OnLogFileReady(base::File log_file) {
  error_log_file_ = std::move(log_file);
  // TODO(aidazolic): Consider if we can start all jobs at the same time, or we
  // need chunking.
  for (const auto& file_path : files_) {
    base::FilePath target_path = GetPathRelativeToMyFiles(profile_, file_path);
    std::unique_ptr<DriveSkyvaultUploader> uploader =
        std::make_unique<DriveSkyvaultUploader>(
            profile_, file_path, target_path, upload_root_,
            base::BindOnce(&GoogleDriveMigrationUploader::OnUploadDone,
                           weak_ptr_factory_.GetWeakPtr(), file_path));

    auto uploader_ptr = uploader.get();
    uploaders_.insert({file_path, std::move(uploader)});
    uploader_ptr->Run();
  }
}

void GoogleDriveMigrationUploader::Cancel(base::OnceClosure callback) {
  cancelled_callback_ = std::move(callback);
  cancelled_ = true;
  // Create a copy of the keys to iterate over. This is necessary because
  // calling Cancel() on the uploader may trigger OnUploadDone(), which
  // modifies the |uploaders_| map, potentially invalidating iterators.
  std::vector<base::FilePath> file_paths;
  for (const auto& uploader : uploaders_) {
    file_paths.push_back(uploader.first);
  }

  for (const auto& path : file_paths) {
    uploaders_[path]->Cancel();
  }
}

void GoogleDriveMigrationUploader::OnUploadDone(
    const base::FilePath& file_path,
    std::optional<MigrationUploadError> error,
    base::FilePath upload_root_path) {
  // Record the destination path the first time we receive it.
  if (upload_root_path_.empty()) {
    upload_root_path_ = upload_root_path;
  }

  if (!error.has_value()) {
    OnErrorLogged(file_path);
    return;
  }

  SkyVaultMigrationUploadErrorHistogram(CloudProvider::kGoogleDrive,
                                        error.value());

  if (!ErrorCanBeIgnored(error.value())) {
    errors_.insert({file_path, error.value()});
  }

  if (!error_log_file_.IsValid()) {
    LOG(ERROR) << "Cannot log error: log file invalid";
    OnErrorLogged(file_path);
    return;
  }

  // TODO(aidazolic): UMA.
  log_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&LogError, std::ref(error_log_file_),
                     CloudProvider::kGoogleDrive, file_path, error.value()),
      base::BindOnce(&GoogleDriveMigrationUploader::OnErrorLogged,
                     weak_ptr_factory_.GetWeakPtr(), file_path));
}

void GoogleDriveMigrationUploader::OnErrorLogged(
    const base::FilePath& file_path) {
  uploaders_.erase(file_path);

  if (!uploaders_.empty()) {
    // Some files are still being uploaded.
    return;
  }

  if (cancelled_) {
    CHECK(cancelled_callback_);
    std::move(cancelled_callback_).Run();
    return;
  }
  CHECK(done_callback_);
  // TODO(aidazolic): What to do if the log file isn't valid?
  std::move(done_callback_)
      .Run(std::move(errors_), upload_root_path_, error_log_path_);
}

}  // namespace policy::local_user_files
