// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_DELEGATE_ANDROID_H_

#include <optional>

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

class ClientDownloadRequest;

class DownloadProtectionDelegateAndroid : public DownloadProtectionDelegate {
 public:
  DownloadProtectionDelegateAndroid();
  ~DownloadProtectionDelegateAndroid() override;

  // DownloadProtectionDelegate:
  bool ShouldCheckDownloadUrl(download::DownloadItem* item) const override;
  bool MayCheckClientDownload(download::DownloadItem* item) const override;
  bool MayCheckFileSystemAccessWrite(
      content::FileSystemAccessWriteItem* item) const override;
  MayCheckDownloadResult IsSupportedDownload(
      download::DownloadItem& item,
      const base::FilePath& target_path) const override;
  void PreSerializeRequest(const download::DownloadItem* item,
                           ClientDownloadRequest& request_proto) override;
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

  // Used only for tests. Set the outcome of the next call to ShouldSample()
  // within IsSupportedDownload(), for convenience in tests to bypass the random
  // number generator.
  void SetNextShouldSampleForTesting(bool should_sample);

 private:
  // Executes one instance of random sampling for a file that would otherwise
  // send a download request, taking any testing override into account.
  // Note: this sampling performed by DownloadProtectionDelegateAndroid is
  // distinct from sampling for "light" pings for unsupported filetypes, and
  // sampling of allowlisted files.
  bool ShouldSampleEligibleFile() const;

  // Translates a MayCheckDownloadResult into a bool to return from
  // MayCheck{ClientDownload,FileSystemAccessWrite}.
  // If `download_item` is non-null, this updates metrics data accordingly.
  bool MayCheckItem(MayCheckDownloadResult may_check_download_result,
                    download::DownloadItem* download_item = nullptr) const;

  const GURL download_request_url_;

  // Overrides the next call to ShouldSample() within IsSupportedDownload(), for
  // convenience in tests to bypass the random number generator.
  mutable std::optional<bool> should_sample_override_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_DELEGATE_ANDROID_H_
