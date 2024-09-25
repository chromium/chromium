// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/skyvault/migration_coordinator.h"

#include <memory>
#include <optional>

#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/ash/policy/skyvault/drive_skyvault_uploader.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/download/download_dir_util.h"
#include "storage/browser/file_system/file_system_url.h"

namespace policy::local_user_files {

namespace {

// Called after `uploader` is fully stopped.
void OnMigrationStopped(std::unique_ptr<MigrationCloudUploader> uploader) {
  VLOG(1) << "Local files migration stopped";
}

// Returns a path combining `destination_dir` with the file's parent path
// relative to MyFiles.
base::FilePath GetDestinationPath(Profile* profile,
                                  const base::FilePath& file_path,
                                  const std::string& destination_dir) {
  base::FilePath my_files_path = GetMyFilesPath(profile);
  base::FilePath destination_path = base::FilePath(destination_dir);
  my_files_path.AppendRelativePath(file_path.DirName(), &destination_path);
  return destination_path;
}

}  // namespace

MigrationCoordinator::MigrationCoordinator(Profile* profile)
    : profile_(profile) {}

MigrationCoordinator::~MigrationCoordinator() = default;

void MigrationCoordinator::Run(CloudProvider cloud_provider,
                               std::vector<base::FilePath> files,
                               const std::string& destination_dir,
                               MigrationDoneCallback callback) {
  CHECK(!uploader_);

  MigrationDoneCallback wrapped_callback =
      base::BindOnce(&MigrationCoordinator::OnMigrationDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  switch (cloud_provider) {
    case CloudProvider::kGoogleDrive:
      uploader_ = std::make_unique<GoogleDriveMigrationUploader>(
          profile_, std::move(files), destination_dir,
          std::move(wrapped_callback));
      break;
    case CloudProvider::kOneDrive:
      uploader_ = std::make_unique<OneDriveMigrationUploader>(
          profile_, std::move(files), destination_dir,
          std::move(wrapped_callback));
      break;
    case CloudProvider::kNotSpecified:
      NOTREACHED_IN_MIGRATION()
          << "Run() should only be called if cloud_provider is specified";
      return;
  }
  uploader_->Run();
}

void MigrationCoordinator::Stop() {
  if (uploader_) {
    MigrationCloudUploader* uploader_ptr = uploader_.get();
    uploader_ptr->Stop(
        base::BindOnce(&OnMigrationStopped, std::move(uploader_)));
  }
}

bool MigrationCoordinator::IsRunning() const {
  return uploader_ != nullptr;
}

void MigrationCoordinator::OnMigrationDone(
    MigrationDoneCallback callback,
    std::map<base::FilePath, MigrationUploadError> errors) {
  uploader_.reset();
  std::move(callback).Run(std::move(errors));
}

MigrationCloudUploader::MigrationCloudUploader(
    Profile* profile,
    std::vector<base::FilePath> files,
    const std::string& destination_dir,
    MigrationDoneCallback callback)
    : profile_(profile),
      files_(std::move(files)),
      destination_dir_(destination_dir),
      callback_(std::move(callback)) {}

MigrationCloudUploader::~MigrationCloudUploader() = default;

OneDriveMigrationUploader::OneDriveMigrationUploader(
    Profile* profile,
    std::vector<base::FilePath> files,
    const std::string& destination_dir,
    MigrationDoneCallback callback)
    : MigrationCloudUploader(profile,
                             std::move(files),
                             destination_dir,
                             std::move(callback)) {}

OneDriveMigrationUploader::~OneDriveMigrationUploader() = default;

void OneDriveMigrationUploader::Run() {
  if (files_.empty()) {
    if (callback_) {
      std::move(callback_).Run({});
    }
    return;
  }
  // TODO(aidazolic): Consider if we can start all jobs at the same time, or we
  // need chunking.
  for (const auto& file_path : files_) {
    // TODO(aidazolic): Ignore files that failed previously.
    base::FilePath target_path =
        GetDestinationPath(profile_, file_path, destination_dir_);
    auto uploader = ash::cloud_upload::OdfsSkyvaultUploader::Upload(
        profile_, file_path, UploadTrigger::kMigration,
        // No need to show progress updates.
        /*progress_callback=*/base::DoNothing(),
        /*upload_callback=*/
        base::BindOnce(&OneDriveMigrationUploader::OnUploadDone,
                       weak_ptr_factory_.GetWeakPtr(), file_path),
        target_path);
    uploaders_.insert({file_path, uploader});
  }
}

void OneDriveMigrationUploader::Stop(base::OnceClosure callback) {
  // TODO(b/349097807): Stop the uploads.
  std::move(callback).Run();
}

void OneDriveMigrationUploader::SetEmulateSlowForTesting(bool value) {
  CHECK_IS_TEST();
  emulate_slow_for_testing_ = value;
}

void OneDriveMigrationUploader::OnUploadDone(
    const base::FilePath& file_path,
    storage::FileSystemURL url,
    std::optional<MigrationUploadError> error) {
  if (error.has_value()) {
    // TODO(aidazolic): UMA.
    // TODO(aidazolic): Persist the failed file to memory.

    // If we only failed to delete the file, don't fail the entire migration
    // because of it.
    if (error != MigrationUploadError::kDeleteFailed) {
      errors_.insert({file_path, error.value()});
    }
  }

  uploaders_.erase(file_path);
  // If all files are done, invoke the callback.
  if (ShouldFinish() && callback_) {
    std::move(callback_).Run(std::move(errors_));
  }
}

bool OneDriveMigrationUploader::ShouldFinish() {
  if (emulate_slow_for_testing_) {
    CHECK_IS_TEST();
    // Do not run the callback.
    return false;
  }

  return uploaders_.empty();
}

GoogleDriveMigrationUploader::GoogleDriveMigrationUploader(
    Profile* profile,
    std::vector<base::FilePath> files,
    const std::string& destination_dir,
    MigrationDoneCallback callback)
    : MigrationCloudUploader(profile,
                             std::move(files),
                             destination_dir,
                             std::move(callback)) {}

GoogleDriveMigrationUploader::~GoogleDriveMigrationUploader() = default;

void GoogleDriveMigrationUploader::Run() {
  if (files_.empty()) {
    if (callback_) {
      std::move(callback_).Run({});
      return;
    }
  }

  // TODO(aidazolic): Consider if we can start all jobs at the same time, or we
  // need chunking.
  for (const auto& file_path : files_) {
    base::FilePath target_path =
        GetDestinationPath(profile_, file_path, destination_dir_);
    std::unique_ptr<DriveSkyvaultUploader> uploader =
        std::make_unique<DriveSkyvaultUploader>(
            profile_, file_path, target_path,
            base::BindOnce(&GoogleDriveMigrationUploader::OnUploadDone,
                           weak_ptr_factory_.GetWeakPtr(), file_path));

    auto uploader_ptr = uploader.get();
    uploaders_.insert({file_path, std::move(uploader)});
    uploader_ptr->Run();
  }
}

void GoogleDriveMigrationUploader::Stop(base::OnceClosure callback) {
  // TODO(b/349097807): Stop IO tasks.
  std::move(callback).Run();
}

void GoogleDriveMigrationUploader::OnUploadDone(
    const base::FilePath& file_path,
    std::optional<MigrationUploadError> error) {
  if (error.has_value()) {
    // TODO(aidazolic): UMA.
    // TODO(aidazolic): Persist the failed file to memory.

    // If we only failed to delete the file, don't fail the entire migration
    // because of it.
    if (error != MigrationUploadError::kDeleteFailed) {
      errors_.insert({file_path, error.value()});
    }
  }

  uploaders_.erase(file_path);
  // If all files are done, invoke the callback.
  if (uploaders_.empty() && callback_) {
    std::move(callback_).Run(errors_);
  }
}

}  // namespace policy::local_user_files
