// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_FILE_SYSTEM_ACCESS_WRITE_REQUEST_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_FILE_SYSTEM_ACCESS_WRITE_REQUEST_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request_base.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/file_system_access_write_item.h"
#include "url/gurl.h"

namespace safe_browsing {

class CheckFileSystemAccessWriteRequest
    : public CheckClientDownloadRequestBase {
 public:
  CheckFileSystemAccessWriteRequest(
      std::unique_ptr<content::FileSystemAccessWriteItem> item,
      CheckDownloadCallback callback,
      DownloadProtectionService* service,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor);

  CheckFileSystemAccessWriteRequest(const CheckFileSystemAccessWriteRequest&) =
      delete;
  CheckFileSystemAccessWriteRequest& operator=(
      const CheckFileSystemAccessWriteRequest&) = delete;

  ~CheckFileSystemAccessWriteRequest() override;

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
  std::optional<enterprise_connectors::AnalysisSettings> ShouldUploadBinary(
      DownloadCheckResultReason reason) override;
  void UploadBinary(DownloadCheckResult result,
                    DownloadCheckResultReason reason,
                    enterprise_connectors::AnalysisSettings settings) override;
  bool ShouldImmediatelyDeepScan(bool server_requests_prompt,
                                 bool log_metrics) const override;
  bool ShouldPromptForDeepScanning(bool server_requests_prompt) const override;
  bool ShouldPromptForLocalDecryption(
      bool server_requests_prompt) const override;
  bool ShouldPromptForIncorrectPassword() const override;
  bool ShouldShowScanFailure() const override;
  void NotifyRequestFinished(DownloadCheckResult result,
                             DownloadCheckResultReason reason) override;
  bool IsAllowlistedByPolicy() const override;
  void LogDeepScanningPrompt(bool did_prompt) const override;

  const std::unique_ptr<content::FileSystemAccessWriteItem> item_;
  std::unique_ptr<ReferrerChainData> referrer_chain_data_;

  base::WeakPtrFactory<CheckFileSystemAccessWriteRequest> weakptr_factory_{
      this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_FILE_SYSTEM_ACCESS_WRITE_REQUEST_H_
