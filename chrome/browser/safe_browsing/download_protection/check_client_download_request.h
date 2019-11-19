// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "chrome/browser/safe_browsing/download_protection/binary_upload_service.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request_base.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "components/download/public/common/download_item.h"
#include "components/safe_browsing/proto/webprotect.pb.h"
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
      scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor);
  ~CheckClientDownloadRequest() override;

  void OnDownloadDestroyed(download::DownloadItem* download) override;
  static bool IsSupportedDownload(const download::DownloadItem& item,
                                  const base::FilePath& target_path,
                                  DownloadCheckResultReason* reason,
                                  ClientDownloadRequest::DownloadType* type);

 private:
  // CheckClientDownloadRequestBase overrides:
  bool IsSupportedDownload(DownloadCheckResultReason* reason,
                           ClientDownloadRequest::DownloadType* type) override;
  content::BrowserContext* GetBrowserContext() override;
  bool IsCancelled() override;
  void PopulateRequest(ClientDownloadRequest* request) override;
  base::WeakPtr<CheckClientDownloadRequestBase> GetWeakPtr() override;

  void NotifySendRequest(const ClientDownloadRequest* request) override;
  void SetDownloadPingToken(const std::string& token) override;
  void MaybeStorePingsForDownload(DownloadCheckResult result,
                                  bool upload_requested,
                                  const std::string& request_data,
                                  const std::string& response_body) override;

  // Returns true if the CheckClientDownloadRequest returned the
  // ASYNC_SCANNING result while it does deep scanning.
  bool MaybeReturnAsynchronousVerdict(
      DownloadCheckResultReason reason) override;

  // Uploads the binary for deep scanning if the reason and policies indicate
  // it should be.
  bool ShouldUploadBinary(DownloadCheckResultReason reason) override;
  void UploadBinary(DownloadCheckResult result,
                    DownloadCheckResultReason reason) override;

  // Called when this request is completed.
  void NotifyRequestFinished(DownloadCheckResult result,
                             DownloadCheckResultReason reason) override;

  // Returns true when the file should be uploaded for a DLP compliance scan.
  // This consults the CheckContentCompliance enterprise policy.
  bool ShouldUploadForDlpScan();

  // Returns true when the file should be uploaded for a malware scan. This
  // consults the SendFilesForMalwareCheck enterprise policy.
  bool ShouldUploadForMalwareScan(DownloadCheckResultReason reason);

  // Called when deep scanning is complete. Where appropriate, it updates the
  // download UX, and sends a real time report about the download.
  void OnDeepScanningComplete(BinaryUploadService::Result result,
                              DeepScanningClientResponse response);

  // The DownloadItem we are checking. Will be NULL if the request has been
  // canceled. Must be accessed only on UI thread.
  download::DownloadItem* item_;
  CheckDownloadRepeatingCallback callback_;

  // When uploading files for deep scanning, we need to preserve the original
  // result and reason from the server, just in case deep scanning fails.
  DownloadCheckResult saved_result_;
  DownloadCheckResultReason saved_reason_;

  base::WeakPtrFactory<CheckClientDownloadRequest> weakptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(CheckClientDownloadRequest);
};

// Helper function to examine a DeepScanningClientResponse and report the
// appropriate events to the enterprise admin.
void MaybeReportDeepScanningVerdict(Profile* profile,
                                    const GURL& url,
                                    const std::string& file_name,
                                    const std::string& download_digest_sha256,
                                    const std::string& mime_type,
                                    const std::string& trigger,
                                    const int64_t content_size,
                                    BinaryUploadService::Result result,
                                    DeepScanningClientResponse response);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_CLIENT_DOWNLOAD_REQUEST_H_
