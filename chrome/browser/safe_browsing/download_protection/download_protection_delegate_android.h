// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_DELEGATE_ANDROID_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_delegate.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/browser/safe_browsing/download_protection/rate_limiting_key_manager.h"
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
  std::vector<PendingClientDownloadRequestModification>
  ProduceClientDownloadRequestModifications(const download::DownloadItem* item,
                                            Profile* profile) override;
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

  // Generates a modification to add `rate_limiting_key` in the
  // ClientDownloadRequest. If there's no initialized RateLimitingKeyManager,
  // this kicks off a lookup of safety_net_id in order to initialize the
  // manager, then tries again using that.
  void PopulateRateLimitingKey(const std::string& profile_id,
                               CollectModificationCallback callback);

  void InitRateLimitingKeyManager(const std::string& safety_net_id);

  const GURL download_request_url_;

  // Overrides the next call to ShouldSample() within IsSupportedDownload(), for
  // convenience in tests to bypass the random number generator.
  mutable std::optional<bool> should_sample_override_;

  std::optional<RateLimitingKeyManager> rate_limiting_key_manager_;

  base::WeakPtrFactory<DownloadProtectionDelegateAndroid> weak_factory_{this};
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_DELEGATE_ANDROID_H_
