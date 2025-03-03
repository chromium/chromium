// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_SAFE_BROWSING_DOWNLOAD_PROTECTION_DOWNLOAD_PROTECTION_DELEGATE_ANDROID_H_

#include <optional>

#include "base/supports_user_data.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_delegate.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}

namespace download {
class DownloadItem;
}

namespace safe_browsing {

class DownloadProtectionDelegateAndroid : public DownloadProtectionDelegate {
 public:
  // This helper is attached to a DownloadItem to manage the logging of the
  // Android download protection outcome histogram.
  // TODO(chlily): This should probably live in a separate file and not be a
  // nested class of the delegate.
  class MetricsData : public base::SupportsUserData::Data {
   public:
    // Describes the possible outcomes of a download that might be eligible for
    // download protection.
    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // LINT.IfChange(DownloadProtectionOutcome)
    enum class DownloadProtectionOutcome {
      // Catch-all bucket for other reasons why a ClientDownloadRequest was not
      // sent.
      kClientDownloadRequestNotSent = 0,
      // File passed all the checks and will trigger a ClientDownloadRequest.
      kClientDownloadRequestSent = 1,
      // Did not send ClientDownloadRequest. Android download protection is not
      // active.
      kDownloadProtectionDisabled = 2,
      // Did not send ClientDownloadRequest. File has no known display name.
      kNoDisplayName = 3,
      // Did not send ClientDownloadRequest. The display name from the
      // DownloadItem was not a supported file type.
      kDownloadItemDisplayNameNotSupportedType = 4,
      // Did not send ClientDownloadRequest. The display name obtained from
      // looking up the content-URI was not a supported file type.
      kJavaDisplayNameNotSupportedType = 5,
      // Did not send ClientDownloadRequest. The following mirror
      // DownloadCheckResultReasons that can be returned from
      // CheckClientDownloadRequest::IsSupportedDownload().
      kEmptyUrlChain = 6,
      kInvalidUrl = 7,
      kUnsupportedUrlScheme = 8,
      kRemoteFile = 9,
      kLocalFile = 10,
      // Did not send ClientDownloadRequest. The download passed all the checks
      // but was excluded due to sampling.
      kNotSampled = 11,
      // Did not send ClientDownloadRequest. The service is misconfigured.
      kMisconfigured = 12,

      kMaxValue = kMisconfigured,
    };
    // LINT.ThenChange(//tools/metrics/histograms/metadata/sb_client/enums.xml:SBClientDownloadAndroidDownloadProtectionOutcome)

    // The histogram is logged in the dtor if it has not yet been logged.
    ~MetricsData() override;

    static MetricsData* GetOrCreate(download::DownloadItem* item);

    // Creates or gets the MetricsData object for the item, then overwrites the
    // currently recorded outcome with the specified value.
    static void SetOutcome(download::DownloadItem* item,
                           DownloadProtectionOutcome outcome);

    // Logs the currently recorded outcome for the item to an UMA histogram.
    // This may only be called once per instance.
    void LogToHistogram();

   private:
    MetricsData();

    DownloadProtectionOutcome outcome_ =
        DownloadProtectionOutcome::kClientDownloadRequestNotSent;
    bool did_log_outcome_ = false;
  };

  DownloadProtectionDelegateAndroid();
  ~DownloadProtectionDelegateAndroid() override;

  // DownloadProtectionDelegate:
  bool ShouldCheckDownloadUrl(download::DownloadItem* item) const override;
  bool ShouldCheckClientDownload(download::DownloadItem* item) const override;
  bool IsSupportedDownload(download::DownloadItem& item,
                           const base::FilePath& target_path) const override;
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
