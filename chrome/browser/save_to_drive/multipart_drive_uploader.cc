// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/multipart_drive_uploader.h"

namespace save_to_drive {

MultipartDriveUploader::MultipartDriveUploader(
    std::string title,
    AccountInfo account_info,
    ProgressCallback progress_callback,
    Profile* profile)
    : DriveUploader(DriveUploaderType::kMultipart,
                    std::move(title),
                    std::move(account_info),
                    std::move(progress_callback),
                    profile) {}

MultipartDriveUploader::~MultipartDriveUploader() = default;

void MultipartDriveUploader::UploadFile() {
  // TODO(crbug.com/424208776): Implement MultipartDriveUploader.
}

}  // namespace save_to_drive
