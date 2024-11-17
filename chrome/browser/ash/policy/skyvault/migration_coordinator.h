// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_MIGRATION_COORDINATOR_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_MIGRATION_COORDINATOR_H_

#include <map>
#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/policy/skyvault/drive_skyvault_uploader.h"
#include "chrome/browser/ash/policy/skyvault/odfs_skyvault_uploader.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "storage/browser/file_system/file_system_url.h"

namespace policy::local_user_files {

// Callback used to signal that all uploads completed (successfully or not).
// Parameters:
//  errors: Map of source file paths to upload errors (empty if no errors).
//  upload_root_path: Path to the upload root, or empty on early failure.
//  error_log_path: Path to the error log file (empty if no errors).
using MigrationDoneCallback =
    base::OnceCallback<void(std::map<base::FilePath, MigrationUploadError>,
                            base::FilePath,
                            base::FilePath)>;

// Callback to signal that migration has completely stopped and can be
// restarted.
using MigrationStoppedCallback = base::OnceCallback<void(bool)>;

class MigrationCloudUploader;

// Handles the upload of local files to a specified cloud storage destination.
// This class provides a generic interface for initiating, stopping, and
// monitoring uploads. The specific implementation for each cloud provider is
// handled by derived classes of MigrationCloudUploader.
class MigrationCoordinator {
 public:
  explicit MigrationCoordinator(Profile* profile);
  MigrationCoordinator(const MigrationCoordinator&) = delete;
  MigrationCoordinator& operator=(const MigrationCoordinator&) = delete;
  virtual ~MigrationCoordinator();

  // Starts the upload of files specified by `source_urls` to the
  // `upload_root` directory on `cloud_provider`. Invokes `callback` upon
  // completion, passing any errors that occurred and the absolute path to the
  // root upload directory. Fails if a migration is already in progress.
  virtual void Run(CloudProvider cloud_provider,
                   std::vector<base::FilePath> files,
                   const std::string& upload_root,
                   MigrationDoneCallback callback);

  // Cancels any ongoing file uploads.
  virtual void Cancel(MigrationStoppedCallback callback);

  // Returns whether any file uploads are currently in progress.
  virtual bool IsRunning() const;

  // Sets the `cb` to be invoked when all the uploads are stopped.
  void SetCancelledCallbackForTesting(base::OnceClosure cb);
  void SetErrorLogPathForTesting(const base::FilePath& path);

 private:
  // Called after underlying upload operation completes.
  virtual void OnMigrationDone(
      MigrationDoneCallback callback,
      std::map<base::FilePath, MigrationUploadError> errors,
      base::FilePath upload_root_path,
      base::FilePath error_log_path);

  // Profile for which this instance was created.
  raw_ptr<Profile> profile_;

  base::FilePath error_log_path_;

  // The implementation of the upload process, specific to the
  // `cloud_provider` argument passed to the `Run` method.
  std::unique_ptr<MigrationCloudUploader> uploader_ = nullptr;

  // If set, invoked when all the uploaders are stopped. Used in tests.
  base::OnceClosure cancelled_cb_for_testing_;

  base::WeakPtrFactory<MigrationCoordinator> weak_ptr_factory_{this};
};

// Abstract class for the implementation of file uploads to a specific cloud
// storage destination. Derived classes provide the concrete logic for
// interacting with the respective cloud provider.
class MigrationCloudUploader {
 public:
  MigrationCloudUploader(Profile* profile,
                         std::vector<base::FilePath> files,
                         const std::string& upload_root,
                         const base::FilePath& error_log_path,
                         MigrationDoneCallback callback);
  MigrationCloudUploader(const MigrationCloudUploader&) = delete;
  MigrationCloudUploader& operator=(const MigrationCloudUploader&) = delete;
  virtual ~MigrationCloudUploader();

  // Starts the upload of files to the relevant cloud location. Invokes
  // `callback_` upon completion.
  void Run();

  // Cancels any ongoing file uploads.
  virtual void Cancel(base::OnceClosure cancelled_callback) = 0;

 protected:
  // Called after opening the log file.
  virtual void OnLogFileReady(base::File log_file) = 0;

  // Called after the error for `file_path` was logged.
  virtual void OnErrorLogged(const base::FilePath& file_path) = 0;

  // Maps file to their upload errors, if any.
  std::map<base::FilePath, MigrationUploadError> errors_;

  // Profile for which this instance was created.
  const raw_ptr<Profile> profile_;
  // The paths of the files or directories to be uploaded.
  const std::vector<base::FilePath> files_;
  // The name of the device-unique upload root folder on Drive
  const std::string upload_root_;
  // Absolute path to the device's upload root folder on Drive. This is
  // populated after the first successful upload.
  base::FilePath upload_root_path_;
  // Callback to run after all uploads finish.
  MigrationDoneCallback done_callback_;
  // Callback to run after all uploads are cancelled.
  base::OnceClosure cancelled_callback_;
  // Indicates that the upload was cancelled, e.g. by a policy change.
  bool cancelled_ = false;
  // Error logging:
  base::FilePath error_log_path_;
  // This file is created/destructed and accessed only on `log_tast_runner_`.
  base::File error_log_file_;
  scoped_refptr<base::SequencedTaskRunner> log_task_runner_;

  base::WeakPtrFactory<MigrationCloudUploader> weak_ptr_factory_{this};
};

// Migration file uploader for uploads to Microsoft OneDrive.
class OneDriveMigrationUploader : public MigrationCloudUploader {
 public:
  OneDriveMigrationUploader(Profile* profile,
                            std::vector<base::FilePath> files,
                            const std::string& upload_root,
                            const base::FilePath& error_log_path,
                            MigrationDoneCallback callback);
  OneDriveMigrationUploader(const OneDriveMigrationUploader&) = delete;
  OneDriveMigrationUploader& operator=(const OneDriveMigrationUploader&) =
      delete;
  ~OneDriveMigrationUploader() override;

  // MigrationCloudUploader overrides:
  void Cancel(base::OnceClosure cancelled_callback) override;

 private:
  // Called when one upload operation completes.
  void OnUploadDone(const base::FilePath& file_path,
                    storage::FileSystemURL url,
                    std::optional<MigrationUploadError> error,
                    base::FilePath upload_root_path);

  // MigrationCloudUploader overrides:
  void OnLogFileReady(base::File log_file) override;
  void OnErrorLogged(const base::FilePath& file_path) override;

  // Maps source urls of files being uploaded to corresponding
  // OdfsSkyvaultUploader instances. Keeps a weak reference as lifetime of
  // OdfsSkyvaultUploader is managed by its action.
  std::map<base::FilePath,
           base::WeakPtr<ash::cloud_upload::OdfsSkyvaultUploader>>
      uploaders_;

  // Flag to indicate that Run() method should wait.
  bool emulate_slow_for_testing_ = false;

  base::WeakPtrFactory<OneDriveMigrationUploader> weak_ptr_factory_{this};
};

// Migration file uploader for uploads to Google Drive.
class GoogleDriveMigrationUploader : public MigrationCloudUploader {
 public:
  GoogleDriveMigrationUploader(Profile* profile,
                               std::vector<base::FilePath> files,
                               const std::string& upload_root,
                               const base::FilePath& error_log_path,
                               MigrationDoneCallback callback);
  GoogleDriveMigrationUploader(const GoogleDriveMigrationUploader&) = delete;
  GoogleDriveMigrationUploader& operator=(const GoogleDriveMigrationUploader&) =
      delete;
  ~GoogleDriveMigrationUploader() override;

  // MigrationCloudUploader overrides:
  void Cancel(base::OnceClosure cancelled_callback) override;

 private:
  void OnUploadDone(const base::FilePath& file_path,
                    std::optional<MigrationUploadError> error,
                    base::FilePath upload_root_path);

  // MigrationCloudUploader overrides:
  void OnLogFileReady(base::File log_file) override;
  void OnErrorLogged(const base::FilePath& file_path) override;

  // Maps source urls of files being uploaded to corresponding
  // DriveSkyvaultUploader instances.
  std::map<base::FilePath, std::unique_ptr<DriveSkyvaultUploader>> uploaders_;

  base::WeakPtrFactory<GoogleDriveMigrationUploader> weak_ptr_factory_{this};
};

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_MIGRATION_COORDINATOR_H_
