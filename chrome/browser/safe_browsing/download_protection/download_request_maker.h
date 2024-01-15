// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_REQUEST_MAKER_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_REQUEST_MAKER_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/safe_browsing/download_protection/file_analyzer.h"
#include "components/download/public/common/download_item.h"
#include "components/history/core/browser/history_service.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "content/public/browser/file_system_access_write_item.h"

namespace safe_browsing {

class DownloadProtectionService;

// This class encapsulate the process of populating all the fields in a Safe
// Browsing download ping.
class DownloadRequestMaker {
 public:
  using Callback =
      base::OnceCallback<void(std::unique_ptr<ClientDownloadRequest>)>;

  // URL and referrer of the window the download was started from.
  struct TabUrls {
    GURL url;
    GURL referrer;
  };

  static std::unique_ptr<DownloadRequestMaker> CreateFromDownloadItem(
      scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor,
      download::DownloadItem* item,
      base::optional_ref<const std::string> password = std::nullopt);

  static std::unique_ptr<DownloadRequestMaker> CreateFromFileSystemAccess(
      scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor,
      DownloadProtectionService* service,
      const content::FileSystemAccessWriteItem& item);

  DownloadRequestMaker(
      scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor,
      content::BrowserContext* browser_context,
      TabUrls tab_urls,
      base::FilePath target_file_path,
      base::FilePath full_path,
      GURL source_url,
      std::string sha256_hash,
      int64_t length,
      const std::vector<ClientDownloadRequest::Resource>& resources,
      bool is_user_initiated,
      ReferrerChainData* referrer_chain_data,
      base::optional_ref<const std::string> password,
      const std::string& previous_token,
      base::OnceCallback<void(const FileAnalyzer::Results&)>
          on_results_callback);

  DownloadRequestMaker(const DownloadRequestMaker&) = delete;
  DownloadRequestMaker& operator=(const DownloadRequestMaker&) = delete;

  ~DownloadRequestMaker();

  // Starts filling in fields in the download ping. Will run the callback with
  // the fully-populated ping.
  void Start(Callback callback);

 private:
  // Callback when |file_analyzer_| is done analyzing the download.
  void OnFileFeatureExtractionDone(FileAnalyzer::Results results);

  // Helper function to get the tab redirects from the history service.
  void GetTabRedirects();

  // Callback when the history service has retrieved the tab redirects.
  void OnGotTabRedirects(history::RedirectList redirect_list);

  // Populates the tailored info field for tailored warnings.
  void PopulateTailoredInfo();

  raw_ptr<content::BrowserContext> browser_context_;
  std::unique_ptr<ClientDownloadRequest> request_;
  const scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor_;
  const std::unique_ptr<FileAnalyzer> file_analyzer_ =
      std::make_unique<FileAnalyzer>(binary_feature_extractor_);
  base::CancelableTaskTracker request_tracker_;  // For HistoryService lookup.

  // The current URL for the WebContents that initiated the download, and its
  // referrer.
  TabUrls tab_urls_;

  // The ultimate destination for the download.
  const base::FilePath target_file_path_;

  // The current path to the file contents.
  const base::FilePath full_path_;

  const std::optional<std::string> password_;

  // Callback used for handling behavior specific to download items of file
  // system accesses.
  base::OnceCallback<void(const FileAnalyzer::Results&)> on_results_callback_;

  Callback callback_;

  base::WeakPtrFactory<DownloadRequestMaker> weakptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_REQUEST_MAKER_H_
