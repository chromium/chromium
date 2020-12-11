// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_BASE_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_BASE_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/cancelable_callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/download_protection/file_analyzer.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "chrome/services/file_util/public/cpp/sandboxed_rar_analyzer.h"
#include "chrome/services/file_util/public/cpp/sandboxed_zip_analyzer.h"
#include "components/history/core/browser/history_service.h"
#include "components/safe_browsing/core/browser/safe_browsing_token_fetcher.h"
#include "components/safe_browsing/core/db/database_manager.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

#if defined(OS_MAC)
#include "chrome/common/safe_browsing/disk_image_type_sniffer_mac.h"
#include "chrome/services/file_util/public/cpp/sandboxed_dmg_analyzer_mac.h"
#endif

namespace network {
class SimpleURLLoader;
}

namespace safe_browsing {

class CheckClientDownloadRequestBase {
 public:
  // URL and referrer of the window the download was started from.
  struct TabUrls {
    GURL url;
    GURL referrer;
  };

  CheckClientDownloadRequestBase(
      GURL source_url,
      base::FilePath target_file_path,
      base::FilePath full_path,
      TabUrls tab_urls,
      std::string mime_type,
      std::string hash,
      content::BrowserContext* browser_context,
      CheckDownloadCallback callback,
      DownloadProtectionService* service,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor);
  virtual ~CheckClientDownloadRequestBase();

  void Start();

  DownloadProtectionService* service() const { return service_; }

 protected:
  // Prepares URLs to be put into a ping message. Currently this just shortens
  // data: URIs, other URLs are included verbatim. If this is a sampled binary,
  // we'll send a light-ping which strips PII from the URL.
  std::string SanitizeUrl(const GURL& url) const;

  // Subclasses can call this method to mark the request as finished (for
  // example because the download was cancelled) before the safe browsing
  // check has completed. This method can end up deleting |this|.
  void FinishRequest(DownloadCheckResult result,
                     DownloadCheckResultReason reason);

 private:
  using ArchivedBinaries =
      google::protobuf::RepeatedPtrField<ClientDownloadRequest_ArchivedBinary>;

  bool ShouldSampleWhitelistedDownload();
  bool ShouldSampleUnsupportedFile(const base::FilePath& filename);
  bool IsDownloadManuallyBlacklisted(const ClientDownloadRequest& request);

  void OnUrlWhitelistCheckDone(bool is_whitelisted);
  // Performs file feature extraction and SafeBrowsing ping for downloads that
  // don't match the URL whitelist.
  void AnalyzeFile();
  void OnFileFeatureExtractionDone(FileAnalyzer::Results results);
  void StartTimeout();
  void OnCertificateWhitelistCheckDone(bool is_whitelisted);
  void GetTabRedirects();
  void OnGotTabRedirects(history::RedirectList redirect_list);
  void SendRequest();
  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);

  virtual bool IsSupportedDownload(
      DownloadCheckResultReason* reason,
      ClientDownloadRequest::DownloadType* type) = 0;
  virtual content::BrowserContext* GetBrowserContext() const = 0;
  virtual bool IsCancelled() = 0;
  virtual base::WeakPtr<CheckClientDownloadRequestBase> GetWeakPtr() = 0;

  // Called to populate any data in the request that is specific for the type of
  // check being done. Most importantly this method is expected to set the hash
  // and size of the download, add DOWNLOAD_URL and DOWNLOAD_REDIRECT
  // resources to the request, and set the referrer chain.
  virtual void PopulateRequest(ClientDownloadRequest* request) = 0;

  // Called right before a network request is send to the server.
  virtual void NotifySendRequest(const ClientDownloadRequest* request) = 0;

  // TODO(mek): The following three methods are all called after receiving a
  // response from the server. Perhaps these should be combined into one hook
  // for concrete sub classes to implement, rather than having three separate
  // hooks with slightly different logic when they are called.

  // Called with the download ping token as returned by the server, if one was
  // returned.
  virtual void SetDownloadPingToken(const std::string& token) = 0;

  // Called when a valid response has been received from the server.
  virtual void MaybeStorePingsForDownload(DownloadCheckResult result,
                                          bool upload_requested,
                                          const std::string& request_data,
                                          const std::string& response_body) = 0;

  // Returns whether or not the file should be uploaded to Safe Browsing for
  // deep scanning. Returns the settings to apply for analysis if the file
  // should be uploaded for deep scanning, or base::nullopt if it should not.
  virtual base::Optional<enterprise_connectors::AnalysisSettings>
  ShouldUploadBinary(DownloadCheckResultReason reason) = 0;

  // If ShouldUploadBinary returns settings, actually performs the upload to
  // Safe Browsing for deep scanning.
  virtual void UploadBinary(
      DownloadCheckResultReason reason,
      enterprise_connectors::AnalysisSettings settings) = 0;

  // Called whenever a request has completed.
  virtual void NotifyRequestFinished(DownloadCheckResult result,
                                     DownloadCheckResultReason reason) = 0;

  // Called when finishing the download, to decide whether to prompt the user
  // for deep scanning or not.
  virtual bool ShouldPromptForDeepScanning(
      DownloadCheckResultReason reason) const = 0;

  // Called when |token_fetcher_| has finished fetching the access token.
  void OnGotAccessToken(
      base::Optional<signin::AccessTokenInfo> access_token_info);

  // Called at the request start to determine if we should bailout due to the
  // file being whitelisted by policy
  virtual bool IsWhitelistedByPolicy() const = 0;

  // Source URL being downloaded from. This shuold always be set, but could be
  // for example an artificial blob: URL if there is no source URL.
  const GURL source_url_;
  const base::FilePath target_file_path_;
  const base::FilePath full_path_;
  // URL and referrer of the window the download was started from.
  const GURL tab_url_;
  const GURL tab_referrer_url_;
  // URL chain of redirects leading to (but not including) |tab_url|.
  std::vector<GURL> tab_redirects_;

  CheckDownloadCallback callback_;

  // A cancelable closure used to track the timeout. If we decide to upload the
  // file for deep scanning, we want to cancel the timeout so it doesn't trigger
  // in the middle of scanning.
  base::CancelableOnceClosure timeout_closure_;

  std::unique_ptr<network::SimpleURLLoader> loader_;
  std::string client_download_request_data_;

  bool archived_executable_ = false;
  FileAnalyzer::ArchiveValid archive_is_valid_ =
      FileAnalyzer::ArchiveValid::UNSET;

#if defined(OS_MAC)
  std::unique_ptr<std::vector<uint8_t>> disk_image_signature_;
  google::protobuf::RepeatedPtrField<
      ClientDownloadRequest_DetachedCodeSignature>
      detached_code_signatures_;
#endif

  ClientDownloadRequest_SignatureInfo signature_info_;
  std::unique_ptr<ClientDownloadRequest_ImageHeaders> image_headers_;
  ArchivedBinaries archived_binaries_;

  DownloadProtectionService* const service_;
  const scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor_;
  const scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  const bool pingback_enabled_;
  const std::unique_ptr<FileAnalyzer> file_analyzer_ =
      std::make_unique<FileAnalyzer>(binary_feature_extractor_);
  ClientDownloadRequest::DownloadType type_ =
      ClientDownloadRequest::WIN_EXECUTABLE;
  base::CancelableTaskTracker request_tracker_;  // For HistoryService lookup.
  base::TimeTicks start_time_ = base::TimeTicks::Now();  // Used for stats.
  base::TimeTicks timeout_start_time_;
  base::TimeTicks request_start_time_;
  bool skipped_url_whitelist_ = false;
  bool skipped_certificate_whitelist_ = false;

  bool is_extended_reporting_ = false;
  bool is_incognito_ = false;
  bool is_under_advanced_protection_ = false;
  bool is_enhanced_protection_ = false;

  int file_count_;
  int directory_count_;

  // The mime type of the download, if known.
  std::string mime_type_;

  // The hash of the download, if known.
  std::string hash_;

  // The token fetcher used to attach OAuth access tokens to requests for
  // appropriately consented users.
  std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher_;

  // The OAuth access token for the user profile, if needed in the request.
  std::string access_token_;

  DISALLOW_COPY_AND_ASSIGN(CheckClientDownloadRequestBase);
};  // namespace safe_browsing

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_BASE_H_
