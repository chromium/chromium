// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_BASE_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_BASE_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/cancelable_callback.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/download_protection/download_request_maker.h"
#include "chrome/browser/safe_browsing/download_protection/file_analyzer.h"
#include "chrome/services/file_util/public/cpp/sandboxed_rar_analyzer.h"
#include "chrome/services/file_util/public/cpp/sandboxed_zip_analyzer.h"
#include "components/history/core/browser/history_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/common/safe_browsing/disk_image_type_sniffer_mac.h"
#include "chrome/services/file_util/public/cpp/sandboxed_dmg_analyzer_mac.h"
#endif

namespace network {
class SimpleURLLoader;
}

namespace safe_browsing {

class CheckClientDownloadRequestBase {
 public:
  CheckClientDownloadRequestBase(
      GURL source_url,
      base::FilePath target_file_path,
      content::BrowserContext* browser_context,
      CheckDownloadCallback callback,
      DownloadProtectionService* service,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      std::unique_ptr<DownloadRequestMaker> download_request_maker);

  CheckClientDownloadRequestBase(const CheckClientDownloadRequestBase&) =
      delete;
  CheckClientDownloadRequestBase& operator=(
      const CheckClientDownloadRequestBase&) = delete;

  virtual ~CheckClientDownloadRequestBase();

  void Start();

  DownloadProtectionService* service() const { return service_; }

  virtual download::DownloadItem* item() const = 0;

 protected:
  // Subclasses can call this method to mark the request as finished (for
  // example because the download was cancelled) before the safe browsing
  // check has completed. This method can end up deleting |this|.
  void FinishRequest(DownloadCheckResult result,
                     DownloadCheckResultReason reason);

 private:
  using ArchivedBinaries =
      google::protobuf::RepeatedPtrField<ClientDownloadRequest_ArchivedBinary>;

  bool ShouldSampleAllowlistedDownload();
  bool ShouldSampleUnsupportedFile(const base::FilePath& filename);
  bool IsDownloadManuallyBlocklisted(const ClientDownloadRequest& request);

  void OnUrlAllowlistCheckDone(bool is_allowlisted);
  void OnRequestBuilt(std::unique_ptr<ClientDownloadRequest> request_proto);

  void StartTimeout();
  void SendRequest();
  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);

  // If we need to perform additional prompting (e.g. deep scanning, local
  // password decryption) due to the response, this method will update `result`
  // and `reason` appropriately. This method also performs logging related to
  // these additional prompts. If both prompts are allowed, deep scanning will
  // be prioritized.
  void GetAdditionalPromptResult(const ClientDownloadResponse& response,
                                 DownloadCheckResult* result,
                                 DownloadCheckResultReason* reason,
                                 std::string* token) const;

  virtual bool IsSupportedDownload(DownloadCheckResultReason* reason) = 0;
  virtual content::BrowserContext* GetBrowserContext() const = 0;
  virtual bool IsCancelled() = 0;
  virtual base::WeakPtr<CheckClientDownloadRequestBase> GetWeakPtr() = 0;

  // Called right before a network request is send to the server.
  virtual void NotifySendRequest(const ClientDownloadRequest* request) = 0;

  // TODO(mek): The following three methods are all called after receiving a
  // response from the server. Perhaps these should be combined into one hook
  // for concrete sub classes to implement, rather than having three separate
  // hooks with slightly different logic when they are called.

  // Called with the client download response as returned by the server, if one
  // was returned and the returned verdict is unsafe (i.e. not safe or unknown).
  virtual void SetDownloadProtectionData(
      const std::string& token,
      const ClientDownloadResponse::Verdict& verdict,
      const ClientDownloadResponse::TailoredVerdict& tailored_verdict) = 0;

  // Called when a valid response has been received from the server.
  virtual void MaybeBeginFeedbackForDownload(
      DownloadCheckResult result,
      bool upload_requested,
      const std::string& request_data,
      const std::string& response_body) = 0;

  // Returns whether or not the file should be uploaded to Safe Browsing for
  // deep scanning. Returns the settings to apply for analysis if the file
  // should be uploaded for deep scanning, or std::nullopt if it should not.
  virtual std::optional<enterprise_connectors::AnalysisSettings>
  ShouldUploadBinary(DownloadCheckResultReason reason) = 0;

  // If ShouldUploadBinary returns settings, actually performs the upload to
  // Safe Browsing for deep scanning.
  virtual void UploadBinary(
      DownloadCheckResult result,
      DownloadCheckResultReason reason,
      enterprise_connectors::AnalysisSettings settings) = 0;

  // Called whenever a request has completed.
  virtual void NotifyRequestFinished(DownloadCheckResult result,
                                     DownloadCheckResultReason reason) = 0;

  // Called when finishing the download, to decide whether to
  // immediately start deep scanning or not. Implementations should log
  // metrics only when `log_metrics` is true.
  virtual bool ShouldImmediatelyDeepScan(bool server_requests_prompt,
                                         bool log_metrics) const = 0;

  // Called when finishing the download, to decide whether to prompt the user
  // for deep scanning or not.
  virtual bool ShouldPromptForDeepScanning(
      bool server_requests_prompt) const = 0;

  // Called when finishing the download, to decide whether to prompt the user
  // for local decryption or not.
  virtual bool ShouldPromptForLocalDecryption(
      bool server_requests_prompt) const = 0;

  // Called when |token_fetcher_| has finished fetching the access token.
  void OnGotAccessToken(const std::string& access_token);

  // Called at the request start to determine if we should bailout due to the
  // file being allowlisted by policy
  virtual bool IsAllowlistedByPolicy() const = 0;

  // For sampled unsupported file types, replaces all URLs in
  // |client_download_request_| with their origin.
  void SanitizeRequest();

  // Called when we decide whether or not to show a deep scanning prompt
  virtual void LogDeepScanningPrompt(bool did_prompt) const = 0;

  // Returns whether we should skip sending a ping to Safe Browsing because the
  // provided password was incorrect.
  virtual bool ShouldPromptForIncorrectPassword() const = 0;

  // Returns whether we should skip sending a ping to Safe Browsing because
  // extraction failed in a way that makes the data useless (e.g. disk write
  // failure).
  virtual bool ShouldShowScanFailure() const = 0;

  // Source URL being downloaded from. This should always be set, but could be
  // for example an artificial blob: URL if there is no source URL.
  const GURL source_url_;
  const base::FilePath target_file_path_;

  CheckDownloadCallback callback_;

  // A cancelable closure used to track the timeout. If we decide to upload the
  // file for deep scanning, we want to cancel the timeout so it doesn't trigger
  // in the middle of scanning.
  base::CancelableOnceClosure timeout_closure_;

  std::unique_ptr<network::SimpleURLLoader> loader_;
  std::unique_ptr<ClientDownloadRequest> client_download_request_;
  std::string client_download_request_data_;

  const raw_ptr<DownloadProtectionService> service_;
  const scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  const bool pingback_enabled_;
  base::CancelableTaskTracker request_tracker_;  // For HistoryService lookup.
  base::TimeTicks start_time_ = base::TimeTicks::Now();  // Used for stats.
  base::TimeTicks timeout_start_time_;
  base::TimeTicks request_start_time_;
  bool skipped_url_allowlist_ = false;
  bool skipped_certificate_allowlist_ = false;
  bool sampled_unsupported_file_ = false;

  bool is_extended_reporting_ = false;
  bool is_incognito_ = false;
  bool is_enhanced_protection_ = false;

  // The token fetcher used to attach OAuth access tokens to requests for
  // appropriately consented users.
  std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher_;

  // The OAuth access token for the user profile, if needed in the request.
  std::string access_token_;

  // Used to create the download request proto.
  std::unique_ptr<DownloadRequestMaker> download_request_maker_;
};  // namespace safe_browsing

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_BASE_H_
