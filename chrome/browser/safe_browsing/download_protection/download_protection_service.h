// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Helper class which handles communication with the SafeBrowsing servers for
// improved binary download protection.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_SERVICE_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/download_protection/deep_scanning_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_observer.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/services_delegate.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/safe_browsing/core/browser/db/database_manager.h"
#include "components/safe_browsing/core/browser/safe_browsing_metrics_collector.h"
#include "components/sessions/core/session_id.h"
#include "url/gurl.h"

namespace content {
class PageNavigator;
struct FileSystemAccessWriteItem;
}  // namespace content

namespace download {
class DownloadItem;
}

namespace network {
class SharedURLLoaderFactory;
}

class Profile;

namespace safe_browsing {
class BinaryFeatureExtractor;
class CheckClientDownloadRequest;
class CheckClientDownloadRequestBase;
class CheckFileSystemAccessWriteRequest;
class ClientDownloadRequest;
class DownloadRequestMaker;
class DownloadFeedbackService;
class PPAPIDownloadRequest;

// This class provides an asynchronous API to check whether a particular
// client download is malicious or not.
class DownloadProtectionService {
 public:
  // Creates a download service.  The service is initially disabled.  You need
  // to call SetEnabled() to start it.  |sb_service| owns this object.
  explicit DownloadProtectionService(SafeBrowsingServiceImpl* sb_service);

  DownloadProtectionService(const DownloadProtectionService&) = delete;
  DownloadProtectionService& operator=(const DownloadProtectionService&) =
      delete;

  virtual ~DownloadProtectionService();

  // Parse a flag of blocklisted sha256 hashes to check at each download.
  // This is used for testing, to hunt for safe-browsing by-pass bugs.
  virtual void ParseManualBlocklistFlag();

  // Return true if this hash value is blocklisted via flag (for testing).
  virtual bool IsHashManuallyBlocklisted(const std::string& sha256_hash) const;

  // Checks whether the given client download is likely to be malicious or not.
  // The result is delivered asynchronously via the given callback.  This
  // method must be called on the UI thread, and the callback will also be
  // invoked on the UI thread.  This method must be called once the download
  // is finished and written to disk.
  virtual void CheckClientDownload(
      download::DownloadItem* item,
      CheckDownloadRepeatingCallback callback,
      base::optional_ref<const std::string> password = std::nullopt);

  // Checks the user permissions, then calls |CheckClientDownload| if
  // appropriate. Returns whether we began scanning.
  virtual bool MaybeCheckClientDownload(
      download::DownloadItem* item,
      CheckDownloadRepeatingCallback callback);

  // Cancel the pending check for `item`. This function simply drops the pending
  // work in the `DownloadProtectionService`. The caller is responsible for
  // updating the download state so that it completes successfully.
  void CancelChecksForDownload(download::DownloadItem* item);

  // Returns whether the download URL should be checked for safety based on user
  // prefs.
  virtual bool ShouldCheckDownloadUrl(download::DownloadItem* item);

  // Checks whether any of the URLs in the redirect chain of the
  // download match the SafeBrowsing bad binary URL list.  The result is
  // delivered asynchronously via the given callback.  This method must be
  // called on the UI thread, and the callback will also be invoked on the UI
  // thread.  Pre-condition: !info.download_url_chain.empty().
  virtual void CheckDownloadUrl(download::DownloadItem* item,
                                CheckDownloadCallback callback);

  // Returns true iff the download specified by |info| should be scanned by
  // CheckClientDownload() for malicious content.
  virtual bool IsSupportedDownload(const download::DownloadItem& item,
                                   const base::FilePath& target_path) const;

  virtual void CheckPPAPIDownloadRequest(
      const GURL& requestor_url,
      content::RenderFrameHost* initiating_frame,
      const base::FilePath& default_file_path,
      const std::vector<base::FilePath::StringType>& alternate_extensions,
      Profile* profile,
      CheckDownloadCallback callback);

  // Checks whether the given File System Access write operation is likely to be
  // malicious or not. The result is delivered asynchronously via the given
  // callback.  This method must be called on the UI thread, and the callback
  // will also be invoked on the UI thread.  This method must be called once the
  // write is finished and data has been written to disk.
  virtual void CheckFileSystemAccessWrite(
      std::unique_ptr<content::FileSystemAccessWriteItem> item,
      CheckDownloadCallback callback);

  // Display more information to the user regarding the download specified by
  // |info|. This method is invoked when the user requests more information
  // about a download that was marked as malicious.
  void ShowDetailsForDownload(const download::DownloadItem* item,
                              content::PageNavigator* navigator);

  // Enables or disables the service.  This is usually called by the
  // SafeBrowsingServiceImpl, which tracks whether any profile uses these
  // services at all.  Disabling causes any pending and future requests to have
  // their callbacks called with "UNKNOWN" results.
  void SetEnabled(bool enabled);

  bool enabled() const { return enabled_; }

  // Returns the timeout that is used by CheckClientDownload().
  base::TimeDelta GetDownloadRequestTimeout() const;

  // Checks the user permissions, and submits the downloaded file if
  // appropriate. Returns whether the submission was successful.
  bool MaybeBeginFeedbackForDownload(Profile* profile,
                                     download::DownloadItem* download,
                                     const std::string& ping_request,
                                     const std::string& ping_response);

  // Registers a callback that will be run when a ClientDownloadRequest has
  // been formed.
  base::CallbackListSubscription RegisterClientDownloadRequestCallback(
      const ClientDownloadRequestCallback& callback);

  // Registers a callback that will be run when a FileSystemAccessWriteRequest
  // has been formed.
  base::CallbackListSubscription RegisterFileSystemAccessWriteRequestCallback(
      const FileSystemAccessWriteRequestCallback& callback);

  // Registers a callback that will be run when a PPAPI ClientDownloadRequest
  // has been formed.
  base::CallbackListSubscription RegisterPPAPIDownloadRequestCallback(
      const PPAPIDownloadRequestCallback& callback);

  double allowlist_sample_rate() const { return allowlist_sample_rate_; }

  static void SetDownloadProtectionData(
      download::DownloadItem* item,
      const std::string& token,
      const ClientDownloadResponse::Verdict& verdict,
      const ClientDownloadResponse::TailoredVerdict& tailored_verdict);

  static std::string GetDownloadPingToken(const download::DownloadItem* item);

  // Whether a DownloadProtectionData is found on the item.
  static bool HasDownloadProtectionVerdict(const download::DownloadItem* item);

  // Returns ClientDownloadResponse::SAFE by default if no
  // DownloadProtectionData is found.
  static ClientDownloadResponse::Verdict GetDownloadProtectionVerdict(
      const download::DownloadItem* item);

  static ClientDownloadResponse::TailoredVerdict
  GetDownloadProtectionTailoredVerdict(const download::DownloadItem* item);

  // Sends dangerous download opened report when download is opened or
  // shown in folder.
  void MaybeSendDangerousDownloadOpenedReport(download::DownloadItem* item,
                                              bool show_download_in_folder);

  // Called to trigger a bypass event report for |download|. This is used when
  // the async scan verdict is received for a file that was already opened by
  // the user while it was being processed, and the verdict ended up being
  // "dangerous" or "sensitive".
  void ReportDelayedBypassEvent(download::DownloadItem* download,
                                download::DownloadDangerType danger_type);

  // Uploads `item` to Safe Browsing for deep scanning, using the upload
  // service attached to the profile `item` was downloaded in. This is
  // non-blocking, and the result we be provided through `callback`. `trigger`
  // is used to identify the reason for deep scanning, aka enterprise policy or
  // APP. `download_check_result` indicates the previously known SB verdict to
  // apply to the download should deep scanning fail. `analysis_settings`
  // contains settings to apply throughout scanning (types of scans to do,
  // whether to block/allow large files, etc). This must be called on the UI
  // thread.
  void UploadForDeepScanning(
      download::DownloadItem* item,
      CheckDownloadRepeatingCallback callback,
      DownloadItemWarningData::DeepScanTrigger trigger,
      DownloadCheckResult download_check_result,
      enterprise_connectors::AnalysisSettings analysis_settings,
      base::optional_ref<const std::string> password);

  // Helper functions for encrypted archive scans.
  static void UploadForConsumerDeepScanning(
      download::DownloadItem* item,
      DownloadItemWarningData::DeepScanTrigger trigger,
      base::optional_ref<const std::string> password);
  static void CheckDownloadWithLocalDecryption(
      download::DownloadItem* item,
      base::optional_ref<const std::string> password);

  // Uploads a save package `item` for deep scanning. `save_package_file`
  // contains a mapping of on-disk files part of that save package to their
  // final paths.
  void UploadSavePackageForDeepScanning(
      download::DownloadItem* item,
      base::flat_map<base::FilePath, base::FilePath> save_package_files,
      CheckDownloadRepeatingCallback callback,
      enterprise_connectors::AnalysisSettings analysis_settings);

  // Returns all the currently active deep scanning requests.
  std::vector<DeepScanningRequest*> GetDeepScanningRequests();

  virtual scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory(
      content::BrowserContext* browser_context);

  // Removes all pending download requests that are associated with the
  // `browser_context`.
  void RemovePendingDownloadRequests(content::BrowserContext* browser_context);

  // Returns the maximum number of user gestures for a download referrer
  // chain. If `item` is non-null, information about that download may
  // change the limit.
  static int GetDownloadAttributionUserGestureLimit(
      download::DownloadItem* item = nullptr);

 private:
  friend class PPAPIDownloadRequest;
  friend class DownloadUrlSBClient;
  friend class DownloadProtectionServiceTestBase;
  friend class DownloadDangerPromptTest;
  friend class CheckClientDownloadRequestBase;
  friend class CheckClientDownloadRequest;
  friend class CheckFileSystemAccessWriteRequest;
  friend class DeepScanningRequest;
  friend class DownloadRequestMaker;

  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceMockTimeTest,
                           TestDownloadRequestTimeout);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           PPAPIDownloadRequest_InvalidResponse);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           PPAPIDownloadRequest_Timeout);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           VerifyReferrerChainWithEmptyNavigationHistory);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           VerifyReferrerChainLengthForExtendedReporting);

  static const void* const kDownloadProtectionDataKey;

  // Helper class for easy setting and getting data related to download
  // protection. The data is only set when the server returns an unsafe verdict
  // (i.e. not safe or unknown).
  class DownloadProtectionData : public base::SupportsUserData::Data {
   public:
    explicit DownloadProtectionData(
        const std::string& token,
        const ClientDownloadResponse::Verdict& verdict,
        const ClientDownloadResponse::TailoredVerdict& tailored_verdict)
        : token_string_(token),
          verdict_(verdict),
          tailored_verdict_(tailored_verdict) {}

    DownloadProtectionData(const DownloadProtectionData&) = delete;
    DownloadProtectionData& operator=(const DownloadProtectionData&) = delete;

    std::string token_string() { return token_string_; }
    ClientDownloadResponse::Verdict verdict() { return verdict_; }
    ClientDownloadResponse::TailoredVerdict tailored_verdict() {
      return tailored_verdict_;
    }

   private:
    std::string token_string_;
    ClientDownloadResponse::Verdict verdict_;
    ClientDownloadResponse::TailoredVerdict tailored_verdict_;
  };

  // Cancels all requests in |download_requests_|, and empties it, releasing
  // the references to the requests.
  void CancelPendingRequests();

  // Called by a CheckClientDownloadRequest instance when it finishes, to
  // remove it from |download_requests_| and to report security sensitive
  // events to safe_browsing_metrics_collector.
  void RequestFinished(CheckClientDownloadRequestBase* request,
                       content::BrowserContext* browser_context,
                       DownloadCheckResult result);

  // Called by a DeepScanningRequest when it finishes, to remove it from
  // |deep_scanning_requests_|.
  virtual void RequestFinished(DeepScanningRequest* request);

  void PPAPIDownloadCheckRequestFinished(PPAPIDownloadRequest* request);

  // Identify referrer chain of the PPAPI download based on the frame URL where
  // the download is initiated. Then add referrer chain info to
  // ClientDownloadRequest proto. This function also records UMA stats of
  // download attribution result.
  void AddReferrerChainToPPAPIClientDownloadRequest(
      content::WebContents* web_contents,
      const GURL& initiating_frame_url,
      const content::GlobalRenderFrameHostId&
          initiating_outermost_main_frame_id,
      const GURL& initiating_main_frame_url,
      SessionID tab_id,
      bool has_user_gesture,
      ClientDownloadRequest* out_request);

  void OnDangerousDownloadOpened(const download::DownloadItem* item,
                                 Profile* profile);

  // Get the BinaryUploadService for the given |profile|. Virtual so it can be
  // overridden in tests.
  virtual BinaryUploadService* GetBinaryUploadService(
      Profile* profile,
      const enterprise_connectors::AnalysisSettings& settings);

  // Get the SafeBrowsingNavigationObserverManager for the given |web_contents|.
  SafeBrowsingNavigationObserverManager* GetNavigationObserverManager(
      content::WebContents* web_contents);

  // Callback when deep scanning has finished, but we may want to do the
  // metadata check anyway.
  void MaybeCheckMetadataAfterDeepScanning(
      download::DownloadItem* item,
      CheckDownloadRepeatingCallback callback,
      DownloadCheckResult result);

  raw_ptr<SafeBrowsingServiceImpl> sb_service_;
  // These pointers may be NULL if SafeBrowsing is disabled.
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;

  // Set of pending server requests for DownloadManager mediated downloads.
  base::flat_map<
      content::BrowserContext*,
      base::flat_map<CheckClientDownloadRequestBase*,
                     std::unique_ptr<CheckClientDownloadRequestBase>>>
      context_download_requests_;

  // Set of pending server requests for PPAPI mediated downloads.
  base::flat_map<PPAPIDownloadRequest*, std::unique_ptr<PPAPIDownloadRequest>>
      ppapi_download_requests_;

  // Set of pending server requests for deep scanning.
  base::flat_map<DeepScanningRequest*, std::unique_ptr<DeepScanningRequest>>
      deep_scanning_requests_;

  // Keeps track of the state of the service.
  bool enabled_;

  // BinaryFeatureExtractor object, may be overridden for testing.
  scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor_;

  int64_t download_request_timeout_ms_;

  std::unique_ptr<DownloadFeedbackService> feedback_service_;

  // A list of callbacks to be run on the main thread when a
  // ClientDownloadRequest has been formed.
  ClientDownloadRequestCallbackList client_download_request_callbacks_;

  // A list of callbacks to be run on the main thread when a
  // FileSystemAccessWriteRequest has been formed.
  FileSystemAccessWriteRequestCallbackList
      file_system_access_write_request_callbacks_;

  // A list of callbacks to be run on the main thread when a
  // PPAPIDownloadRequest has been formed.
  PPAPIDownloadRequestCallbackList ppapi_download_request_callbacks_;

  // List of 8-byte hashes that are blocklisted manually by flag.
  // Normally empty.
  std::set<std::string> manual_blocklist_hashes_;

  // Rate of allowlisted downloads we sample to send out download ping.
  double allowlist_sample_rate_;

  // DownloadProtectionObserver to send real time reports for dangerous download
  // events and handle special user actions on the download.
  DownloadProtectionObserver download_protection_observer_;

  base::WeakPtrFactory<DownloadProtectionService> weak_ptr_factory_;
};
}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_SERVICE_H_
