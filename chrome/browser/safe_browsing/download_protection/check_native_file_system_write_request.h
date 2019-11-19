// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_NATIVE_FILE_SYSTEM_WRITE_REQUEST_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_NATIVE_FILE_SYSTEM_WRITE_REQUEST_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request_base.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "components/download/public/common/download_item.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/native_file_system_write_item.h"
#include "url/gurl.h"

namespace safe_browsing {

class CheckNativeFileSystemWriteRequest
    : public CheckClientDownloadRequestBase {
 public:
  CheckNativeFileSystemWriteRequest(
      std::unique_ptr<content::NativeFileSystemWriteItem> item,
      CheckDownloadCallback callback,
      DownloadProtectionService* service,
      scoped_refptr<SafeBrowsingDatabaseManager> database_manager,
      scoped_refptr<BinaryFeatureExtractor> binary_feature_extractor);
  ~CheckNativeFileSystemWriteRequest() override;

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
  bool MaybeReturnAsynchronousVerdict(
      DownloadCheckResultReason reason) override;
  bool ShouldUploadBinary(DownloadCheckResultReason reason) override;
  void UploadBinary(DownloadCheckResult result,
                    DownloadCheckResultReason reason) override;
  void NotifyRequestFinished(DownloadCheckResult result,
                             DownloadCheckResultReason reason) override;

  const std::unique_ptr<content::NativeFileSystemWriteItem> item_;
  std::unique_ptr<ReferrerChainData> referrer_chain_data_;

  base::WeakPtrFactory<CheckNativeFileSystemWriteRequest> weakptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(CheckNativeFileSystemWriteRequest);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_CHECK_NATIVE_FILE_SYSTEM_WRITE_REQUEST_H_
