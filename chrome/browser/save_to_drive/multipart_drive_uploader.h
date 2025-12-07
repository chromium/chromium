// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAVE_TO_DRIVE_MULTIPART_DRIVE_UPLOADER_H_
#define CHROME_BROWSER_SAVE_TO_DRIVE_MULTIPART_DRIVE_UPLOADER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/save_to_drive/drive_uploader.h"
#include "url/gurl.h"

class GURL;
class Profile;
struct AccountInfo;

namespace endpoint_fetcher {
class EndpointFetcher;
struct EndpointResponse;
}  // namespace endpoint_fetcher

namespace mojo_base {
class BigBuffer;
}  // namespace mojo_base

namespace save_to_drive {

class ContentReader;

// A DriveUploader implementation that uses the Drive API's multipart upload
// protocol to upload the file to Drive.
class MultipartDriveUploader : public DriveUploader {
 public:
  MultipartDriveUploader(std::string title,
                         AccountInfo account_info,
                         ProgressCallback progress_callback,
                         Profile* profile,
                         ContentReader* content_reader);
  MultipartDriveUploader(const MultipartDriveUploader&) = delete;
  MultipartDriveUploader& operator=(const MultipartDriveUploader&) = delete;
  ~MultipartDriveUploader() override;

  // DriveUploader:
  void UploadFile() override;

  void OnUploadProgress(uint64_t current_bytes, uint64_t total_bytes);
  void HandleUploadResponse(
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);
  void OnContentRead(mojo_base::BigBuffer buffer);

 private:
  std::unique_ptr<endpoint_fetcher::EndpointFetcher> upload_endpoint_fetcher_;

  base::WeakPtrFactory<MultipartDriveUploader> weak_ptr_factory_{this};
};

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_SAVE_TO_DRIVE_MULTIPART_DRIVE_UPLOADER_H_
