// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/drive_uploader_wrapper.h"

#include "base/memory/weak_ptr.h"
#include "components/drive/drive_uploader.h"

namespace sync_file_system {
namespace drive_backend {

DriveUploaderWrapper::DriveUploaderWrapper(
    drive::DriveUploaderInterface* drive_uploader)
    : drive_uploader_(drive_uploader) {}

DriveUploaderWrapper::~DriveUploaderWrapper() = default;

void DriveUploaderWrapper::UploadExistingFile(
    const std::string& resource_id,
    const base::FilePath& local_file_path,
    const std::string& content_type,
    const drive::UploadExistingFileOptions& options,
    drive::UploadCompletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  drive_uploader_->UploadExistingFile(
      resource_id, local_file_path, content_type, options, std::move(callback),
      google_apis::ProgressCallback());
}

void DriveUploaderWrapper::UploadNewFile(
    const std::string& parent_resource_id,
    const base::FilePath& local_file_path,
    const std::string& title,
    const std::string& content_type,
    const drive::UploadNewFileOptions& options,
    drive::UploadCompletionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  drive_uploader_->UploadNewFile(parent_resource_id, local_file_path, title,
                                 content_type, options, std::move(callback),
                                 google_apis::ProgressCallback());
}

}  // namespace drive_backend
}  // namespace sync_file_system
