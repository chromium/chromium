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
#include "chrome/browser/ash/policy/skyvault/drive_skyvault_uploader.h"
#include "chrome/browser/ash/policy/skyvault/odfs_skyvault_uploader.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "storage/browser/file_system/file_system_url.h"

namespace policy::local_user_files {

// Callback used to signal that all uploads completed (successfully or not).
using MigrationDoneCallback =
    base::OnceCallback<void(std::map<base::FilePath, MigrationUploadError>)>;

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
  // `destination_dir` directory in the cloud storage location specified by
  // `cloud_provider`. The `callback` will be invoked upon completion,
  // indicating whether the migration was successful. Fails if a migration is
  // already in progress.
  virtual void Run(CloudProvider cloud_provider,
                   std::vector<base::FilePath> files,
                   const std::string& destination_dir,
                   MigrationDoneCallback callback);

  // Stops any ongoing file uploads.
  virtual void Stop();

  // Returns whether any file uploads are currently in progress.
  virtual bool IsRunning() const;

 private:
  // Called after underlying upload operation completes.
  virtual void OnMigrationDone(
      MigrationDoneCallback callback,
      std::map<base::FilePath, MigrationUploadError> errors);

  // Profile for which this instance was created.
  raw_ptr<Profile> profile_;

  // The implementation of the upload process, specific to the chosen cloud
  // storage destination. This is initialized dynamically based on the
  // `destination` argument passed to the `Run` method.
  std::unique_ptr<MigrationCloudUploader> uploader_ = nullptr;

  base::WeakPtrFactory<MigrationCoordinator> weak_ptr_factory_{this};
};

// Abstract class for the implementation of file uploads to a specific cloud
// storage destination. Derived classes provide the concrete logic for
// interacting with the respective cloud provider.
class MigrationCloudUploader {
 public:
  MigrationCloudUploader(Profile* profile,
                         std::vector<base::FilePath> files,
                         const std::string& destination_dir,
                         MigrationDoneCallback callback);
  MigrationCloudUploader(const MigrationCloudUploader&) = delete;
  MigrationCloudUploader& operator=(const MigrationCloudUploader&) = delete;
  virtual ~MigrationCloudUploader();

  // Starts the upload of files to the relevant cloud location. Invokes
  // `callback_` upon completion.
  virtual void Run() = 0;

  // Stops any ongoing file uploads.
  virtual void Stop(base::OnceClosure stopped_callback) = 0;

 protected:
  // Maps file to their upload errors, if any.
  std::map<base::FilePath, MigrationUploadError> errors_;

  // Profile for which this instance was created.
  const raw_ptr<Profile> profile_;
  // The paths of the files or directories to be uploaded.
  const std::vector<base::FilePath> files_;
  // The name of the destination directory.
  const std::string destination_dir_;
  // Callback to run after all uploads finish.
  MigrationDoneCallback callback_;
};

// Migration file uploader for uploads to Microsoft OneDrive.
class OneDriveMigrationUploader : public MigrationCloudUploader {
 public:
  OneDriveMigrationUploader(Profile* profile,
                            std::vector<base::FilePath> files,
                            const std::string& destination_dir,
                            MigrationDoneCallback callback);
  OneDriveMigrationUploader(const OneDriveMigrationUploader&) = delete;
  OneDriveMigrationUploader& operator=(const OneDriveMigrationUploader&) =
      delete;
  ~OneDriveMigrationUploader() override;

  // MigrationCloudUploader overrides:
  void Run() override;
  void Stop(base::OnceClosure stopped_callback) override;

  // Used in tests to block the MigrationDoneCallback.
  void SetEmulateSlowForTesting(bool value);

 private:
  // Called when one upload operation completes.
  void OnUploadDone(const base::FilePath& file_path,
                    storage::FileSystemURL url,
                    std::optional<MigrationUploadError> error);

  // Whether MigrationDoneCallback should be run. Can only be false in tests.
  bool ShouldFinish();

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

// TODO(b/349101997): Implementation.
// Migration file uploader for uploads to Google Drive.
class GoogleDriveMigrationUploader : public MigrationCloudUploader {
 public:
  GoogleDriveMigrationUploader(Profile* profile,
                               std::vector<base::FilePath> files,
                               const std::string& destination_dir,
                               MigrationDoneCallback callback);
  GoogleDriveMigrationUploader(const GoogleDriveMigrationUploader&) = delete;
  GoogleDriveMigrationUploader& operator=(const GoogleDriveMigrationUploader&) =
      delete;
  ~GoogleDriveMigrationUploader() override;

  // MigrationCloudUploader overrides:
  void Run() override;
  void Stop(base::OnceClosure stopped_callback) override;

 private:
  void OnUploadDone(const base::FilePath& file_path,
                    std::optional<MigrationUploadError> error);

  // Maps source urls of files being uploaded to corresponding
  // DriveSkyvaultUploader instances.
  std::map<base::FilePath, std::unique_ptr<DriveSkyvaultUploader>> uploaders_;

  base::WeakPtrFactory<GoogleDriveMigrationUploader> weak_ptr_factory_{this};
};

}  // namespace policy::local_user_files

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_MIGRATION_COORDINATOR_H_
