// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_RENAME_HANDLER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_RENAME_HANDLER_H_

#include "base/callback.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/file_system/download_controller.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item_rename_handler.h"
#include "google_apis/gaia/google_service_auth_error.h"

class PrefService;
namespace content {
class BrowserContext;
class WebContents;
}  // namespace content
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

 protected:
  // These methods are declared protected to override in tests so that calls to
  // other components can be isolated.
  virtual void TryControllerTask(content::BrowserContext* context,
                                 const std::string& access_token);
  virtual void PromptUserSignInForAuthorization(content::WebContents* contents);
  virtual void FetchAccessToken(content::BrowserContext* context,
                                const std::string& refresh_token);

  // These methods are declared protected so that they can be used in tests.
  FileSystemDownloadController* GetControllerForTesting();
  // Callback for PromptUserSignInForAuthorization().
  void OnAuthorization(const GoogleServiceAuthError& status,
                       const std::string& access_token,
                       const std::string& refresh_token);
  // Callback for FetchAccessToken().
  void OnAccessTokenFetched(const GoogleServiceAuthError& status,
                            const std::string& access_token,
                            const std::string& refresh_token);

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
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory(
      content::BrowserContext* context);

  // Called when failure status is returned via callbacks but is not
  // GoogleServiceAuthError::State::REQUEST_CANCELED.
  void OnAuthenticationError(const GoogleServiceAuthError& error);
  // Called when failure status is returned via callbacks and is
  // GoogleServiceAuthError::State::REQUEST_CANCELED.
  void OnSignInCancellation();
  // Callback for controller_ upon API requests returning authentication error.
  void OnApiAuthenticationError();
  // Callback for controller_ as well as upon failure.
  void NotifyResultToDownloadThread(bool success);

  PrefService* GetPrefs();

  // Fields copied from |download_item| or from policy settings.  These are
  // constant for the life of the rename handler.
  const base::FilePath target_path_;
  const FileSystemSettings settings_;

  // Invoked to tell the download system when the rename has completed.
  Callback download_callback_;

  std::unique_ptr<AccessTokenFetcher> token_fetcher_;
  // Main controller that manages the entire API call flow of file upload.
  FileSystemDownloadController controller_;
  base::WeakPtrFactory<FileSystemRenameHandler> weak_factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_RENAME_HANDLER_H_
