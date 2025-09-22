// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAVE_TO_DRIVE_RESUMABLE_DRIVE_UPLOADER_H_
#define CHROME_BROWSER_SAVE_TO_DRIVE_RESUMABLE_DRIVE_UPLOADER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/save_to_drive/drive_uploader.h"
#include "url/gurl.h"

namespace endpoint_fetcher {
class EndpointFetcher;
struct EndpointResponse;
}  // namespace endpoint_fetcher

namespace mojo_base {
class BigBuffer;
}  // namespace mojo_base

namespace extensions::api::pdf_viewer_private {
enum class SaveToDriveErrorType;
}  // namespace extensions::api::pdf_viewer_private

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

 private:
  void HandleInitiationResponse(
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);
  void OnContentRead(mojo_base::BigBuffer buffer);
  void HandleUploadResponse(
      size_t chunk_size,
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);
  void HandleHttpSuccessUploadResponse(
      size_t chunk_size,
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);
  void OnUploadProgress(uint64_t current_chunk_bytes,
                        uint64_t total_chunk_bytes);
  void NotifyUploadProgress(size_t uploaded_bytes);

  std::unique_ptr<endpoint_fetcher::EndpointFetcher>
      initiation_endpoint_fetcher_;
  std::unique_ptr<endpoint_fetcher::EndpointFetcher> upload_endpoint_fetcher_;
  GURL upload_url_;
  // The offset of the next chunk to be uploaded. This is the number of bytes
  // that have been uploaded so far.
  size_t offset_ = 0;

  base::WeakPtrFactory<ResumableDriveUploader> weak_ptr_factory_{this};
};

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_SAVE_TO_DRIVE_RESUMABLE_DRIVE_UPLOADER_H_
