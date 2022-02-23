// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/rename_handler.h"

#include <utility>

#include "base/files/file_util.h"
#include "base/observer_list.h"
#include "chrome/browser/enterprise/connectors/file_system/access_token_fetcher.h"
#include "chrome/browser/enterprise/connectors/file_system/account_info_utils.h"
#include "chrome/browser/enterprise/connectors/file_system/box_uploader.h"
#include "chrome/browser/enterprise/connectors/file_system/signin_dialog_delegate.h"
#include "chrome/browser/enterprise/connectors/file_system/signin_experience.h"
#include "chrome/browser/enterprise/connectors/file_system/uma_metrics_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/scoped_tabbed_browser_displayer.h"
#include "components/download/public/common/download_interrupt_reasons_utils.h"
#include "components/download/public/common/download_item.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "content/public/browser/storage_partition.h"

namespace enterprise_connectors {

namespace {

// The OAuth token consumer name.
const char kOAuthConsumerName[] = "file_system_rename_handler";

PrefService* PrefsFromBrowserContext(content::BrowserContext* context) {
  return Profile::FromBrowserContext(context)->GetPrefs();
}

PrefService* PrefsFromDownloadItem(download::DownloadItem* item) {
  content::BrowserContext* context =
      content::DownloadItemUtils::GetBrowserContext(item);
  return context ? PrefsFromBrowserContext(context) : nullptr;
}

using download::ConvertNetErrorToInterruptReason;

}  // namespace

// static
std::unique_ptr<download::DownloadItemRenameHandler>
FileSystemRenameHandler::CreateIfNeeded(download::DownloadItem* download_item) {
  if (download_item->GetState() == download::DownloadItem::COMPLETE) {
    auto rename_handler =
        download_item->GetRerouteInfo().IsInitialized()
            ? std::make_unique<FileSystemRenameHandler>(download_item)
            : nullptr;
    return rename_handler;
  }
  // TODO(https://crbug.com/1213761) Resume upload if state is IN_PROGRESS, and
  // perhaps also INTERRUPTED and CANCELLED.
  absl::optional<FileSystemSettings> settings =
      GetFileSystemSettings(download_item);
  auto rename_handler = settings.has_value()
                            ? std::make_unique<FileSystemRenameHandler>(
                                  download_item, std::move(settings.value()))
                            : nullptr;
  if (!rename_handler) {
    UmaLogDownloadsRoutingDestination(
        EnterpriseFileSystemDownloadsRoutingDestination::NOT_ROUTED);
  } else if (rename_handler->settings_.service_provider ==
             kFileSystemServiceProviderPrefNameBox) {
    UmaLogDownloadsRoutingDestination(
        EnterpriseFileSystemDownloadsRoutingDestination::ROUTED_TO_BOX);
  } else {
    NOTREACHED() << "Unsupported service provider: "
                 << rename_handler->settings_.service_provider;
  }
  return rename_handler;
}

// The only permitted use of |download_item| in this class other than the ctor
// is passing it to content::DownloadItemUtils methods.  Methods in this class
// run on the UI thread where methods of |download_item| should not be called.
FileSystemRenameHandler::FileSystemRenameHandler(
    download::DownloadItem* download_item,
    FileSystemSettings settings)
    : download::DownloadItemRenameHandler(download_item),
      settings_(std::move(settings)),
      uploader_(BoxUploader::Create(download_item)) {
  DCHECK_EQ(settings_.service_provider, kFileSystemServiceProviderPrefNameBox);
}

FileSystemRenameHandler::FileSystemRenameHandler(
    download::DownloadItem* download_item)
    : download::DownloadItemRenameHandler(download_item),
      uploader_(BoxUploader::Create(download_item)) {}

FileSystemRenameHandler::~FileSystemRenameHandler() {
  for (auto& observer : observers_)
    observer.OnDestruction();
}

void FileSystemRenameHandler::Start(ProgressUpdateCallback progress_update_cb,
                                    DownloadCallback upload_complete_cb) {
  uploader_->Init(
      base::BindRepeating(&FileSystemRenameHandler::OnApiAuthenticationError,
                          weak_factory_.GetWeakPtr()),
      std::move(progress_update_cb), std::move(upload_complete_cb), GetPrefs());
  for (auto& observer : observers_)
    observer.OnStart();
  StartInternal();
}

void FileSystemRenameHandler::TryUploaderTask(content::BrowserContext* context,
                                              const std::string& access_token) {
  uploader_->TryTask(GetURLLoaderFactory(context), access_token);
}

void FileSystemRenameHandler::PromptUserSignInForAuthorization(
    content::WebContents* contents) {
  StartFileSystemConnectorSigninExperienceForDownloadItem(
      contents, settings_, GetPrefs(),
      base::BindOnce(&FileSystemRenameHandler::OnAuthorization,
                     weak_factory_.GetWeakPtr()),
      signin_observer_);
}

void FileSystemRenameHandler::FetchAccessToken(
    content::BrowserContext* context,
    const std::string& refresh_token) {
  token_fetcher_ = std::make_unique<AccessTokenFetcher>(
      GetURLLoaderFactory(context), settings_.service_provider,
      settings_.token_endpoint, refresh_token, /*auth_code=*/std::string(),
      kOAuthConsumerName,
      base::BindOnce(&FileSystemRenameHandler::OnAccessTokenFetched,
                     weak_factory_.GetWeakPtr()));
  for (auto& observer : observers_)
    observer.OnFetchAccessTokenStart();
  token_fetcher_->Start(settings_.client_id, settings_.client_secret,
                        settings_.scopes);
}

void FileSystemRenameHandler::SetUploaderForTesting(
    std::unique_ptr<BoxUploader> fake_uploader) {
  CHECK(fake_uploader);
  uploader_ = std::move(fake_uploader);
}

void FileSystemRenameHandler::OpenDownload() {
  AddTabToShowDownload(uploader_->GetUploadedFileUrl());
}

void FileSystemRenameHandler::ShowDownloadInContext() {
  AddTabToShowDownload(uploader_->GetDestinationFolderUrl());
}

void FileSystemRenameHandler::AddTabToShowDownload(const GURL& url) {
  if (url.is_valid()) {
    content::BrowserContext* context =
        content::DownloadItemUtils::GetBrowserContext(download_item());
    Profile* profile = Profile::FromBrowserContext(context);
    chrome::ScopedTabbedBrowserDisplayer displayer(profile);
    Browser* browser = displayer.browser();
    chrome::AddTabAt(browser, url, /* index = */ -1, /* foreground = */ true);
  }
  // The uploaded file or folder url's may not be valid before file upload completes, and we should
  // avoid just opening a new empty tab. Therefore, a new tab is only opened when the url is valid.
}

void FileSystemRenameHandler::StartInternal(std::string access_token) {
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
    uploader_->TerminateTask(kBrowserFailure);
    return;
  }

  std::string refresh_token;
  bool ok = access_token.size() ||
            GetFileSystemOAuth2Tokens(prefs, settings_.service_provider,
                                      &access_token, &refresh_token);

  if (ok && access_token.size()) {  // Case 2.
    TryUploaderTask(context, access_token);
  } else if (ok && refresh_token.size()) {  // Case 3.
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
// (2)  Y       ||        || TryUploaderTask()
// (3)  N       || Y      || FetchAccessToken()
//
// (1) PromptUserSignInForAuthorization()
//    (a) Success: SaveTokens -> (2).
//    (b) Failure with GoogleServiceAuthError::State::REQUEST_CANCELED: [Abort].
//    (c) Other failures (no network error): ClearRToken->(1).
//    (d) Network Error: [Abort].
// (2) TryUploaderTask()
//    (a) No authentication error: result sent back to download thread [Done].
//    (b) Authentication error: ClearAToken -> (3).
// (3) FetchAccessToken()
//    (a) Success: SaveTokens->(2).
//    (b) None-Network Failures: ClearRToken->(1).
//    (c) Network Error: [Abort].
////////////////////////////////////////////////////////////////////////////////

scoped_refptr<network::SharedURLLoaderFactory>
FileSystemRenameHandler::GetURLLoaderFactory(content::BrowserContext* context) {
  content::StoragePartition* partition = context->GetDefaultStoragePartition();
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
  for (auto& observer : observers_)
    observer.OnAccessTokenFetched(status);

  // Case 1d or 3c:
  const net::Error net_error = static_cast<net::Error>(status.network_error());
  if (net_error) {
    // Don't clear the OAuth2 tokens if it's only a network error.
    DCHECK_EQ(status.state(), GoogleServiceAuthError::State::CONNECTION_FAILED);
    return uploader_->TerminateTask(ConvertNetErrorToInterruptReason(
        net_error, download::DOWNLOAD_INTERRUPT_FROM_NETWORK));
  }

  // Case 1a and 3a:
  if (status.state() == GoogleServiceAuthError::State::NONE) {
    const bool save_success = SetFileSystemOAuth2Tokens(
        GetPrefs(), settings_.service_provider, access_token, refresh_token);
    LOG_IF(ERROR, !save_success) << "Failed to save OAuth2 tokens.";
    // Can proceed with current task using this token even if failed to save.
    return StartInternal(access_token);
  }

  // Case 1c and 3b:
  if (ClearFileSystemOAuth2Tokens(GetPrefs(), settings_.service_provider)) {
    return OnAuthenticationError(status);
  }

  // Handle token storage operations failure.
  return uploader_->TerminateTask(kCredentialUpdateFailure);
}

void FileSystemRenameHandler::OnAuthenticationError(
    const GoogleServiceAuthError& error) {
  if (ClearFileSystemAccessToken(GetPrefs(), settings_.service_provider)) {
    // Case 2b, but also Case 1c and 3b so that now both tokens are cleared.
    VLOG(20) << "Re-authenticating...";
    StartInternal();
  } else {
    LOG(ERROR) << "Failed to clear access token. Will notify failure back.";
    uploader_->TerminateTask(kCredentialUpdateFailure);
  }
}

void FileSystemRenameHandler::OnSignInCancellation() {
  DLOG(ERROR) << "Sign in canceled!";
  const bool clear_success =
      ClearFileSystemOAuth2Tokens(GetPrefs(), settings_.service_provider);
  LOG_IF(ERROR, !clear_success) << "Failed to clear OAuth2 tokens.";
  uploader_->TerminateTask(kSignInCancellation);
}

void FileSystemRenameHandler::OnApiAuthenticationError() {
  VLOG(20) << "Authentication failed in service provider API calls.";
  return OnAuthenticationError(
      GoogleServiceAuthError::FromInvalidGaiaCredentialsReason(
          GoogleServiceAuthError::InvalidGaiaCredentialsReason::
              CREDENTIALS_REJECTED_BY_SERVER));
}

PrefService* FileSystemRenameHandler::GetPrefs() {
  return PrefsFromDownloadItem(download_item());
}

base::WeakPtr<FileSystemRenameHandler>
FileSystemRenameHandler::RegisterSigninObserverForTesting(
    SigninExperienceTestObserver* observer) {
  // The FileSystemRenameHandler should open trigger the Box signin workflow
  // once. The observer should be set only once, otherwise one observer will
  // forcibly detach another observer.
  DCHECK(signin_observer_ == nullptr);
  signin_observer_ = observer;
  return weak_factory_.GetWeakPtr();
}

void FileSystemRenameHandler::UnregisterSigninObserverForTesting(
    SigninExperienceTestObserver* observer) {
  DCHECK(observer == signin_observer_);
  signin_observer_ = nullptr;
}

// FileSystemRenameHandler::TestObserver
FileSystemRenameHandler::TestObserver::TestObserver(
    FileSystemRenameHandler* rename_handler)
    : rename_handler_(rename_handler->weak_factory_.GetWeakPtr()) {
  rename_handler->observers_.AddObserver(this);
}

FileSystemRenameHandler::TestObserver::~TestObserver() {
  if (rename_handler_)
    rename_handler_->observers_.RemoveObserver(this);
}

void FileSystemRenameHandler::TestObserver::OnDestruction() {
  rename_handler_.reset();
}

// static
BoxUploader* FileSystemRenameHandler::TestObserver::GetBoxUploader(
    FileSystemRenameHandler* rename_handler) {
  return rename_handler->uploader_.get();
}

// RenameStartObserver
RenameStartObserver::RenameStartObserver(
    FileSystemRenameHandler* rename_handler)
    : FileSystemRenameHandler::TestObserver(rename_handler) {}

void RenameStartObserver::OnStart() {
  started_ = true;
  if (run_loop_.running())
    run_loop_.Quit();
}

void RenameStartObserver::WaitForStart() {
  if (!started_)
    run_loop_.Run();
}

// BoxFetchAccessTokenTestObserver
BoxFetchAccessTokenTestObserver::BoxFetchAccessTokenTestObserver(
    FileSystemRenameHandler* rename_handler)
    : FileSystemRenameHandler::TestObserver(rename_handler) {}

void BoxFetchAccessTokenTestObserver::OnFetchAccessTokenStart() {
  status_ = Status::kInProgress;
}

void BoxFetchAccessTokenTestObserver::OnAccessTokenFetched(
    const GoogleServiceAuthError& status) {
  fetch_token_err_ = status;
  status_ = Status::kSucceeded;
  if (run_loop_.running())
    run_loop_.Quit();
}

bool BoxFetchAccessTokenTestObserver::WaitForFetch() {
  if (status_ != Status::kSucceeded)
    run_loop_.Run();
  return fetch_token_err_.state() == GoogleServiceAuthError::State::NONE;
}

}  // namespace enterprise_connectors
