// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request_base.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/core/browser/download_check_result.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

class Profile;

namespace safe_browsing {

class CheckClientDownloadRequest : public CheckClientDownloadRequestBase,
                                   public download::DownloadItem::Observer {
 public:
  CheckClientDownloadRequest(
      download::DownloadItem* item,
      CheckDownloadRepeatingCallback callback,
      DownloadProtectionService* service,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor,
      base::optional_ref<const std::string> password = std::nullopt);

  CheckClientDownloadRequest(const CheckClientDownloadRequest&) = delete;
  CheckClientDownloadRequest& operator=(const CheckClientDownloadRequest&) =
      delete;

  ~CheckClientDownloadRequest() override;

  // download::DownloadItem::Observer:
  void OnDownloadDestroyed(download::DownloadItem* download) override;
  void OnDownloadUpdated(download::DownloadItem* download) override;

  static bool IsSupportedDownload(const download::DownloadItem& item,
                                  const base::FilePath& target_path,
                                  DownloadCheckResultReason* reason);

  download::DownloadItem* item() const override;

 private:
  // CheckClientDownloadRequestBase overrides:
  bool IsSupportedDownload(DownloadCheckResultReason* reason) override;
  content::BrowserContext* GetBrowserContext() const override;
  bool IsCancelled() override;
  base::WeakPtr<CheckClientDownloadRequestBase> GetWeakPtr() override;

  void NotifySendRequest(const ClientDownloadRequest* request) override;
  void SetDownloadProtectionData(
      const std::string& token,
      const ClientDownloadResponse::Verdict& verdict,
      const ClientDownloadResponse::TailoredVerdict& tailored_verdict) override;
  void MaybeBeginFeedbackForDownload(DownloadCheckResult result,
                                     bool upload_requested,
                                     const std::string& request_data,
                                     const std::string& response_body) override;
  bool ShouldImmediatelyDeepScan(bool server_requests_prompt,
                                 bool log_metrics) const override;
  bool ShouldPromptForDeepScanning(bool server_requests_prompt) const override;
  bool ShouldPromptForLocalDecryption(
      bool server_requests_prompt) const override;
  bool ShouldPromptForIncorrectPassword() const override;
  bool ShouldShowScanFailure() const override;
  void LogDeepScanningPrompt(bool did_prompt) const override;

  // Uploads the binary for deep scanning if the reason and policies indicate
  // it should be. ShouldUploadBinary will returns the settings to apply for
  // deep scanning if it should occur, or std::nullopt if no scan should be
  // done.
  std::optional<enterprise_connectors::AnalysisSettings> ShouldUploadBinary(
      DownloadCheckResultReason reason) override;
  void UploadBinary(DownloadCheckResult result,
                    DownloadCheckResultReason reason,
                    enterprise_connectors::AnalysisSettings settings) override;

  // Called when this request is completed.
  void NotifyRequestFinished(DownloadCheckResult result,
                             DownloadCheckResultReason reason) override;


  bool IsAllowlistedByPolicy() const override;

  bool IsUnderAdvancedProtection(Profile* profile) const;

  // The DownloadItem we are checking. Will be NULL if the request has been
  // canceled. Must be accessed only on UI thread.
  raw_ptr<download::DownloadItem> item_;
  std::optional<std::string> password_;
  CheckDownloadRepeatingCallback callback_;

  // Upload start time used for UMA duration histograms.
  base::TimeTicks upload_start_time_;

  base::WeakPtrFactory<CheckClientDownloadRequest> weakptr_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_H_
