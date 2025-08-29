// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAVE_TO_DRIVE_MULTIPART_DRIVE_UPLOADER_H_
#define CHROME_BROWSER_SAVE_TO_DRIVE_MULTIPART_DRIVE_UPLOADER_H_

#include <string>

#include "chrome/browser/save_to_drive/drive_uploader.h"

namespace save_to_drive {

// A DriveUploader implementation that uses the Drive API's multipart upload
// protocol to upload the file to Drive.
class MultipartDriveUploader : public DriveUploader {
 public:
  MultipartDriveUploader(std::string title,
                         AccountInfo account_info,
                         ProgressCallback progress_callback,
                         Profile* profile);
  MultipartDriveUploader(const MultipartDriveUploader&) = delete;
  MultipartDriveUploader& operator=(const MultipartDriveUploader&) = delete;
  ~MultipartDriveUploader() override;

  // DriveUploader:
  void UploadFile() override;
};

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_SAVE_TO_DRIVE_MULTIPART_DRIVE_UPLOADER_H_
