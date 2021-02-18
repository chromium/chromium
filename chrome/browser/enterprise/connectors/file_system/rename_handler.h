// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_RENAME_HANDLER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_RENAME_HANDLER_H_

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item_rename_handler.h"

namespace enterprise_connectors {

class AccessTokenFetcher;

// Experimental flag to enable or disable the file system connector.
extern const base::Feature kFileSystemConnectorEnabled;

// An implementation of download::DownloadItemRenameHandler that sends a
// download item file to a cloud-based storage provider as specified in the
// SendDownloadToCloudEnterpriseConnector policy.
class FileSystemRenameHandler : public download::DownloadItemRenameHandler {
 public:
  static std::unique_ptr<download::DownloadItemRenameHandler> CreateIfNeeded(
      download::DownloadItem* download_item);

  FileSystemRenameHandler(download::DownloadItem* download_item,
                          FileSystemSettings settings);
  ~FileSystemRenameHandler() override;

 private:
  static base::Optional<FileSystemSettings> IsEnabled(
      download::DownloadItem* download_item);

  static std::unique_ptr<download::DownloadItemRenameHandler> Create(
      download::DownloadItem* download_item,
      FileSystemSettings settings);

  // download::DownloadItemRenameHandler interface.
  void Start(Callback callback) override;
  void OpenDownload() override;
  void ShowDownloadInContext() override;

  void StartInternal();
  void OnAccessTokenFetched(bool status,
                            const std::string& access_token,
                            const std::string& refresh_token);
  void NotifyResultToDownloadThread(bool success);

  // Fields copied from |download_item| or from policy settings.  These are
  // constant for the life of the rename handler.
  const base::FilePath target_path_;
  const FileSystemSettings settings_;

  // Invoked to tell the download system when the rename has completed.
  Callback download_callback_;
  std::unique_ptr<AccessTokenFetcher> token_fetcher_;
  base::WeakPtrFactory<FileSystemRenameHandler> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_RENAME_HANDLER_H_
