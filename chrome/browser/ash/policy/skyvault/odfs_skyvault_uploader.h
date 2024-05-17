// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_ODFS_SKYVAULT_UPLOADER_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_ODFS_SKYVAULT_UPLOADER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"

namespace ash::cloud_upload {

// Uploads the file to Microsoft OneDrive and calls the `upload_callback_` with
// the result of the file upload, which is when `OdfsSkyvaultUploader` goes out
// of scope. Instantiated by the static `Upload` method. Runs
// `progress_callback` by the upload progress if possible.
class OdfsSkyvaultUploader
    : public base::RefCounted<OdfsSkyvaultUploader>,
      ::file_manager::io_task::IOTaskController::Observer {
 public:
  // Type of the file to be uploaded to OneDrive whether it's a downloaded file
  // or a screencapture file, ...etc.
  enum class FileType {
    kDownload = 0,
    kScreenCapture = 1,
    kMaxValue = kScreenCapture,
  };

  // Starts uploading the file specified at `file_path`.
  static void Upload(Profile* profile,
                     const base::FilePath& file_path,
                     FileType file_type,
                     base::RepeatingCallback<void(int)> progress_callback,
                     base::OnceCallback<void(bool)> upload_callback);

  OdfsSkyvaultUploader(const OdfsSkyvaultUploader&) = delete;
  OdfsSkyvaultUploader& operator=(const OdfsSkyvaultUploader&) = delete;

 private:
  friend base::RefCounted<OdfsSkyvaultUploader>;

  OdfsSkyvaultUploader(Profile* profile,
                       const base::FilePath& file_path,
                       FileType file_type,
                       base::RepeatingCallback<void(int)> progress_callback);
  ~OdfsSkyvaultUploader() override;

  // Starts the upload workflow.
  void Run(base::OnceCallback<void(bool)> upload_callback);

  void OnEndUpload(bool success);

  void GetODFSMetadataAndStartIOTask();

  void CheckReauthenticationAndStartIOTask(
      const storage::FileSystemURL& destination_folder_url,
      base::expected<ODFSMetadata, base::File::Error> metadata_or_error);

  // IOTaskController::Observer:
  void OnIOTaskStatus(
      const file_manager::io_task::ProgressStatus& status) override;

  raw_ptr<Profile> profile_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  raw_ptr<::file_manager::io_task::IOTaskController> io_task_controller_;

  // The Id of the move IOTask.
  ::file_manager::io_task::IOTaskId observed_task_id_ = -1;

  // The observed file local path.
  base::FilePath local_file_path_;

  // The type of the file to be uploaded.
  FileType file_type_;

  // Progress callback repeatedly run with progress updates.
  base::RepeatingCallback<void(int)> progress_callback_;

  // Upload callback run once with upload success/failure.
  base::OnceCallback<void(bool)> upload_callback_;

  base::WeakPtrFactory<OdfsSkyvaultUploader> weak_ptr_factory_{this};
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_ODFS_SKYVAULT_UPLOADER_H_
