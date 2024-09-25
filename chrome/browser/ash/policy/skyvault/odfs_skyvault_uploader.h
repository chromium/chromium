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
                              std::optional<MigrationUploadError>)>;

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

  // Uploads the file at `file_system_url` to OneDrive, placing it at the
  // specified `target_path`.
  //
  // Upon completion, invokes `upload_callback` with the following:
  // * `MigrationUploadError error` - Indicates the type of error encountered
  // during the upload, if any. See the `MigrationUploadError` enum for possible
  // values.
  // * `storage::FileSystemURL url` - The URL of the uploaded file on OneDrive.
  // This will be empty if the upload failed.
  //
  // Optionally, periodically invokes the `progress_callback` during the upload
  // to provide progress updates in bytes transferred.
  //
  // Returns a weak pointer to the `OdfsSkyvaultUploader` object. This can be
  // used to cancel the upload before it completes.
  //
  // Example: Uploading "example.txt" with a `target_path` of "Documents/Files"
  // results in
  // "<ODFS ROOT>/Documents/Files/example.txt" on OneDrive.
  static base::WeakPtr<OdfsSkyvaultUploader> Upload(
      Profile* profile,
      const base::FilePath& path,
      UploadTrigger trigger,
      base::RepeatingCallback<void(int64_t)> progress_callback,
      UploadDoneCallback upload_callback,
      const base::FilePath& target_path);

  OdfsSkyvaultUploader(const OdfsSkyvaultUploader&) = delete;
  OdfsSkyvaultUploader& operator=(const OdfsSkyvaultUploader&) = delete;

  // Returns a weak pointer to this instance.
  base::WeakPtr<OdfsSkyvaultUploader> GetWeakPtr();

  // Should cancel the whole upload, if possible.
  void Cancel();

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

  raw_ptr<Profile> profile_;

 private:
  friend base::RefCounted<OdfsSkyvaultUploader>;

  // Starts the upload flow.
  void Run(UploadDoneCallback upload_callback);

  void OnEndUpload(storage::FileSystemURL url,
                   std::optional<MigrationUploadError> error = std::nullopt);

  void GetODFSMetadataAndStartIOTask();

  void CheckReauthenticationAndStartIOTask(
      base::expected<ODFSMetadata, base::File::Error> metadata_or_error);

  // IOTaskController::Observer:
  void OnIOTaskStatus(
      const ::file_manager::io_task::ProgressStatus& status) override;

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
  static scoped_refptr<OdfsMigrationUploader> Create(
      Profile* profile,
      int64_t id,
      const storage::FileSystemURL& file_system_url,
      const base::FilePath& target_path);

 private:
  OdfsMigrationUploader(Profile* profile,
                        int64_t id,
                        const storage::FileSystemURL& file_system_url,
                        const base::FilePath& target_path);
  ~OdfsMigrationUploader() override;

  // OdfsSkyvaultUploader:
  base::FilePath GetDestinationFolderPath(
      file_system_provider::ProvidedFileSystemInterface* file_system) override;
  void RequestSignIn(
      base::OnceCallback<void(base::File::Error)> on_sign_in_cb) override;

  // Path in OneDrive to upload the file to.
  base::FilePath target_path_;

  base::CallbackListSubscription subscription_;
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_ODFS_SKYVAULT_UPLOADER_H_
