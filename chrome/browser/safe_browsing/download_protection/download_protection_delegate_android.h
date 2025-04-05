// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_DELEGATE_ANDROID_H_

#include <optional>

#include "chrome/browser/safe_browsing/download_protection/download_protection_delegate.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace base {
class FilePath;
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
  bool ShouldCheckClientDownload(download::DownloadItem* item) const override;
  bool IsSupportedDownload(download::DownloadItem& item,
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
  const GURL download_request_url_;

  // Overrides the next call to ShouldSample() within IsSupportedDownload(), for
  // convenience in tests to bypass the random number generator.
  mutable std::optional<bool> should_sample_override_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_DELEGATE_ANDROID_H_
