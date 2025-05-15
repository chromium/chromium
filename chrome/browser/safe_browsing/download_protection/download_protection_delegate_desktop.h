// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_DELEGATE_DESKTOP_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_DELEGATE_DESKTOP_H_

#include "chrome/browser/safe_browsing/download_protection/download_protection_delegate.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace content {
struct FileSystemAccessWriteItem;
}

namespace download {
class DownloadItem;
}

namespace network {
struct ResourceRequest;
}

namespace safe_browsing {

class DownloadProtectionDelegateDesktop : public DownloadProtectionDelegate {
 public:
  DownloadProtectionDelegateDesktop();
  ~DownloadProtectionDelegateDesktop() override;

  // DownloadProtectionDelegate:
  bool ShouldCheckDownloadUrl(download::DownloadItem* item) const override;
  bool MayCheckClientDownload(download::DownloadItem* item) const override;
  bool MayCheckFileSystemAccessWrite(
      content::FileSystemAccessWriteItem* item) const override;
  MayCheckDownloadResult IsSupportedDownload(
      download::DownloadItem& item,
      const base::FilePath& target_path) const override;
  void FinalizeResourceRequest(
      network::ResourceRequest& resource_request) override;
  const GURL& GetDownloadRequestUrl() const override;
  net::NetworkTrafficAnnotationTag
  CompleteClientDownloadRequestTrafficAnnotation(
      const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
      const override;
  float GetAllowlistedDownloadSampleRate() const override;
  float GetUnsupportedFileSampleRate(
      const base::FilePath& filename) const override;

 private:
  const GURL download_request_url_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_DELEGATE_DESKTOP_H_
