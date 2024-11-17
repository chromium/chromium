// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_ODFS_SKYVAULT_UPLOADER_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_ODFS_SKYVAULT_UPLOADER_H_

#include <optional>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"

namespace ash::cloud_upload {

using policy::local_user_files::MigrationUploadError;
using policy::local_user_files::UploadTrigger;

// Uploads the file to Microsoft OneDrive and calls the `upload_callback_` with
// the result of the file upload, which is when `OdfsSkyvaultUploader` goes out
// of scope. Instantiated by the static `Upload` method. Runs
// `progress_callback` by the upload progress if possible.
class OdfsSkyvaultUploader
    : public base::RefCounted<OdfsSkyvaultUploader>,
      ::file_manager::io_task::IOTaskController::Observer {
 public:
  using UploadDoneCallback =
      base::OnceCallback<void(storage::FileSystemURL,
                              std::optional<MigrationUploadError>,
                              base::FilePath upload_root_path)>;

  // Uploads the file at `path` to the OneDrive root directory.
  //
  // Upon completion, invokes `upload_callback` with the following:
  // * `bool success` - Indicates whether the upload was successful.
  // * `storage::FileSystemURL url` - (If successful) The URL of the uploaded
  // file on OneDrive.
  //
  // Optionally, periodically invokes the `progress_callback` during the upload
  // to provide progress updates in bytes transferred.
  //
  // Returns a weak pointer to the `OdfsSkyvaultUploader` object. This can be
  // used to cancel the upload before it completes.
  static base::WeakPtr<OdfsSkyvaultUploader> Upload(
      Profile* profile,
      const base::FilePath& path,
      UploadTrigger trigger,
      base::RepeatingCallback<void(int64_t)> progress_callback,
      base::OnceCallback<void(bool, storage::FileSystemURL)> upload_callback,
      std::optional<const gfx::Image> thumbnail = std::nullopt);

  // Uploads the file to OneDrive, placing it in the device-unique folder at the
  // specified relative path.
  //
  // Invokes `upload_callback` upon completion.
  //
  // Optionally, periodically invokes the `progress_callback` during the upload
  // to provide progress updates in bytes transferred.
  //
  // Returns a weak pointer to the `OdfsSkyvaultUploader` object. This can be
  // used to cancel the upload before it completes.
  //
  // Example: Uploading "example.txt" with a `relative_source_path` of
  // "Documents/Files" and `upload_root` "ChromeOS Device 123" results in
  // "<ODFS ROOT>/ChromeOS Device 123/Documents/Files/example.txt" on OneDrive.
  static base::WeakPtr<OdfsSkyvaultUploader> Upload(
      Profile* profile,
      const base::FilePath& path,
      const base::FilePath& relative_source_path,
      const std::string& upload_root,
      UploadTrigger trigger,
      base::RepeatingCallback<void(int64_t)> progress_callback,
      UploadDoneCallback upload_callback);

  OdfsSkyvaultUploader(const OdfsSkyvaultUploader&) = delete;
  OdfsSkyvaultUploader& operator=(const OdfsSkyvaultUploader&) = delete;

  // Returns a weak pointer to this instance.
  base::WeakPtr<OdfsSkyvaultUploader> GetWeakPtr();

  // Should cancel the whole upload, if possible.
  virtual void Cancel();

 protected:
  OdfsSkyvaultUploader(Profile* profile,
                       int64_t id,
                       const storage::FileSystemURL& file_system_url,
                       UploadTrigger trigger,
                       base::RepeatingCallback<void(int64_t)> progress_callback,
                       std::optional<const gfx::Image> thumbnail);
  ~OdfsSkyvaultUploader() override;

  // Returns the path to upload the file to.
  virtual base::FilePath GetDestinationFolderPath(
      file_system_provider::ProvidedFileSystemInterface* file_system);

  // Requests the sign in to OneDrive.
  virtual void RequestSignIn(
      base::OnceCallback<void(base::File::Error)> on_sign_in_cb);

  // Starts the upload flow.
  virtual void Run(UploadDoneCallback upload_callback);

  raw_ptr<Profile> profile_;

  // Absolute path to the device's upload root folder on Drive. This is
  // populated when the upload is about to start.
  base::FilePath upload_root_path_;

 private:
  friend base::RefCounted<OdfsSkyvaultUploader>;

  void OnEndUpload(storage::FileSystemURL url,
                   std::optional<MigrationUploadError> error = std::nullopt);

  void GetODFSMetadataAndStartIOTask();

  void CheckReauthenticationAndStartIOTask(
      base::expected<ODFSMetadata, base::File::Error> metadata_or_error);

  // IOTaskController::Observer:
  void OnIOTaskStatus(
      const ::file_manager::io_task::ProgressStatus& status) override;

  // Translates the status error into a MigrationUploadError.
  void ProcessError(const ::file_manager::io_task::ProgressStatus& status);

  // Called when the mount response is received.
  void OnMountResponse(base::File::Error result);

  // Starts the IOTask to upload the file to OneDrive.
  void StartIOTask();

  scoped_refptr<storage::FileSystemContext> file_system_context_;
  raw_ptr<::file_manager::io_task::IOTaskController> io_task_controller_;

  // The Id of the OdfsSkyvaultUploader instance. Used for notifications.
  const int64_t id_;

  // The Id of the move IOTask.
  std::optional<::file_manager::io_task::IOTaskId> observed_task_id_ =
      std::nullopt;

  // The url of the file to be uploaded.
  storage::FileSystemURL file_system_url_;

  // The event or action that initiated the file upload.
  const UploadTrigger trigger_;

  // Progress callback repeatedly run with progress updates.
  base::RepeatingCallback<void(int64_t)> progress_callback_;

  // Upload callback run once with upload success/failure and the file url (if
  // successfully uploaded).
  UploadDoneCallback upload_callback_;

  // Set to `true` if upload is explicitly cancelled by owner. Forces every step
  // to exit early.
  bool cancelled_ = false;

  // Optional preview of the file that is being uploaded.
  std::optional<const gfx::Image> thumbnail_;

  base::WeakPtrFactory<OdfsSkyvaultUploader> weak_ptr_factory_{this};
};

// Similar to  OdfsSkyvaultUploader, but specialized for the migration flow:
// - doesn't require the file to first be moved to tmp
// - doesn't require progress updates
// - uploads file to a dedicated folder on OneDrive, and not to root
// - invokes different sign-in process, that ensures only one notification is
// TODO(aidazolic): Fix the instantiation.
class OdfsMigrationUploader : public OdfsSkyvaultUploader {
 public:
  using FactoryCallback =
      base::RepeatingCallback<scoped_refptr<OdfsMigrationUploader>(
          Profile*,
          int64_t,
          const storage::FileSystemURL&,
          const base::FilePath&)>;
  static scoped_refptr<OdfsMigrationUploader> Create(
      Profile* profile,
      int64_t id,
      const storage::FileSystemURL& file_system_url,
      const base::FilePath& relative_source_path,
      const std::string& upload_root);

  // Sets a testing factory function, allowing the injection of mock
  // OdfsMigrationUploader objects into the migration upload process.
  static void SetFactoryForTesting(FactoryCallback factory);

 protected:
  OdfsMigrationUploader(Profile* profile,
                        int64_t id,
                        const storage::FileSystemURL& file_system_url,
                        const base::FilePath& relative_source_path,
                        const std::string& upload_root);
  ~OdfsMigrationUploader() override;

 private:
  // OdfsSkyvaultUploader:
  base::FilePath GetDestinationFolderPath(
      file_system_provider::ProvidedFileSystemInterface* file_system) override;
  void RequestSignIn(
      base::OnceCallback<void(base::File::Error)> on_sign_in_cb) override;

  // Part of the source path relative to MyFiles
  const base::FilePath relative_source_path_;
  // The name of the device-unique upload root folder on Drive
  const std::string upload_root_;

  base::CallbackListSubscription subscription_;
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_ODFS_SKYVAULT_UPLOADER_H_
