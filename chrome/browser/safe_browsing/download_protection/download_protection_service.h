// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include <unordered_map>
#include <vector>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/download/download_commands.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/download_protection/deep_scanning_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/download_protection/download_reporter.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"
#include "chrome/browser/safe_browsing/services_delegate.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "components/safe_browsing/core/db/database_manager.h"
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
  explicit DownloadProtectionService(SafeBrowsingService* sb_service);

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
  virtual void CheckClientDownload(download::DownloadItem* item,
                                   CheckDownloadRepeatingCallback callback);

  // Checks the user permissions, then calls |CheckClientDownload| if
  // appropriate. Returns whether we began scanning.
  virtual bool MaybeCheckClientDownload(
      download::DownloadItem* item,
      CheckDownloadRepeatingCallback callback);

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
      const GURL& initiating_frame_url,
      content::WebContents* web_contents,
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
  // SafeBrowsingService, which tracks whether any profile uses these services
  // at all.  Disabling causes any pending and future requests to have their
  // callbacks called with "UNKNOWN" results.
  void SetEnabled(bool enabled);

  bool enabled() const { return enabled_; }

  // Returns the timeout that is used by CheckClientDownload().
  int64_t download_request_timeout_ms() const {
    return download_request_timeout_ms_;
  }

  // Checks the user permissions, and submits the downloaded file if
  // appropriate. Returns whether the submission was successful.
  bool MaybeBeginFeedbackForDownload(
      Profile* profile,
      download::DownloadItem* download,
      DownloadCommands::Command download_command);

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

  scoped_refptr<SafeBrowsingNavigationObserverManager>
  navigation_observer_manager() {
    return navigation_observer_manager_;
  }

  static void SetDownloadPingToken(download::DownloadItem* item,
                                   const std::string& token);

  static std::string GetDownloadPingToken(const download::DownloadItem* item);

  // Sends dangerous download opened report when download is opened or
  // shown in folder, and if the following conditions are met:
  // (1) it is a dangerous download.
  // (2) user is NOT in incognito mode.
  // (3) user is opted-in for extended reporting.
  void MaybeSendDangerousDownloadOpenedReport(
      const download::DownloadItem* item,
      bool show_download_in_folder);

  // Uploads |item| to Safe Browsing for deep scanning, using the upload
  // service attached to the profile |item| was downloaded in. This is
  // non-blocking, and the result we be provided through |callback|. |source| is
  // used to identify the reason for deep scanning. Only the scan types listed
  // in |allowed_scans| will be performed. This must be called on the UI
  // thread.
  void UploadForDeepScanning(
      download::DownloadItem* item,
      CheckDownloadRepeatingCallback callback,
      DeepScanningRequest::DeepScanTrigger trigger,
      enterprise_connectors::AnalysisSettings analysis_settings);

  virtual scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory(
      content::BrowserContext* browser_context);

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

  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           TestDownloadRequestTimeout);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           PPAPIDownloadRequest_InvalidResponse);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           PPAPIDownloadRequest_Timeout);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           VerifyReferrerChainWithEmptyNavigationHistory);
  FRIEND_TEST_ALL_PREFIXES(DownloadProtectionServiceTest,
                           VerifyReferrerChainLengthForExtendedReporting);

  static const void* const kDownloadPingTokenKey;

  // Helper class for easy setting and getting token string.
  class DownloadPingToken : public base::SupportsUserData::Data {
   public:
    explicit DownloadPingToken(const std::string& token)
        : token_string_(token) {}

    std::string token_string() { return token_string_; }

   private:
    std::string token_string_;

    DISALLOW_COPY_AND_ASSIGN(DownloadPingToken);
  };

  // Cancels all requests in |download_requests_|, and empties it, releasing
  // the references to the requests.
  void CancelPendingRequests();

  // Called by a CheckClientDownloadRequest instance when it finishes, to
  // remove it from |download_requests_|.
  void RequestFinished(CheckClientDownloadRequestBase* request);

  // Called by a DeepScanningRequest when it finishes, to remove it from
  // |deep_scanning_requests_|.
  virtual void RequestFinished(DeepScanningRequest* request);

  void PPAPIDownloadCheckRequestFinished(PPAPIDownloadRequest* request);

  // Identify referrer chain info of a download. This function also records UMA
  // stats of download attribution result.
  std::unique_ptr<ReferrerChainData> IdentifyReferrerChain(
      const download::DownloadItem& item);

  // Identify referrer chain info of a File System Access write. This function
  // also records UMA stats of download attribution result.
  std::unique_ptr<ReferrerChainData> IdentifyReferrerChain(
      const content::FileSystemAccessWriteItem& item);

  // Identify referrer chain of the PPAPI download based on the frame URL where
  // the download is initiated. Then add referrer chain info to
  // ClientDownloadRequest proto. This function also records UMA stats of
  // download attribution result.
  void AddReferrerChainToPPAPIClientDownloadRequest(
      const GURL& initiating_frame_url,
      const GURL& initiating_main_frame_url,
      SessionID tab_id,
      bool has_user_gesture,
      ClientDownloadRequest* out_request);

  void OnDangerousDownloadOpened(const download::DownloadItem* item,
                                 Profile* profile);

  // Get the BinaryUploadService for the given |profile|. Virtual so it can be
  // overridden in tests.
  virtual BinaryUploadService* GetBinaryUploadService(Profile* profile);

  SafeBrowsingService* sb_service_;
  // These pointers may be NULL if SafeBrowsing is disabled.
  scoped_refptr<SafeBrowsingUIManager> ui_manager_;
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  scoped_refptr<SafeBrowsingNavigationObserverManager>
      navigation_observer_manager_;

  // Set of pending server requests for DownloadManager mediated downloads.
  base::flat_map<CheckClientDownloadRequestBase*,
                 std::unique_ptr<CheckClientDownloadRequestBase>>
      download_requests_;

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

  // DownloadReporter to send real time reports for dangerous download events.
  DownloadReporter download_reporter_;

  base::WeakPtrFactory<DownloadProtectionService> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadProtectionService);
};
}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_SERVICE_H_
