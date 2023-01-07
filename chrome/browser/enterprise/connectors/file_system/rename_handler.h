// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_RENAME_HANDLER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_RENAME_HANDLER_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "components/download/public/common/download_interrupt_reasons.h"
#include "components/download/public/common/download_item_rename_handler.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class PrefService;

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace enterprise_connectors {

class AccessTokenFetcher;
class BoxUploader;
class SigninExperienceTestObserver;

using InterruptReason = download::DownloadInterruptReason;
constexpr auto kSuccess = download::DOWNLOAD_INTERRUPT_REASON_NONE;
constexpr auto kServiceProviderUnknownError =
    download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED;
constexpr auto kServiceProviderDown =
    download::DOWNLOAD_INTERRUPT_REASON_NETWORK_SERVER_DOWN;
constexpr auto kBrowserFailure = download::DOWNLOAD_INTERRUPT_REASON_CRASH;
constexpr auto kCredentialUpdateFailure = kBrowserFailure;
constexpr auto kSignInCancellation =
    download::DOWNLOAD_INTERRUPT_REASON_USER_CANCELED;

// An implementation of download::DownloadItemRenameHandler that sends a
// download item file to a cloud-based storage provider as specified in the
// SendDownloadToCloudEnterpriseConnector policy.
class FileSystemRenameHandler : public download::DownloadItemRenameHandler {
 public:
  static std::unique_ptr<download::DownloadItemRenameHandler> CreateIfNeeded(
      download::DownloadItem* download_item);

  class TestObserver : public base::CheckedObserver {
   public:
    explicit TestObserver(FileSystemRenameHandler* rename_handler);
    ~TestObserver() override;

    enum Status { kNotStarted, kInProgress, kSucceeded, kFailed };

    virtual void OnStart() {}
    virtual void OnFetchAccessTokenStart() {}
    virtual void OnAccessTokenFetched(const GoogleServiceAuthError& status) {}
    virtual void OnDestruction();

    static BoxUploader* GetBoxUploader(FileSystemRenameHandler* rename_handler);

   private:
    base::WeakPtr<FileSystemRenameHandler> rename_handler_;
  };

  FileSystemRenameHandler(download::DownloadItem* download_item,
                          FileSystemSettings settings);
  explicit FileSystemRenameHandler(download::DownloadItem* download_item);
  ~FileSystemRenameHandler() override;

  base::WeakPtr<FileSystemRenameHandler> RegisterSigninObserverForTesting(
      SigninExperienceTestObserver* observer);
  void UnregisterSigninObserverForTesting(
      SigninExperienceTestObserver* observer);

 protected:
  // download::DownloadItemRenameHandler interface.
  void Start(ProgressUpdateCallback progress_update_cb,
             DownloadCallback upload_complete_cb) override;
  void OpenDownload() override;
  void ShowDownloadInContext() override;

  // These methods are declared protected to be overridden in unit tests so that
  // calls to other components can be isolated.
  virtual void TryUploaderTask(content::BrowserContext* context,
                               const std::string& access_token);
  virtual void PromptUserSignInForAuthorization(content::WebContents* contents);
  virtual void FetchAccessToken(content::BrowserContext* context,
                                const std::string& refresh_token);

  // These methods are declared protected so that they can be used in tests.
  void SetUploaderForTesting(std::unique_ptr<BoxUploader> fake_uploader);
  // Callback for PromptUserSignInForAuthorization().
  void OnAuthorization(const GoogleServiceAuthError& status,
                       const std::string& access_token,
                       const std::string& refresh_token);
  // Callback for FetchAccessToken().
  void OnAccessTokenFetched(const GoogleServiceAuthError& status,
                            const std::string& access_token,
                            const std::string& refresh_token);

 private:
  static absl::optional<FileSystemSettings> IsEnabled(
      download::DownloadItem* download_item);

  void StartInternal(std::string access_token = std::string());
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory(
      content::BrowserContext* context);

  // Helper method used in OpenDownload() and ShowDownloadInContext().
  void AddTabToShowDownload(const GURL& url);

  // Called when failure status is returned via callbacks but is not
  // GoogleServiceAuthError::State::REQUEST_CANCELED.
  void OnAuthenticationError(const GoogleServiceAuthError& error);
  // Called when failure status is returned via callbacks and is
  // GoogleServiceAuthError::State::REQUEST_CANCELED.
  void OnSignInCancellation();
  // Callback for uploader_ upon API requests returning authentication error.
  void OnApiAuthenticationError();

  PrefService* GetPrefs();

  // Copied from policy settings. Constant for the life of the rename handler.
  const FileSystemSettings settings_;

  std::unique_ptr<AccessTokenFetcher> token_fetcher_;
  // Main uploader that manages the entire API call flow of file upload.
  std::unique_ptr<BoxUploader> uploader_;
  base::ObserverList<TestObserver> observers_;
  raw_ptr<SigninExperienceTestObserver> signin_observer_ = nullptr;
  base::WeakPtrFactory<FileSystemRenameHandler> weak_factory_{this};
};

class RenameStartObserver : public FileSystemRenameHandler::TestObserver {
 public:
  explicit RenameStartObserver(FileSystemRenameHandler* rename_handler);
  ~RenameStartObserver() override = default;

  // RenameHandlerObserver methods
  void OnStart() override;

  void WaitForStart();

 private:
  bool started_ = false;
  base::RunLoop run_loop_;
};

class BoxFetchAccessTokenTestObserver
    : public FileSystemRenameHandler::TestObserver {
 public:
  explicit BoxFetchAccessTokenTestObserver(
      FileSystemRenameHandler* rename_handler);
  ~BoxFetchAccessTokenTestObserver() override = default;

  // RenameHandlerObserver methods
  void OnFetchAccessTokenStart() override;
  void OnAccessTokenFetched(const GoogleServiceAuthError& status) override;

  bool WaitForFetch();

 private:
  Status status_ = Status::kNotStarted;
  GoogleServiceAuthError fetch_token_err_;
  base::RunLoop run_loop_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_FILE_SYSTEM_RENAME_HANDLER_H_
