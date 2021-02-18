// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/rename_handler.h"

#include <utility>

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

}  // namespace
const base::Feature kFileSystemConnectorEnabled{
    "FileSystemConnectorsEnabled", base::FEATURE_DISABLED_BY_DEFAULT};

// static
base::Optional<FileSystemSettings> FileSystemRenameHandler::IsEnabled(
    download::DownloadItem* download_item) {
  if (!base::FeatureList::IsEnabled(kFileSystemConnectorEnabled))
    return base::nullopt;

  ConnectorsService* service = ConnectorsServiceFactory::GetForBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(download_item));
  return service->GetFileSystemSettings(
      download_item->GetURL(), FileSystemConnector::SEND_DOWNLOAD_TO_CLOUD);
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

// The only permitted use of |download_item| in this class is passing it to
// content::DownloadItemUtils methods.  This class runs in the UI thread where
// method of |download_item| should not be called.
FileSystemRenameHandler::FileSystemRenameHandler(
    download::DownloadItem* download_item,
    FileSystemSettings settings)
    : download::DownloadItemRenameHandler(download_item),
      settings_(std::move(settings)) {}

FileSystemRenameHandler::~FileSystemRenameHandler() = default;

void FileSystemRenameHandler::Start(Callback callback) {
  download_callback_ = std::move(callback);
  StartInternal();
}

void FileSystemRenameHandler::OpenDownload() {}

void FileSystemRenameHandler::ShowDownloadInContext() {}

void FileSystemRenameHandler::StartInternal() {
  content::BrowserContext* context =
      content::DownloadItemUtils::GetBrowserContext(download_item());
  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(download_item());
  if (!context || !web_contents) {
    NotifyResultToDownloadThread(false);
    return;
  }

  PrefService* prefs = PrefsFromBrowserContext(context);
  if (!prefs) {
    NotifyResultToDownloadThread(false);
    return;
  }

  std::string access_token;
  std::string refresh_token;
  bool ok = GetFileSystemOAuth2Tokens(prefs, settings_.service_provider,
                                      &access_token, &refresh_token);

  if (ok && !access_token.empty()) {
    // TODO(crbug.com/1179763): Hook up to download controller.
    NotifyResultToDownloadThread(false);
  } else if (ok && !refresh_token.empty()) {
    content::StoragePartition* partition =
        content::BrowserContext::GetStoragePartitionForSite(context,
                                                            settings_.home);
    token_fetcher_ = std::make_unique<AccessTokenFetcher>(
        partition->GetURLLoaderFactoryForBrowserProcess(),
        settings_.service_provider, settings_.token_endpoint, refresh_token,
        std::string(),
        base::BindOnce(&FileSystemRenameHandler::OnAccessTokenFetched,
                       weak_factory_.GetWeakPtr()));
    token_fetcher_->Start(settings_.client_id, settings_.client_secret,
                          settings_.scopes);
  } else {
    FileSystemSigninDialogDelegate::ShowDialog(
        web_contents, settings_,
        base::BindOnce(&FileSystemRenameHandler::OnAccessTokenFetched,
                       weak_factory_.GetWeakPtr()));
  }
}

void FileSystemRenameHandler::OnAccessTokenFetched(
    bool status,
    const std::string& access_token,
    const std::string& refresh_token) {
  if (status) {
    PrefService* prefs = PrefsFromDownloadItem(download_item());
    if (prefs && SetFileSystemOAuth2Tokens(prefs, settings_.service_provider,
                                           access_token, refresh_token)) {
      StartInternal();
      return;
    }
  }

  NotifyResultToDownloadThread(false);
}

void FileSystemRenameHandler::NotifyResultToDownloadThread(bool success) {
  // TODO(crbug.com/1168815): define required error messages.
  auto reason = success ? download::DOWNLOAD_INTERRUPT_REASON_NONE
                        : download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
  std::move(download_callback_).Run(reason, target_path_);
}

}  // namespace enterprise_connectors
