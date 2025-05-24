// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_ANDROID_DOWNLOAD_PROTECTION_METRICS_DATA_H_
#define CHROME_BROWSER_SAFE_BROWSING_ANDROID_DOWNLOAD_PROTECTION_METRICS_DATA_H_

#include "base/supports_user_data.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"

namespace download {
class DownloadItem;
}

namespace safe_browsing {

// This helper is attached to a DownloadItem to manage the logging of the
// Android download protection outcome histogram.
class DownloadProtectionMetricsData : public base::SupportsUserData::Data {
 public:
  // Describes the possible outcomes of a download that might be eligible for
  // download protection.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(AndroidDownloadProtectionOutcome)
  enum class AndroidDownloadProtectionOutcome {
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
    kDownloadNotSupportedType = 4,
    // Did not send ClientDownloadRequest. The following mirror
    // DownloadCheckResultReasons that can be returned from
    // CheckClientDownloadRequest::IsSupportedDownload().
    kEmptyUrlChain = 5,
    kInvalidUrl = 6,
    kUnsupportedUrlScheme = 7,
    kRemoteFile = 8,
    kLocalFile = 9,
    // Did not send ClientDownloadRequest. The service is misconfigured.
    // Note: If the service is misconfigued but Android download protection is
    // not enabled, then kDownloadProtectionDisabled will be logged
    // preferentially.
    kMisconfigured = 10,
    // Did not send ClientDownloadRequest. The download passed all the checks
    // but was excluded due to sampling.
    kNotSampled = 11,

    kMaxValue = kNotSampled,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/sb_client/enums.xml:SBClientDownloadAndroidDownloadProtectionOutcome)

  // Converts corresponding enum values.
  static AndroidDownloadProtectionOutcome ConvertDownloadCheckResultReason(
      DownloadCheckResultReason reason);

  // The histogram is logged in the dtor if it has not yet been logged.
  ~DownloadProtectionMetricsData() override;

  static DownloadProtectionMetricsData* GetOrCreate(
      download::DownloadItem* item);

  // Creates or gets the MetricsData object for the item, then overwrites the
  // currently recorded outcome with the specified value.
  static void SetOutcome(download::DownloadItem* item,
                         AndroidDownloadProtectionOutcome outcome);

  // Logs the currently recorded outcome for the item to an UMA histogram.
  // This may only be called once per instance.
  void LogToHistogram();

 private:
  DownloadProtectionMetricsData();

  AndroidDownloadProtectionOutcome outcome_ =
      AndroidDownloadProtectionOutcome::kClientDownloadRequestNotSent;
  bool did_log_outcome_ = false;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_ANDROID_DOWNLOAD_PROTECTION_METRICS_DATA_H_
