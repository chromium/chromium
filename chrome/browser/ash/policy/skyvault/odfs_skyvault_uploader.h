// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_ODFS_SKYVAULT_UPLOADER_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_ODFS_SKYVAULT_UPLOADER_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/policy/skyvault/policy_utils.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"

namespace ash::cloud_upload {

using policy::local_user_files::MigrationUploadError;

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
  // Type of the file to be uploaded to OneDrive whether it's a downloaded file
  // or a screencapture file, ...etc.
  enum class FileType {
    kDownload = 0,
    kScreenCapture = 1,
    kMigration = 2,
    kMaxValue = kMigration,
  };

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
      FileType file_type,
      base::RepeatingCallback<void(int64_t)> progress_callback,
      base::OnceCallback<void(bool, storage::FileSystemURL)> upload_callback);

  // Uploads the file at `file_system_url` to OneDrive, placing it at the
  // specified `target_path`.
  //
  // Upon completion, invokes `upload_callback` with the following:
  // * `MigrationUploadError error` - Indicates the type of error encountered
  // during the upload, if any.
  //                                  See the `MigrationUploadError` enum for
  //                                  possible values.
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
      FileType file_type,
      base::RepeatingCallback<void(int64_t)> progress_callback,
      UploadDoneCallback upload_callback,
      const base::FilePath& target_path);

  OdfsSkyvaultUploader(const OdfsSkyvaultUploader&) = delete;
  OdfsSkyvaultUploader& operator=(const OdfsSkyvaultUploader&) = delete;

  // Returns a weak pointer to this instance.
  base::WeakPtr<OdfsSkyvaultUploader> GetWeakPtr();

 private:
  friend base::RefCounted<OdfsSkyvaultUploader>;

  OdfsSkyvaultUploader(Profile* profile,
                       int64_t id,
                       const storage::FileSystemURL& file_system_url,
                       FileType file_type,
                       base::RepeatingCallback<void(int64_t)> progress_callback,
                       std::optional<base::FilePath> target_path);
  ~OdfsSkyvaultUploader() override;

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

  raw_ptr<Profile> profile_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  raw_ptr<::file_manager::io_task::IOTaskController> io_task_controller_;

  // The Id of the OdfsSkyvaultUploader instance. Used for notifications.
  const int64_t id_;

  // The Id of the move IOTask.
  std::optional<::file_manager::io_task::IOTaskId> observed_task_id_ =
      std::nullopt;

  // The url of the file to be uploaded.
  storage::FileSystemURL file_system_url_;

  // Path to upload the file to. If empty, file is uploaded to the root folder.
  std::optional<base::FilePath> target_path_;

  // The type of the file to be uploaded.
  FileType file_type_;

  // Progress callback repeatedly run with progress updates.
  base::RepeatingCallback<void(int64_t)> progress_callback_;

  // Upload callback run once with upload success/failure and the file url (if
  // successfully uploaded).
  UploadDoneCallback upload_callback_;

  base::WeakPtrFactory<OdfsSkyvaultUploader> weak_ptr_factory_{this};
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_ODFS_SKYVAULT_UPLOADER_H_
