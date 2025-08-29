// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAVE_TO_DRIVE_DRIVE_UPLOADER_H_
#define CHROME_BROWSER_SAVE_TO_DRIVE_DRIVE_UPLOADER_H_

#include <string>

#include "base/functional/callback.h"
#include "components/signin/public/identity_manager/account_info.h"

namespace extensions::api::pdf_viewer_private {
struct SaveToDriveProgress;
}  // namespace extensions::api::pdf_viewer_private

namespace save_to_drive {

// The type of the Drive uploader.
enum class DriveUploaderType {
  kUnknown,
  kResumable,
  kMultipart,
};

// Base class for all Drive uploader implementations. It is responsible for
// fetching the access token for the user's account, uploading the file to
// Drive, and notifying the caller about the upload progress. Destroying the
// DriveUploader will cancel the upload if it is in progress.
class DriveUploader {
 public:
  // Callback to be invoked periodically when there is progress in the Save to
  // Drive upload process.
  using ProgressCallback = base::RepeatingCallback<void(
      extensions::api::pdf_viewer_private::SaveToDriveProgress)>;

  DriveUploader(DriveUploaderType drive_uploader_type,
                std::string title,
                AccountInfo account_info,
                ProgressCallback progress_callback);
  DriveUploader(const DriveUploader&) = delete;
  DriveUploader& operator=(const DriveUploader&) = delete;
  virtual ~DriveUploader();

  // Starts the upload process. This function should be called only once.
  void Start();

  // Subclasses should implement this function to upload the file to Drive
  // according to the protocol they are implementing.
  virtual void UploadFile() = 0;

  DriveUploaderType get_drive_uploader_type_for_testing() const;

 protected:
  const DriveUploaderType drive_uploader_type_;
  const std::string title_;
  const AccountInfo account_info_;
  const ProgressCallback progress_callback_;
};

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_SAVE_TO_DRIVE_DRIVE_UPLOADER_H_
