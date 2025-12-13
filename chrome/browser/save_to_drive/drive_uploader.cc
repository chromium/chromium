// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/save_to_drive/drive_uploader.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/save_to_drive/content_reader.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "chrome/grit/generated_resources.h"
#include "components/drive/drive_api_util.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/signin/public/identity_manager/access_token_fetcher.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/common/base_requests.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/google_api_keys.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/socket/socket.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace save_to_drive {
namespace {

using extensions::api::pdf_viewer_private::SaveToDriveErrorType;
using extensions::api::pdf_viewer_private::SaveToDriveProgress;
using extensions::api::pdf_viewer_private::SaveToDriveStatus;

constexpr char kDeveloperKey[] = "X-Developer-Key";

constexpr std::string_view kMetadataContentType =
    "Content-Type: application/json; charset=UTF-8";
constexpr std::string_view kParentFolderUrl =
    "https://www.googleapis.com/drive/v3beta/files";

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

constexpr std::string_view kErrorReasonQuotaExceeded = "quotaExceeded";
constexpr std::string_view kErrorStorageQuotaExceeded = "storageQuotaExceeded";

constexpr base::TimeDelta kUploadInProgressUpdateDispatchInterval =
    base::Milliseconds(500);

std::optional<DriveUploader::Item> ParseClientFolderResponse(
    std::unique_ptr<endpoint_fetcher::EndpointResponse> endpoint_response) {
  if (!endpoint_response || endpoint_response->response.empty() ||
      endpoint_response->http_status_code != net::HTTP_OK) {
    return std::nullopt;
  }
  std::optional<base::Value::Dict> dict = base::JSONReader::ReadDict(
      endpoint_response->response, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!dict) {
    return std::nullopt;
  }
  const std::string* file_id = dict->FindString("id");
  if (!file_id) {
    return std::nullopt;
  }
  const std::string* file_name = dict->FindString("name");
  if (!file_name) {
    return std::nullopt;
  }
  return DriveUploader::Item{*file_id, *file_name};
}

SaveToDriveProgress CreateSuccessProgress(
    const endpoint_fetcher::EndpointResponse& endpoint_response,
    size_t file_size,
    std::string_view parent_folder_name) {
  SaveToDriveProgress progress;
  // The upload is not considered successful until the response is parsed.
  progress.status = SaveToDriveStatus::kUploadFailed;
  progress.error_type = SaveToDriveErrorType::kUnknownError;

  if (endpoint_response.response.empty()) {
    return progress;
  }
  std::optional<base::Value::Dict> dict = base::JSONReader::ReadDict(
      endpoint_response.response, base::JSON_PARSE_CHROMIUM_EXTENSIONS);
  if (!dict) {
    return progress;
  }
  const std::string* file_id = dict->FindString("id");
  if (!file_id) {
    return progress;
  }
  const std::string* name = dict->FindString("name");
  if (!name) {
    return progress;
  }
  progress.status = SaveToDriveStatus::kUploadCompleted;
  progress.error_type = SaveToDriveErrorType::kNoError;
  progress.drive_item_id = *file_id;
  progress.file_size_bytes = file_size;
  progress.uploaded_bytes = file_size;
  progress.file_name = *name;
  progress.parent_folder_name = parent_folder_name;
  return progress;
}

// See https://developers.google.com/drive/handle-errors for error handling.
SaveToDriveErrorType GetErrorType(
    const endpoint_fetcher::EndpointResponse& endpoint_response) {
  if (endpoint_response.error_type &&
      *endpoint_response.error_type ==
          endpoint_fetcher::FetchErrorType::kNetError) {
    return SaveToDriveErrorType::kOffline;
  }
  if (endpoint_response.http_status_code == net::HTTP_UNAUTHORIZED) {
    return SaveToDriveErrorType::kOauthError;
  }
  const std::optional<std::string> reason =
      google_apis::MapJsonErrorToReason(endpoint_response.response);
  // Public documentation recommends checking for `storageQuotaExceeded` but for
  // some cases it returns `quotaExceeded'.
  if (reason && (*reason == kErrorReasonQuotaExceeded ||
                 *reason == kErrorStorageQuotaExceeded)) {
    return SaveToDriveErrorType::kQuotaExceeded;
  }
  return SaveToDriveErrorType::kUnknownError;
}

}  // namespace

DriveUploader::DriveUploader(DriveUploaderType drive_uploader_type,
                             std::string title,
                             AccountInfo account_info,
                             ProgressCallback progress_callback,
                             Profile* profile,
                             ContentReader* content_reader)
    : drive_uploader_type_(drive_uploader_type),
      title_(std::move(title)),
      account_info_(std::move(account_info)),
      progress_callback_(std::move(progress_callback)),
      identity_manager_(IdentityManagerFactory::GetForProfile(profile)),
      url_loader_factory_(profile->GetDefaultStoragePartition()
                              ->GetURLLoaderFactoryForBrowserProcess()),
      content_reader_(content_reader) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(content_reader_);
}

DriveUploader::~DriveUploader() = default;

void DriveUploader::Start() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!identity_manager_->HasAccountWithRefreshToken(
          account_info_.account_id)) {
    NotifyError(SaveToDriveErrorType::kOauthError);
    return;
  }
  scoped_identity_manager_observation_.Observe(identity_manager_);
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
    NotifyError(SaveToDriveErrorType::kOauthError);
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

  FetchParentFolder();
}

void DriveUploader::FetchParentFolder() {
  GURL url = GURL(kParentFolderUrl);
  url = net::AppendOrReplaceQueryParameter(url, "create_as_client_folder",
                                           "true");
  base::Value::Dict metadata;
  metadata.Set("name",
               l10n_util::GetStringUTF16(IDS_SAVE_TO_DRIVE_FOLDER_NAME));
  metadata.Set("mimeType", drive::util::kDriveFolderMimeType);
  std::optional<std::string> metadata_string = base::WriteJson(metadata);
  parent_endpoint_fetcher_ = CreateEndpointFetcher(
      url, endpoint_fetcher::HttpMethod::kPost, kMetadataContentType,
      *metadata_string, oauth_headers_, base::DoNothing());
  parent_endpoint_fetcher_->Fetch(base::BindOnce(
      &DriveUploader::OnFetchParentFolder, weak_ptr_factory_.GetWeakPtr()));
}

void DriveUploader::OnFetchParentFolder(
    std::unique_ptr<endpoint_fetcher::EndpointResponse> response) {
  parent_folder_ = ParseClientFolderResponse(std::move(response));
  SaveToDriveProgress progress;
  if (!parent_folder_) {
    NotifyError(SaveToDriveErrorType::kParentFolderSelectionFailed);
    return;
  }

  progress.status = SaveToDriveStatus::kFetchParentFolder;
  progress.error_type = SaveToDriveErrorType::kNoError;
  progress_callback_.Run(std::move(progress));

  UploadFile();
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

DriveUploaderType DriveUploader::get_drive_uploader_type() const {
  return drive_uploader_type_;
}

void DriveUploader::set_oauth_headers_for_testing(
    std::vector<std::string> oauth_headers) {
  oauth_headers_ = std::move(oauth_headers);
}

const std::vector<std::string>& DriveUploader::oauth_headers() const {
  return oauth_headers_;
}

void DriveUploader::NotifyUploadInProgress(size_t uploaded_bytes,
                                           size_t total_bytes) {
  const base::TimeTicks now = base::TimeTicks::Now();
  if (now - last_upload_in_progress_update_time_ <
      kUploadInProgressUpdateDispatchInterval) {
    return;
  }
  last_upload_in_progress_update_time_ = now;

  SaveToDriveProgress progress;
  progress.status = SaveToDriveStatus::kUploadInProgress;
  progress.error_type = SaveToDriveErrorType::kNoError;
  progress.file_size_bytes = total_bytes;
  progress.uploaded_bytes = uploaded_bytes;
  progress_callback_.Run(std::move(progress));
}

void DriveUploader::NotifyUploadSuccess(
    std::unique_ptr<endpoint_fetcher::EndpointResponse> response) {
  progress_callback_.Run(CreateSuccessProgress(
      *response, content_reader_->GetSize(), parent_folder_->name));
}

void DriveUploader::NotifyUploadFailure(
    std::unique_ptr<endpoint_fetcher::EndpointResponse> response) {
  NotifyError(GetErrorType(*response));
}

void DriveUploader::NotifyError(SaveToDriveErrorType error_type) {
  SaveToDriveProgress progress;
  progress.status = SaveToDriveStatus::kUploadFailed;
  progress.error_type = error_type;
  progress_callback_.Run(std::move(progress));
}

void DriveUploader::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  if (account_info_.account_id == account_id) {
    NotifyError(SaveToDriveErrorType::kOauthError);
  }
}

}  // namespace save_to_drive
