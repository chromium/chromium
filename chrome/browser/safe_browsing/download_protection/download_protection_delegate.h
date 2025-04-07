// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_DELEGATE_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_DELEGATE_H_

#include <memory>

#include "net/traffic_annotation/network_traffic_annotation.h"

class GURL;

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

// Interface providing platform-specific logic for Download Protection, used
// with DownloadProtectionService, CheckClientDownloadRequest, and
// DownloadRequestMaker.
class DownloadProtectionDelegate {
 public:
  // Creates the appropriate implementation instance.
  static std::unique_ptr<DownloadProtectionDelegate> CreateForPlatform();

  virtual ~DownloadProtectionDelegate() = default;

  // Returns whether the download URL should be checked based on user
  // preferences.
  virtual bool ShouldCheckDownloadUrl(download::DownloadItem* item) const = 0;

  // Returns whether the download item should be checked by
  // CheckClientDownload() based on user preferences.
  virtual bool ShouldCheckClientDownload(
      download::DownloadItem* item) const = 0;

  // Returns whether the download item should be checked by
  // CheckClientDownload() based on whether the file supports the check.
  // May modify the DownloadItem with a SupportsUserData::Data.
  // TODO(chlily): Refactor and/or rename this, as it currently contains logic
  // based on things other than the file itself (i.e. random sampling).
  virtual bool IsSupportedDownload(download::DownloadItem& item,
                                   const base::FilePath& target_path) const = 0;

  // Called immediately prior to serializing the ClientDownloadRequest into the
  // string to send in the POST request body, which is followed by sending out
  // the network request. Allows the delegate to make final modifications to
  // the request. `item` is the download this pertains to, which may be null.
  virtual void PreSerializeRequest(const download::DownloadItem* item,
                                   ClientDownloadRequest& request_proto) {}

  // Called immediately prior to consuming the ResourceRequest used to send out
  // a download ping. Allows the delegate to make final modifications to the
  // ResourceRequest.
  virtual void FinalizeResourceRequest(
      network::ResourceRequest& resource_request) {}

  // Returns the URL that will be contacted for download protection requests.
  virtual const GURL& GetDownloadRequestUrl() const = 0;

  // Sampling rate for when an allowlisted download may generate a sampled ping,
  // if other requirements are met.
  virtual float GetAllowlistedDownloadSampleRate() const = 0;

  // Sampling rate for when an unsupported download may generate a sampled ping,
  // if other requirements are met.
  virtual float GetUnsupportedFileSampleRate(
      const base::FilePath& filename) const = 0;

  // Completes the network traffic annotation for CheckClientDownloadRequest.
  virtual net::NetworkTrafficAnnotationTag
  CompleteClientDownloadRequestTrafficAnnotation(
      const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
      const = 0;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_DELEGATE_H_
