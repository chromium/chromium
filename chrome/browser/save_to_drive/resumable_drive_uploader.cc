// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/resumable_drive_uploader.h"

namespace save_to_drive {

ResumableDriveUploader::ResumableDriveUploader(
    std::string title,
    AccountInfo account_info,
    ProgressCallback progress_callback,
    Profile* profile,
    ContentReader* content_reader)
    : DriveUploader(DriveUploaderType::kResumable,
                    std::move(title),
                    std::move(account_info),
                    std::move(progress_callback),
                    profile,
                    content_reader) {}

ResumableDriveUploader::~ResumableDriveUploader() = default;

void ResumableDriveUploader::UploadFile() {
  // TODO(crbug.com/424208776): Implement ResumableDriveUploader.
}

}  // namespace save_to_drive
