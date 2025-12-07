// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAVE_TO_DRIVE_DRIVE_UPLOADER_H_
#define CHROME_BROWSER_SAVE_TO_DRIVE_DRIVE_UPLOADER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"

class GoogleServiceAuthError;
class GURL;
class Profile;

namespace endpoint_fetcher {
enum class HttpMethod;
class EndpointFetcher;
struct EndpointResponse;
using UploadProgressCallback =
    base::RepeatingCallback<void(uint64_t, uint64_t)>;
}  // namespace endpoint_fetcher

namespace extensions::api::pdf_viewer_private {
enum class SaveToDriveErrorType;
struct SaveToDriveProgress;
}  // namespace extensions::api::pdf_viewer_private

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace signin {
class AccessTokenFetcher;
struct AccessTokenInfo;
}  // namespace signin

namespace save_to_drive {

class ContentReader;

// The type of the Drive uploader.
enum class DriveUploaderType {
  kUnknown,
  kResumable,
  kMultipart,
};

// Base class for all Drive uploader implementations. It is responsible for
// fetching the access token for the user's account, uploading the file to
// Drive, and notifying the caller about the upload progress. Destroying the
// DriveUploader will cancel the upload if it is in progress. This class should
// only be used on the UI thread.
class DriveUploader : public signin::IdentityManager::Observer {
 public:
  // Callback to be invoked periodically when there is progress in the Save to
  // Drive upload process.
  using ProgressCallback = base::RepeatingCallback<void(
      extensions::api::pdf_viewer_private::SaveToDriveProgress)>;

  DriveUploader(DriveUploaderType drive_uploader_type,
                std::string title,
                AccountInfo account_info,
                ProgressCallback progress_callback,
                Profile* profile,
                ContentReader* content_reader);
  DriveUploader(const DriveUploader&) = delete;
  DriveUploader& operator=(const DriveUploader&) = delete;
  ~DriveUploader() override;

  // Starts the upload process. This function should be called only once.
  void Start();

  DriveUploaderType get_drive_uploader_type() const;

  void set_oauth_headers_for_testing(std::vector<std::string> oauth_headers);

  // Metadata of a Drive item. This is used to parse the response from the
  // Drive API.
  struct Item {
    std::string id;
    std::string name;
  };

 protected:
  // Implemented by subclasses to upload the file to Drive using their specific
  // protocol. This method assumes that OAuth headers have been created and
  // `parent_folder_` is set. Retrying a failed upload by calling this method
  // again will result in a new file being created on Drive.
  virtual void UploadFile() = 0;

  // Convenience function to create an `EndpointFetcher` to make HTTP requests
  // to Drive.
  std::unique_ptr<endpoint_fetcher::EndpointFetcher> CreateEndpointFetcher(
      const GURL& fetch_url,
      endpoint_fetcher::HttpMethod http_method,
      std::string_view content_type,
      std::string_view request_string,
      const std::vector<std::string>& request_headers,
      endpoint_fetcher::UploadProgressCallback upload_progress_callback);

  void OnFetchAccessToken(GoogleServiceAuthError error,
                          signin::AccessTokenInfo access_token_info);

  // Fetches a special folder using Drive API to use as the parent folder
  // for the uploaded file.
  void FetchParentFolder();

  // Handles the response from the Drive API when fetching the parent folder.
  void OnFetchParentFolder(
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);

  // Notifies through `progress_callback_` the latest upload progress. This
  // method will throttle the progress updates to avoid spamming the extension.
  // `uploaded_bytes` is the number of bytes that have been uploaded so far and
  // `total_bytes` is the total number of bytes that need to be uploaded.
  void NotifyUploadInProgress(size_t uploaded_bytes, size_t total_bytes);

  // Notifies through `progress_callback_` that the upload succeeded.
  // `response` is the response from the Drive API that contains the uploaded
  // file metadata.
  void NotifyUploadSuccess(
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);

  // Notifies through `progress_callback_` that the upload failed.
  // `response` is the response from the Drive API that contains the error
  // information.
  void NotifyUploadFailure(
      std::unique_ptr<endpoint_fetcher::EndpointResponse> response);

  // Notifies through `progress_callback_` that an error has occurred.
  void NotifyError(
      extensions::api::pdf_viewer_private::SaveToDriveErrorType error_type);

  // signin::IdentityManager::Observer:
  void OnRefreshTokenRemovedForAccount(
      const CoreAccountId& account_id) override;

  const std::vector<std::string>& oauth_headers() const;

  const DriveUploaderType drive_uploader_type_;
  const std::string title_;
  const AccountInfo account_info_;
  const ProgressCallback progress_callback_;
  const raw_ptr<signin::IdentityManager> identity_manager_;
  std::unique_ptr<signin::AccessTokenFetcher> access_token_fetcher_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<endpoint_fetcher::EndpointFetcher> parent_endpoint_fetcher_;
  std::optional<Item> parent_folder_;
  const raw_ptr<ContentReader> content_reader_;

 private:
  std::vector<std::string> oauth_headers_;
  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      scoped_identity_manager_observation_{this};
  // The last time an upload progress update was sent to the extension. This is
  // used to throttle the upload in progress updates.
  base::TimeTicks last_upload_in_progress_update_time_;

  base::WeakPtrFactory<DriveUploader> weak_ptr_factory_{this};
};

}  // namespace save_to_drive

#endif  // CHROME_BROWSER_SAVE_TO_DRIVE_DRIVE_UPLOADER_H_
