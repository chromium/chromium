// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FAKE_DRIVE_UPLOADER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FAKE_DRIVE_UPLOADER_H_

#include <string>

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/escape.h"
#include "chrome/browser/sync_file_system/drive_backend/fake_drive_service_helper.h"
#include "components/drive/drive_uploader.h"
#include "components/drive/service/fake_drive_service.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/test_util.h"

namespace sync_file_system {
namespace drive_backend {

class FakeDriveServiceWrapper : public drive::FakeDriveService {
 public:
  FakeDriveServiceWrapper();

  FakeDriveServiceWrapper(const FakeDriveServiceWrapper&) = delete;
  FakeDriveServiceWrapper& operator=(const FakeDriveServiceWrapper&) = delete;

  ~FakeDriveServiceWrapper() override;

  // DriveServiceInterface overrides.
  google_apis::CancelCallbackOnce AddNewDirectory(
      const std::string& parent_resource_id,
      const std::string& directory_name,
      const drive::AddNewDirectoryOptions& options,
      google_apis::FileResourceCallback callback) override;

  void set_make_directory_conflict(bool enable) {
    make_directory_conflict_ = enable;
  }

 private:
  bool make_directory_conflict_;
};

// A fake implementation of DriveUploaderInterface, which provides fake
// behaviors for file uploading.
class FakeDriveUploader : public drive::DriveUploaderInterface {
 public:
  explicit FakeDriveUploader(FakeDriveServiceWrapper* fake_drive_service);

  FakeDriveUploader(const FakeDriveUploader&) = delete;
  FakeDriveUploader& operator=(const FakeDriveUploader&) = delete;

  ~FakeDriveUploader() override;

  // DriveUploaderInterface overrides.
  void StartBatchProcessing() override;
  void StopBatchProcessing() override;
  google_apis::CancelCallbackOnce UploadNewFile(
      const std::string& parent_resource_id,
      const base::FilePath& local_file_path,
      const std::string& title,
      const std::string& content_type,
      const drive::UploadNewFileOptions& options,
      drive::UploadCompletionCallback callback,
      google_apis::ProgressCallback progress_callback) override;
  google_apis::CancelCallbackOnce UploadExistingFile(
      const std::string& resource_id,
      const base::FilePath& local_file_path,
      const std::string& content_type,
      const drive::UploadExistingFileOptions& options,
      drive::UploadCompletionCallback callback,
      google_apis::ProgressCallback progress_callback) override;
  google_apis::CancelCallbackOnce ResumeUploadFile(
      const GURL& upload_location,
      const base::FilePath& local_file_path,
      const std::string& content_type,
      drive::UploadCompletionCallback callback,
      google_apis::ProgressCallback progress_callback) override;

  void set_make_file_conflict(bool enable) { make_file_conflict_ = enable; }

 private:
  raw_ptr<FakeDriveServiceWrapper> fake_drive_service_;
  bool make_file_conflict_;
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FAKE_DRIVE_UPLOADER_H_
