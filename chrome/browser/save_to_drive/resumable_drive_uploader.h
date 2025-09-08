// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAVE_TO_DRIVE_RESUMABLE_DRIVE_UPLOADER_H_
#define CHROME_BROWSER_SAVE_TO_DRIVE_RESUMABLE_DRIVE_UPLOADER_H_

#include <string>

#include "chrome/browser/save_to_drive/drive_uploader.h"

struct AccountInfo;
class Profile;

namespace save_to_drive {

class ContentReader;

// A DriveUploader implementation that uses the Drive API's resumable upload
// protocol to upload the file to Drive.
class ResumableDriveUploader : public DriveUploader {
 public:
  ResumableDriveUploader(std::string title,
                         AccountInfo account_info,
                         ProgressCallback progress_callback,
                         Profile* profile,
                         ContentReader* content_reader);
  ResumableDriveUploader(const ResumableDriveUploader&) = delete;
  ResumableDriveUploader& operator=(const ResumableDriveUploader&) = delete;
  ~ResumableDriveUploader() override;

  // DriveUploader:
  void UploadFile() override;
};

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_SAVE_TO_DRIVE_RESUMABLE_DRIVE_UPLOADER_H_
