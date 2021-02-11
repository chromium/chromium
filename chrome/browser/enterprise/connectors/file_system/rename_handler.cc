// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/file_system/rename_handler.h"

#include <utility>

#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/download_item_utils.h"

namespace enterprise_connectors {

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

// Don't hold on to |download_item| since it can only be accessed from a
// specific thread.  Get all the info we need from it now.
FileSystemRenameHandler::FileSystemRenameHandler(
    download::DownloadItem* download_item,
    FileSystemSettings settings)
    : download::DownloadItemRenameHandler(download_item),
      settings_(std::move(settings)) {}

FileSystemRenameHandler::~FileSystemRenameHandler() = default;

void FileSystemRenameHandler::Start(Callback callback) {
  download_callback_ = std::move(callback);

  // TODO(alicego): Hook up to download controller.
  NotifyResultToDownloadThread(false);
}

void FileSystemRenameHandler::OpenDownload() {}

void FileSystemRenameHandler::ShowDownloadInContext() {}

void FileSystemRenameHandler::NotifyResultToDownloadThread(bool success) {
  // TODO(crbug.com/1168815): define required error messages.
  auto reason = success ? download::DOWNLOAD_INTERRUPT_REASON_NONE
                        : download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED;
  std::move(download_callback_).Run(reason, target_path_);
}

}  // namespace enterprise_connectors
