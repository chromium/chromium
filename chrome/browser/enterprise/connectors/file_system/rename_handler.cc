// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/rename_handler.h"

#include <utility>

#include "base/files/file_util.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/file_system/access_token_fetcher.h"
#include "chrome/browser/enterprise/connectors/file_system/signin_dialog_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "components/download/public/common/download_item.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/storage_partition.h"

namespace enterprise_connectors {

namespace {

PrefService* PrefsFromBrowserContext(content::BrowserContext* context) {
  return Profile::FromBrowserContext(context)->GetPrefs();
}

PrefService* PrefsFromDownloadItem(download::DownloadItem* item) {
  content::BrowserContext* context =
      content::DownloadItemUtils::GetBrowserContext(item);
  return context ? Profile::FromBrowserContext(context)->GetPrefs() : nullptr;
}

bool MimeTypeMatches(const std::set<std::string>& mime_types,
                     const std::string& mime_type) {
  return mime_types.count(kWildcardMimeType) != 0 ||
         mime_types.count(mime_type) != 0;
}

}  // namespace

const base::Feature kFileSystemConnectorEnabled{
    "FileSystemConnectorsEnabled", base::FEATURE_DISABLED_BY_DEFAULT};

// static
base::Optional<FileSystemSettings> FileSystemRenameHandler::IsEnabled(
    download::DownloadItem* download_item) {
  if (!base::FeatureList::IsEnabled(kFileSystemConnectorEnabled))
    return base::nullopt;

  // Check to see if the download item matches any rules.  If the URL of the
  // download itself does not match then check the URL of site on which the
  // download is hosted.
  ConnectorsService* service = ConnectorsServiceFactory::GetForBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(download_item));
  auto settings = service->GetFileSystemSettings(
      download_item->GetURL(), FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD);
  if (settings.has_value() &&
      MimeTypeMatches(settings->mime_types, download_item->GetMimeType())) {
    return settings;
  }

  settings = service->GetFileSystemSettings(
      download_item->GetTabUrl(), FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD);
  if (settings.has_value() &&
      MimeTypeMatches(settings->mime_types, download_item->GetMimeType())) {
    return settings;
  }

  return base::nullopt;
}

// static
std::unique_ptr<download::DownloadItemRenameHandler>
FileSystemRenameHandler::Create(download::DownloadItem* download_item,
                                FileSystemSettings settings) {
  return std::make_unique<FileSystemRenameHandler>(download_item,
                                                   std::move(settings));
}

// static
std::unique_ptr<download::DownloadItemRenameHandler>
FileSystemRenameHandler::CreateIfNeeded(download::DownloadItem* download_item) {
  auto settings = IsEnabled(download_item);
  if (!settings.has_value())
    return nullptr;

  return Create(download_item, std::move(settings.value()));
}

// The only permitted use of |download_item| in this class other than the ctor
// is passing it to content::DownloadItemUtils methods.  Methods in this class
// run on the UI thread where methods of |download_item| should not be called.
FileSystemRenameHandler::FileSystemRenameHandler(
    download::DownloadItem* download_item,
    FileSystemSettings settings)
    : download::DownloadItemRenameHandler(download_item),
      target_path_(download_item->GetTargetFilePath()),
      settings_(std::move(settings)),
      controller_(download_item) {}

FileSystemRenameHandler::~FileSystemRenameHandler() = default;

void FileSystemRenameHandler::Start(Callback callback) {
  download_callback_ = std::move(callback);
  controller_.Init(
      base::BindRepeating(&FileSystemRenameHandler::OnApiAuthenticationError,
                          weak_factory_.GetWeakPtr()),
      base::BindOnce(&FileSystemRenameHandler::NotifyResultToDownloadThread,
                     weak_factory_.GetWeakPtr()),
      GetPrefs());
  StartInternal();
}

void FileSystemRenameHandler::TryControllerTask(
    content::BrowserContext* context,
    const std::string& access_token) {
  controller_.TryTask(GetURLLoaderFactory(context), access_token);
}

void FileSystemRenameHandler::PromptUserSignInForAuthorization(
    content::WebContents* contents) {
  FileSystemSigninDialogDelegate::ShowDialog(
      contents, settings_,
      base::BindOnce(&FileSystemRenameHandler::OnAuthorization,
                     weak_factory_.GetWeakPtr()));
}

void FileSystemRenameHandler::FetchAccessToken(
    content::BrowserContext* context,
    const std::string& refresh_token) {
  token_fetcher_ = std::make_unique<AccessTokenFetcher>(
      GetURLLoaderFactory(context), settings_.service_provider,
      settings_.token_endpoint, refresh_token, std::string(),
      base::BindOnce(&FileSystemRenameHandler::OnAccessTokenFetched,
                     weak_factory_.GetWeakPtr()));
  token_fetcher_->Start(settings_.client_id, settings_.client_secret,
                        settings_.scopes);
}

FileSystemDownloadController*
FileSystemRenameHandler::GetControllerForTesting() {
  return &controller_;
}

void FileSystemRenameHandler::OpenDownload() {}

void FileSystemRenameHandler::ShowDownloadInContext() {}

void FileSystemRenameHandler::StartInternal() {
  PrefService* prefs;
  content::BrowserContext* context =
      content::DownloadItemUtils::GetBrowserContext(download_item());
  content::WebContents* contents =
      content::DownloadItemUtils::GetWebContents(download_item());

  // Check these pointers because they are pulled from a map keyed by
  // |download_item| from the UI thread. Since |download_item| lives in the
  // download thread and can remove the mapping at any time, the pointers are
  // not to be assumed valid on the UI thread.
  if (!context || !contents || !(prefs = PrefsFromBrowserContext(context))) {
    DLOG(ERROR) << "Empty pointers???";
    NotifyResultToDownloadThread(false);
    return;
  }

  std::string access_token;
  std::string refresh_token;
  bool ok = GetFileSystemOAuth2Tokens(prefs, settings_.service_provider,
                                      &access_token, &refresh_token);

  if (ok && !access_token.empty()) {  // Case 2.
    TryControllerTask(context, access_token);
  } else if (ok && !refresh_token.empty()) {  // Case 3.
    // Start AccessTokenFetcher to obtain access token with refresh token.
    FetchAccessToken(context, refresh_token);
  } else {  // Case 1.
    // Neither token is available, so prompt user to sign in & save new tokens.
    PromptUserSignInForAuthorization(contents);
  }
}

////////////////////////////////////////////////////////////////////////////////
// The OAuth2 "Dance":
//      AToken  || RToken || Action
// (1)  N       || N      || PromptUserSignInForAuthorization()
// (2)  Y       ||        || TryControllerTask()
// (3)  N       || Y      || FetchAccessToken
//
// (1) PromptUserSignInForAuthorization()
//    (a) Success: SaveTokens -> (2).
//    (b) Failure with GoogleServiceAuthError::State::REQUEST_CANCELED: [Abort].
//    (c) Other failures: Retry (1).
// (2) TryControllerTask()
//    (a) No authentication error: NotifyResultToDownloadThread() [Done].
//    (b) Authentication error: ClearAToken -> (3).
// (3) FetchAccessToken
//    (a) Success: SaveTokens->(2).
//    (b) Failure: ClearRToken->(1).
////////////////////////////////////////////////////////////////////////////////

scoped_refptr<network::SharedURLLoaderFactory>
FileSystemRenameHandler::GetURLLoaderFactory(content::BrowserContext* context) {
  content::StoragePartition* partition =
      content::BrowserContext::GetStoragePartitionForUrl(context,
                                                         settings_.home);
  return partition->GetURLLoaderFactoryForBrowserProcess();
}

// Case 1:
void FileSystemRenameHandler::OnAuthorization(
    const GoogleServiceAuthError& status,
    const std::string& access_token,
    const std::string& refresh_token) {
  if (status.state() == GoogleServiceAuthError::State::REQUEST_CANCELED) {
    // 1b:
    OnSignInCancellation();
  } else {
    // 1a or 1c:
    OnAccessTokenFetched(status, access_token, refresh_token);
  }
}

// Case 3 but overlaps with Case 1a and 1c
void FileSystemRenameHandler::OnAccessTokenFetched(
    const GoogleServiceAuthError& status,
    const std::string& access_token,
    const std::string& refresh_token) {
  PrefService* prefs = PrefsFromDownloadItem(download_item());
  if (status.state() != GoogleServiceAuthError::State::NONE) {
    // Case 1c and 3b:
    if (ClearFileSystemRefreshToken(prefs, settings_.service_provider)) {
      return OnAuthenticationError(status);
    }
  } else if (prefs &&
             SetFileSystemOAuth2Tokens(prefs, settings_.service_provider,
                                       access_token, refresh_token)) {
    // Case 1a and 3a:
    return StartInternal();
  }
  // Handle token storage operations failure.
  return NotifyResultToDownloadThread(false);
}

void FileSystemRenameHandler::OnAuthenticationError(
    const GoogleServiceAuthError& error) {
  PrefService* prefs = GetPrefs();
  if (prefs && ClearFileSystemAccessToken(prefs, settings_.service_provider)) {
    // Case 2b, but also Case 1c and 3b so that now both tokens are cleared.
    StartInternal();
  } else {
    NotifyResultToDownloadThread(false);
  }
}

void FileSystemRenameHandler::OnSignInCancellation() {
  DLOG(ERROR) << "Sign in canceled!";
  PrefService* prefs = GetPrefs();
  if (prefs) {
    ClearFileSystemOAuth2Tokens(prefs, settings_.service_provider);
  }
  NotifyResultToDownloadThread(false);
  // TODO(https://crbug.com/1184351): Handle local temporary file.
}

void FileSystemRenameHandler::OnApiAuthenticationError() {
  DLOG(ERROR) << "Authentication failed in service provider API calls.";
  return OnAuthenticationError(
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
}

void FileSystemRenameHandler::NotifyResultToDownloadThread(bool success) {
  // TODO(https://crbug.com/1168815): Define required error messages.
  auto reason = success ? download::DOWNLOAD_INTERRUPT_REASON_NONE
                        : download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
  // Make sure target_path_ has been initialized.
  DCHECK(!target_path_.empty());
  std::move(download_callback_).Run(reason, target_path_);
}

PrefService* FileSystemRenameHandler::GetPrefs() {
  return PrefsFromDownloadItem(download_item());
}

}  // namespace enterprise_connectors
