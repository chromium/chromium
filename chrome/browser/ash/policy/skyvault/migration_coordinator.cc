// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/migration_coordinator.h"

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/skyvault/drive_skyvault_uploader.h"
#include "chrome/browser/ash/policy/skyvault/odfs_skyvault_uploader.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/download/download_dir_util.h"
#include "chrome/browser/profiles/profile.h"
#include "storage/browser/file_system/file_system_url.h"

namespace policy::local_user_files {

namespace {

// Called after `uploader` is fully stopped.
void OnMigrationStopped(std::unique_ptr<MigrationCloudUploader> uploader) {
  VLOG(1) << "Local files migration stopped";
}

// Returns the file's path relative to MyFiles.
base::FilePath GetPathRelativeToMyFiles(Profile* profile,
                                        const base::FilePath& file_path) {
  base::FilePath my_files_path = GetMyFilesPath(profile);
  base::FilePath rel_path;
  my_files_path.AppendRelativePath(file_path.DirName(), &rel_path);
  return rel_path;
}

}  // namespace

MigrationCoordinator::MigrationCoordinator(Profile* profile)
    : profile_(profile) {}

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
          profile_, std::move(files), upload_root, std::move(wrapped_callback));
      break;
    case CloudProvider::kOneDrive:
      uploader_ = std::make_unique<OneDriveMigrationUploader>(
          profile_, std::move(files), upload_root, std::move(wrapped_callback));
      break;
    case CloudProvider::kNotSpecified:
      NOTREACHED()
          << "Run() should only be called if cloud_provider is specified";
  }
  uploader_->Run();
}

void MigrationCoordinator::Cancel() {
  if (uploader_) {
    MigrationCloudUploader* uploader_ptr = uploader_.get();
    uploader_ptr->Cancel(
        base::BindOnce(&OnMigrationStopped, std::move(uploader_)));
  }
}

bool MigrationCoordinator::IsRunning() const {
  return uploader_ != nullptr;
}

void MigrationCoordinator::OnMigrationDone(
    MigrationDoneCallback callback,
    std::map<base::FilePath, MigrationUploadError> errors,
    base::FilePath upload_root_path) {
  uploader_.reset();
  std::move(callback).Run(std::move(errors), upload_root_path);
}

MigrationCloudUploader::MigrationCloudUploader(
    Profile* profile,
    std::vector<base::FilePath> files,
    const std::string& upload_root,
    MigrationDoneCallback callback)
    : profile_(profile),
      files_(std::move(files)),
      upload_root_(upload_root),
      done_callback_(std::move(callback)) {}

MigrationCloudUploader::~MigrationCloudUploader() = default;

OneDriveMigrationUploader::OneDriveMigrationUploader(
    Profile* profile,
    std::vector<base::FilePath> files,
    const std::string& upload_root,
    MigrationDoneCallback callback)
    : MigrationCloudUploader(profile,
                             std::move(files),
                             upload_root,
                             std::move(callback)) {}

OneDriveMigrationUploader::~OneDriveMigrationUploader() = default;

void OneDriveMigrationUploader::Run() {
  if (files_.empty()) {
    if (done_callback_) {
      std::move(done_callback_).Run({}, base::FilePath());
    }
    return;
  }
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
  if (error.has_value()) {
    // TODO(aidazolic): UMA.
    // TODO(aidazolic): Persist the failed file to memory.

    // If we only failed to delete the file, don't fail the entire migration
    // because of it.
    if (error != MigrationUploadError::kDeleteFailed) {
      errors_.insert({file_path, error.value()});
    }
  }

  if (upload_root_path_.empty()) {
    upload_root_path_ = upload_root_path;
  }

  uploaders_.erase(file_path);

  if (!uploaders_.empty()) {
    // Some files are still being uploaded.
    return;
  }
  // If cancelled, invoke the cancelled callback.
  if (cancelled_) {
    if (cancelled_callback_) {
      std::move(cancelled_callback_).Run();
    } else {
      LOG(WARNING) << "Cancelled callback not set.";
    }
    return;
  }
  if (done_callback_) {
    std::move(done_callback_).Run(std::move(errors_), upload_root_path_);
  } else {
    LOG(WARNING) << "Done callback not set.";
  }
}

GoogleDriveMigrationUploader::GoogleDriveMigrationUploader(
    Profile* profile,
    std::vector<base::FilePath> files,
    const std::string& upload_root,
    MigrationDoneCallback callback)
    : MigrationCloudUploader(profile,
                             std::move(files),
                             upload_root,
                             std::move(callback)) {}

GoogleDriveMigrationUploader::~GoogleDriveMigrationUploader() = default;

void GoogleDriveMigrationUploader::Run() {
  if (files_.empty()) {
    if (done_callback_) {
      std::move(done_callback_).Run({}, base::FilePath());
      return;
    }
  }

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
  // TODO(b/349097807): Stop IO tasks.
  std::move(callback).Run();
}

void GoogleDriveMigrationUploader::OnUploadDone(
    const base::FilePath& file_path,
    std::optional<MigrationUploadError> error,
    base::FilePath upload_root_path) {
  if (error.has_value()) {
    // TODO(aidazolic): UMA.
    // TODO(aidazolic): Persist the failed file to memory.

    // If we only failed to delete the file, don't fail the entire migration
    // because of it.
    if (error != MigrationUploadError::kDeleteFailed) {
      errors_.insert({file_path, error.value()});
    }
  }

  // Record the destination path the first time we receive it.
  if (upload_root_path_.empty()) {
    upload_root_path_ = upload_root_path;
  }

  uploaders_.erase(file_path);
  // If all files are done, invoke the callback.
  if (uploaders_.empty() && done_callback_) {
    std::move(done_callback_).Run(errors_, upload_root_path_);
  }
}

}  // namespace policy::local_user_files
