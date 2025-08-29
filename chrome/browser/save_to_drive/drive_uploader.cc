// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/drive_uploader.h"

#include <string>
#include <utility>

#include "components/signin/public/identity_manager/account_info.h"

namespace save_to_drive {

DriveUploader::DriveUploader(DriveUploaderType drive_uploader_type,
                             std::string title,
                             AccountInfo account_info,
                             ProgressCallback progress_callback)
    : drive_uploader_type_(drive_uploader_type),
      title_(std::move(title)),
      account_info_(std::move(account_info)),
      progress_callback_(std::move(progress_callback)) {}

DriveUploader::~DriveUploader() = default;

void DriveUploader::Start() {
  // TODO(crbug.com/435142523): Implement DriveUploader::Start.
  // 1. Fetch access token.
  // 2. Get the parent folder.
  // 3. Call UploadFile() to upload the file.
  // 4. Notify caller about the upload progress.
}

DriveUploaderType DriveUploader::get_drive_uploader_type_for_testing() const {
  return drive_uploader_type_;
}

}  // namespace save_to_drive
