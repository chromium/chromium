// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/supports_user_data.h"
#include "base/task/cancelable_task_tracker.h"
#include "build/build_config.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/download_protection/file_analyzer.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/common/safe_browsing/binary_feature_extractor.h"
#include "chrome/services/file_util/public/cpp/sandboxed_rar_analyzer.h"
#include "chrome/services/file_util/public/cpp/sandboxed_zip_analyzer.h"
#include "components/download/public/common/download_item.h"
#include "components/history/core/browser/history_service.h"
#include "components/safe_browsing/db/database_manager.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

#if defined(OS_MACOSX)
#include "chrome/common/safe_browsing/disk_image_type_sniffer_mac.h"
#include "chrome/services/file_util/public/cpp/sandboxed_dmg_analyzer_mac.h"
#endif

using content::BrowserThread;

namespace network {
class SimpleURLLoader;
}

namespace safe_browsing {

class CheckClientDownloadRequest
    : public base::RefCountedThreadSafe<CheckClientDownloadRequest,
                                        BrowserThread::DeleteOnUIThread>,
      public download::DownloadItem::Observer {
 public:
  CheckClientDownloadRequest(
      download::DownloadItem* item,
      const CheckDownloadCallback& callback,
      DownloadProtectionService* service,
      const scoped_refptr<SafeBrowsingDatabaseManager>& database_manager,
      BinaryFeatureExtractor* binary_feature_extractor);
  bool ShouldSampleUnsupportedFile(const base::FilePath& filename);
  void Start();
  void StartTimeout();

  // |download_destroyed| indicates if cancellation is due to the destruction of
  // the download item.
  void Cancel(bool download_destroyed);
  void OnDownloadDestroyed(download::DownloadItem* download) override;
  void OnURLLoaderComplete(std::unique_ptr<std::string> response_body);
  static bool IsSupportedDownload(const download::DownloadItem& item,
                                  const base::FilePath& target_path,
                                  DownloadCheckResultReason* reason,
                                  ClientDownloadRequest::DownloadType* type);

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::UI>;
  friend class base::DeleteHelper<CheckClientDownloadRequest>;

  using ArchivedBinaries =
      google::protobuf::RepeatedPtrField<ClientDownloadRequest_ArchivedBinary>;

  ~CheckClientDownloadRequest() override;
  // Performs file feature extraction and SafeBrowsing ping for downloads that
  // don't match the URL whitelist.
  void AnalyzeFile();
  void OnFileFeatureExtractionDone(FileAnalyzer::Results results);
  void StartExtractFileFeatures();
  void ExtractFileFeatures(const base::FilePath& file_path);
  void StartExtractRarFeatures();
  void OnRarAnalysisFinished(const ArchiveAnalyzerResults& results);
  void StartExtractZipFeatures();
  void OnZipAnalysisFinished(const ArchiveAnalyzerResults& results);

  static void CopyArchivedBinaries(const ArchivedBinaries& src_binaries,
                                   ArchivedBinaries* dest_binaries);

#if defined(OS_MACOSX)
  void StartExtractDmgFeatures();
  void ExtractFileOrDmgFeatures(bool download_file_has_koly_signature);
  void OnDmgAnalysisFinished(const ArchiveAnalyzerResults& results);
#endif  // defined(OS_MACOSX)

  bool ShouldSampleWhitelistedDownload();
  // Checks the download URL against SafeBrowsing whitelist. If download URL is
  // on whitelist, file feature extraction and download ping are skipped.
  void CheckUrlAgainstWhitelist();
  void CheckCertificateChainAgainstWhitelist();
  void GetTabRedirects();
  void OnGotTabRedirects(const GURL& url,
                         const history::RedirectList* redirect_list);
  bool IsDownloadManuallyBlacklisted(const ClientDownloadRequest& request);
  std::string SanitizeUrl(const GURL& url) const;
  void SendRequest();
  void PostFinishTask(DownloadCheckResult result,
                      DownloadCheckResultReason reason);
  void FinishRequest(DownloadCheckResult result,
                     DownloadCheckResultReason reason);
  bool CertificateChainIsWhitelisted(
      const ClientDownloadRequest_CertificateChain& chain);

  // The DownloadItem we are checking. Will be NULL if the request has been
  // canceled. Must be accessed only on UI thread.
  download::DownloadItem* item_;
  // Copies of data from |item_| for access on other threads.
  std::vector<GURL> url_chain_;
  GURL referrer_url_;
  // URL chain of redirects leading to (but not including) |tab_url|.
  std::vector<GURL> tab_redirects_;
  // URL and referrer of the window the download was started from.
  GURL tab_url_;
  GURL tab_referrer_url_;

  bool archived_executable_;
  FileAnalyzer::ArchiveValid archive_is_valid_;

#if defined(OS_MACOSX)
  std::unique_ptr<std::vector<uint8_t>> disk_image_signature_;
  google::protobuf::RepeatedPtrField<
      ClientDownloadRequest_DetachedCodeSignature>
      detached_code_signatures_;
#endif

  ClientDownloadRequest_SignatureInfo signature_info_;
  std::unique_ptr<ClientDownloadRequest_ImageHeaders> image_headers_;
  ArchivedBinaries archived_binaries_;
  CheckDownloadCallback callback_;
  // Will be NULL if the request has been canceled.
  DownloadProtectionService* service_;
  scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor_;
  scoped_refptr<SafeBrowsingDatabaseManager> database_manager_;
  const bool pingback_enabled_;
  std::unique_ptr<network::SimpleURLLoader> loader_;
  std::unique_ptr<FileAnalyzer> file_analyzer_;
  bool finished_;
  ClientDownloadRequest::DownloadType type_;
  std::string client_download_request_data_;
  base::CancelableTaskTracker request_tracker_;  // For HistoryService lookup.
  base::TimeTicks start_time_;                   // Used for stats.
  base::TimeTicks timeout_start_time_;
  base::TimeTicks request_start_time_;
  bool skipped_url_whitelist_;
  bool skipped_certificate_whitelist_;
  bool is_extended_reporting_;
  bool is_incognito_;
  bool is_under_advanced_protection_;
  // This task tracker is used for posting the URL whitelist check to the IO
  // thread. The posted task will be cancelled if DownloadItem gets destroyed.
  base::CancelableTaskTracker cancelable_task_tracker_;
  base::WeakPtrFactory<CheckClientDownloadRequest> weakptr_factory_;

  FRIEND_TEST_ALL_PREFIXES(CheckClientDownloadRequestTest,
                           CheckLimitArchivedExtensions);

  DISALLOW_COPY_AND_ASSIGN(CheckClientDownloadRequest);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_H_
