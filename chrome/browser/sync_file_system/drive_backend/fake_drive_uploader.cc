// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/fake_drive_uploader.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/drive_common_callbacks.h"
#include "testing/gtest/include/gtest/gtest.h"

using drive::FakeDriveService;
using drive::UploadCompletionCallback;
using google_apis::ApiErrorCode;
using google_apis::CancelCallbackOnce;
using google_apis::FileResource;
using google_apis::FileResourceCallback;
using google_apis::ProgressCallback;

namespace sync_file_system {
namespace drive_backend {

namespace {

void DidAddFileOrDirectoryForMakingConflict(
    ApiErrorCode error,
    std::unique_ptr<FileResource> entry) {
  ASSERT_EQ(google_apis::HTTP_CREATED, error);
  ASSERT_TRUE(entry);
}

void DidAddFileForUploadNew(UploadCompletionCallback callback,
                            ApiErrorCode error,
                            std::unique_ptr<FileResource> entry) {
  ASSERT_EQ(google_apis::HTTP_CREATED, error);
  ASSERT_TRUE(entry);
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), google_apis::HTTP_SUCCESS,
                                GURL(), std::move(entry)));
}

void DidGetFileResourceForUploadExisting(UploadCompletionCallback callback,
                                         ApiErrorCode error,
                                         std::unique_ptr<FileResource> entry) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), error, GURL(), std::move(entry)));
}

}  // namespace

FakeDriveServiceWrapper::FakeDriveServiceWrapper()
    : make_directory_conflict_(false) {}

FakeDriveServiceWrapper::~FakeDriveServiceWrapper() {}

CancelCallbackOnce FakeDriveServiceWrapper::AddNewDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_name,
    const drive::AddNewDirectoryOptions& options,
    FileResourceCallback callback) {
  if (make_directory_conflict_) {
    FakeDriveService::AddNewDirectory(
        parent_resource_id, directory_name, options,
        base::BindOnce(&DidAddFileOrDirectoryForMakingConflict));
  }
  return FakeDriveService::AddNewDirectory(parent_resource_id, directory_name,
                                           options, std::move(callback));
}

FakeDriveUploader::FakeDriveUploader(
    FakeDriveServiceWrapper* fake_drive_service)
    : fake_drive_service_(fake_drive_service), make_file_conflict_(false) {}

FakeDriveUploader::~FakeDriveUploader() {}

void FakeDriveUploader::StartBatchProcessing() {}

void FakeDriveUploader::StopBatchProcessing() {}

CancelCallbackOnce FakeDriveUploader::UploadNewFile(
    const std::string& parent_resource_id,
    const base::FilePath& local_file_path,
    const std::string& title,
    const std::string& content_type,
    const drive::UploadNewFileOptions& options,
    UploadCompletionCallback callback,
    ProgressCallback progress_callback) {
  DCHECK(!callback.is_null());
  const std::string kFileContent = "test content";

  if (make_file_conflict_) {
    fake_drive_service_->AddNewFile(
        content_type, kFileContent, parent_resource_id, title,
        false,  // shared_with_me
        base::BindOnce(&DidAddFileOrDirectoryForMakingConflict));
  }

  fake_drive_service_->AddNewFile(
      content_type, kFileContent, parent_resource_id, title,
      false,  // shared_with_me
      base::BindOnce(&DidAddFileForUploadNew, std::move(callback)));
  base::RunLoop().RunUntilIdle();

  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveUploader::UploadExistingFile(
    const std::string& resource_id,
    const base::FilePath& local_file_path,
    const std::string& content_type,
    const drive::UploadExistingFileOptions& options,
    UploadCompletionCallback callback,
    ProgressCallback progress_callback) {
  DCHECK(!callback.is_null());
  fake_drive_service_->GetFileResource(
      resource_id, base::BindOnce(&DidGetFileResourceForUploadExisting,
                                  std::move(callback)));
  return CancelCallbackOnce();
}

CancelCallbackOnce FakeDriveUploader::ResumeUploadFile(
    const GURL& upload_location,
    const base::FilePath& local_file_path,
    const std::string& content_type,
    drive::UploadCompletionCallback callback,
    ProgressCallback progress_callback) {
  // At the moment, sync file system doesn't support resuming of the uploading.
  // So this method shouldn't be reached.
  NOTREACHED_IN_MIGRATION();
  return CancelCallbackOnce();
}

}  // namespace drive_backend
}  // namespace sync_file_system
