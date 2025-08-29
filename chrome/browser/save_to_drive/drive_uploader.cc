// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/drive_uploader.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/google_api_keys.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/socket/socket.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace save_to_drive {
namespace {

using extensions::api::pdf_viewer_private::SaveToDriveErrorType;
using extensions::api::pdf_viewer_private::SaveToDriveProgress;
using extensions::api::pdf_viewer_private::SaveToDriveStatus;

constexpr char kDeveloperKey[] = "X-Developer-Key";

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotationTag =
    net::DefineNetworkTrafficAnnotation("save_to_drive", R"(
  semantics {
    sender: "Save to Drive"
    description: "Saves a file to Google Drive."
    trigger: "User clicks on Save to Drive button in the PDF viewer."
    data:
      "Content of the file to be uploaded."
      "Metadata: File name, file size, file type, etc."
      "ACCESS_TOKEN: Help identify if the user calling has access to the group"
    destination: GOOGLE_OWNED_SERVICE
    internal {
      contacts{email : "save-to-drive-eng-all@google.com"}
    }
    user_data {
      type: USER_CONTENT
      type: WEB_CONTENT
      type: OTHER
      type: ACCESS_TOKEN
    }
    last_reviewed: "2025-08-27"
  }
  policy {
    cookies_allowed: NO
    setting: "This feature cannot be disabled by settings."
    chrome_policy {
      BrowserSignin {
        BrowserSignin: 0
      }
      AlwaysOpenPdfExternally {
        AlwaysOpenPdfExternally: true
      }
    }
  })");

constexpr base::TimeDelta kDefaultTimeout = base::Seconds(30);

}  // namespace

DriveUploader::DriveUploader(DriveUploaderType drive_uploader_type,
                             std::string title,
                             AccountInfo account_info,
                             ProgressCallback progress_callback,
                             Profile* profile)
    : drive_uploader_type_(drive_uploader_type),
      title_(std::move(title)),
      account_info_(std::move(account_info)),
      progress_callback_(std::move(progress_callback)),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile)),
      url_loader_factory_(profile->GetDefaultStoragePartition()
                              ->GetURLLoaderFactoryForBrowserProcess()) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

DriveUploader::~DriveUploader() = default;

void DriveUploader::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!identity_manager_->HasAccountWithRefreshToken(
          account_info_.account_id)) {
    SaveToDriveProgress progress;
    progress.status = SaveToDriveStatus::kUploadFailed;
    progress.error_type = SaveToDriveErrorType::kOauthError;
    progress_callback_.Run(std::move(progress));
    return;
  }
  access_token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      account_info_.account_id, signin::OAuthConsumerId::kSaveToDrive,
      base::BindOnce(&DriveUploader::OnFetchAccessToken,
                     weak_ptr_factory_.GetWeakPtr()),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void DriveUploader::OnFetchAccessToken(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    SaveToDriveProgress progress;
    progress.status = SaveToDriveStatus::kUploadFailed;
    progress.error_type = SaveToDriveErrorType::kOauthError;
    progress_callback_.Run(std::move(progress));
    return;
  }

  oauth_headers_ = {kDeveloperKey,
                    GaiaUrls::GetInstance()->oauth2_chrome_client_id(),
                    net::HttpRequestHeaders::kAuthorization,
                    base::StrCat({"Bearer ", access_token_info.token})};

  SaveToDriveProgress progress;
  progress.status = SaveToDriveStatus::kFetchOauth;
  progress.error_type = SaveToDriveErrorType::kNoError;
  progress_callback_.Run(std::move(progress));

  // TODO(crbug.com/435142523): Implement the rest of the DriveUploader
  // 1. Get the parent folder.
  // 2. Call UploadFile() to upload the file.
  // 3. Notify caller about the upload progress.
}

std::unique_ptr<endpoint_fetcher::EndpointFetcher>
DriveUploader::CreateEndpointFetcher(
    const GURL& fetch_url,
    endpoint_fetcher::HttpMethod http_method,
    std::string_view content_type,
    std::string_view request_string,
    const std::vector<std::string>& request_headers,
    endpoint_fetcher::UploadProgressCallback upload_progress_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto request_params =
      endpoint_fetcher::EndpointFetcher::RequestParams::Builder(
          http_method, kTrafficAnnotationTag)
          .SetCredentialsMode(endpoint_fetcher::CredentialsMode::kOmit)
          .SetSetSiteForCookies(false)
          .SetAuthType(endpoint_fetcher::AuthType::NO_AUTH)
          .SetPostData(std::string(request_string))
          .SetContentType(std::string(content_type))
          .SetHeaders(request_headers)
          .SetTimeout(kDefaultTimeout)
          .SetUrl(fetch_url)
          .SetUploadProgressCallback(std::move(upload_progress_callback))
          .Build();
  return std::make_unique<endpoint_fetcher::EndpointFetcher>(
      /*url_loader_factory=*/url_loader_factory_.get(),
      /*identity_manager=*/identity_manager_,
      /*request_params=*/std::move(request_params));
}

DriveUploaderType DriveUploader::get_drive_uploader_type_for_testing() const {
  return drive_uploader_type_;
}

}  // namespace save_to_drive
