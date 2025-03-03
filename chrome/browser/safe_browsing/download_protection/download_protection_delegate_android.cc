// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_delegate_android.h"

#include <algorithm>

#include "base/android/content_uri_utils.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "chrome/common/safe_browsing/download_type_util.h"
#include "components/download/public/common/download_item.h"
#include "components/google/core/common/google_util.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "url/url_constants.h"

namespace safe_browsing {
namespace {

using MetricsData = DownloadProtectionDelegateAndroid::MetricsData;
using DownloadProtectionOutcome =
    DownloadProtectionDelegateAndroid::MetricsData::DownloadProtectionOutcome;

const char kDownloadRequestDefaultUrl[] =
    "https://androidchromeprotect.pa.googleapis.com/v1/download";

bool IsDownloadRequestUrlValid(const GURL& url) {
  return url.is_valid() && url.SchemeIs(url::kHttpsScheme) &&
         google_util::IsGoogleAssociatedDomainUrl(url);
}

GURL ConstructDownloadRequestUrl() {
  std::string url_override = kMaliciousApkDownloadCheckServiceUrlOverride.Get();
  GURL url(url_override);
  if (!IsDownloadRequestUrlValid(url)) {
    url = GURL(kDownloadRequestDefaultUrl);
  }
  return url;
}

// Determines whether Android download protection should be active.
// Also sets the metrics outcome if the result is disabled.
bool IsAndroidDownloadProtectionEnabledForDownloadProfile(
    download::DownloadItem* item) {
  CHECK(item);
  bool enabled = base::FeatureList::IsEnabled(kMaliciousApkDownloadCheck);
  Profile* profile = Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item));
  if (enabled && (!profile || !profile->GetPrefs())) {
    enabled = false;
  }
  // In telemetry-only mode, APK download checks should only be active for
  // Enhanced Protection users.
  if (enabled && kMaliciousApkDownloadCheckTelemetryOnly.Get() &&
      !IsEnhancedProtectionEnabled(*profile->GetPrefs())) {
    enabled = false;
  }
  // Android download protection should never be enabled if Safe Browsing is
  // off.
  if (enabled && !IsSafeBrowsingEnabled(*profile->GetPrefs())) {
    enabled = false;
  }
  if (!enabled) {
    MetricsData::SetOutcome(
        item, DownloadProtectionOutcome::kDownloadProtectionDisabled);
  }
  return enabled;
}

// Implements random sampling of a percentage of eligible downloads.
bool ShouldSample() {
  int sample_percentage = kMaliciousApkDownloadCheckSamplePercentage.Get();
  // If sample_percentage param is misconfigured, don't apply sampling.
  if (sample_percentage < 0 || sample_percentage > 100) {
    sample_percentage = 100;
  }
  // This ensures that in telemetry-only mode, we sample at most 10% of
  // eligible downloads.
  if (kMaliciousApkDownloadCheckTelemetryOnly.Get()) {
    sample_percentage = std::min(sample_percentage, 10);
  }
  // Avoid the syscall if possible.
  if (sample_percentage >= 100) {
    CHECK_EQ(sample_percentage, 100);
    return true;
  }
  return base::RandDouble() * 100 < sample_percentage;
}

DownloadProtectionDelegateAndroid::MetricsData::DownloadProtectionOutcome
ConvertDownloadCheckResultReason(DownloadCheckResultReason reason) {
  switch (reason) {
    case DownloadCheckResultReason::REASON_EMPTY_URL_CHAIN:
      return DownloadProtectionDelegateAndroid::MetricsData::
          DownloadProtectionOutcome::kEmptyUrlChain;
    case DownloadCheckResultReason::REASON_INVALID_URL:
      return DownloadProtectionDelegateAndroid::MetricsData::
          DownloadProtectionOutcome::kInvalidUrl;
    case DownloadCheckResultReason::REASON_UNSUPPORTED_URL_SCHEME:
      return DownloadProtectionDelegateAndroid::MetricsData::
          DownloadProtectionOutcome::kUnsupportedUrlScheme;
    case DownloadCheckResultReason::REASON_REMOTE_FILE:
      return DownloadProtectionDelegateAndroid::MetricsData::
          DownloadProtectionOutcome::kRemoteFile;
    case DownloadCheckResultReason::REASON_LOCAL_FILE:
      return DownloadProtectionDelegateAndroid::MetricsData::
          DownloadProtectionOutcome::kLocalFile;
    default:
      NOTREACHED();
  }
}

}  // namespace

DownloadProtectionDelegateAndroid::MetricsData::MetricsData() = default;

DownloadProtectionDelegateAndroid::MetricsData::~MetricsData() {
  LogToHistogram();
}

const void* const kAndroidDownloadProtectionMetricsDataKey =
    &kAndroidDownloadProtectionMetricsDataKey;

// static
DownloadProtectionDelegateAndroid::MetricsData*
DownloadProtectionDelegateAndroid::MetricsData::GetOrCreate(
    download::DownloadItem* item) {
  CHECK(item);
  MetricsData* data = static_cast<MetricsData*>(
      item->GetUserData(kAndroidDownloadProtectionMetricsDataKey));
  if (!data) {
    data = new MetricsData();
    item->SetUserData(kAndroidDownloadProtectionMetricsDataKey,
                      base::WrapUnique(data));
  }
  CHECK(data);
  return data;
}

// static
void DownloadProtectionDelegateAndroid::MetricsData::SetOutcome(
    download::DownloadItem* item,
    DownloadProtectionOutcome outcome) {
  CHECK(item);
  MetricsData* data = GetOrCreate(item);
  data->outcome_ = outcome;
}

void DownloadProtectionDelegateAndroid::MetricsData::LogToHistogram() {
  if (did_log_outcome_) {
    return;
  }
  base::UmaHistogramEnumeration(
      "SBClientDownload.Android.DownloadProtectionOutcome", outcome_);
  did_log_outcome_ = true;
}

DownloadProtectionDelegateAndroid::DownloadProtectionDelegateAndroid()
    : download_request_url_(ConstructDownloadRequestUrl()) {}

DownloadProtectionDelegateAndroid::~DownloadProtectionDelegateAndroid() =
    default;

bool DownloadProtectionDelegateAndroid::ShouldCheckDownloadUrl(
    download::DownloadItem* item) const {
  return IsAndroidDownloadProtectionEnabledForDownloadProfile(item);
}

bool DownloadProtectionDelegateAndroid::ShouldCheckClientDownload(
    download::DownloadItem* item) const {
  bool is_enabled = IsAndroidDownloadProtectionEnabledForDownloadProfile(item);
  if (is_enabled && !IsDownloadRequestUrlValid(download_request_url_)) {
    // This value overwrites kDownloadProtectionDisabled that was set above.
    MetricsData::SetOutcome(item, DownloadProtectionOutcome::kMisconfigured);
    return false;
  }
  return is_enabled;
}

bool DownloadProtectionDelegateAndroid::IsSupportedDownload(
    download::DownloadItem& item,
    const base::FilePath& target_path) const {
  // On Android, the target path is likely a content-URI. Therefore, use the
  // display name instead. This assumes the DownloadItem's display name has
  // already been populated by InProgressDownloadManager.
  base::FilePath file_name = item.GetFileNameToReportUser();
  // If for some reason there's no human-readable display name in the
  // DownloadItem, try harder to find one.
  bool looked_up_display_name = false;
  if (file_name.IsContentUri()) {
    std::u16string display_name;
    looked_up_display_name = true;
    if (base::MaybeGetFileDisplayName(file_name, &display_name)) {
      file_name = base::FilePath::FromUTF16Unsafe(display_name);
    } else {
      MetricsData::SetOutcome(&item, DownloadProtectionOutcome::kNoDisplayName);
      return false;
    }
  }

  DownloadCheckResultReason reason = REASON_MAX;
  if (!CheckClientDownloadRequest::IsSupportedDownload(item, file_name,
                                                       &reason)) {
    MetricsData::SetOutcome(&item, ConvertDownloadCheckResultReason(reason));
    return false;
  }

  // For Android download protection, only check APK files (as defined by having
  // a filename ending in a ".apk" extension).
  if (download_type_util::GetDownloadType(file_name) !=
      ClientDownloadRequest::ANDROID_APK) {
    MetricsData::SetOutcome(
        &item, looked_up_display_name
                   ? DownloadProtectionOutcome::kJavaDisplayNameNotSupportedType
                   : DownloadProtectionOutcome::
                         kDownloadItemDisplayNameNotSupportedType);
    return false;
  }

  bool should_sample = should_sample_override_.value_or(ShouldSample());
  if (!should_sample) {
    MetricsData::SetOutcome(&item, DownloadProtectionOutcome::kNotSampled);
  }
  should_sample_override_ = std::nullopt;
  return should_sample;
}

const GURL& DownloadProtectionDelegateAndroid::GetDownloadRequestUrl() const {
  return download_request_url_;
}

net::NetworkTrafficAnnotationTag DownloadProtectionDelegateAndroid::
    CompleteClientDownloadRequestTrafficAnnotation(
        const net::PartialNetworkTrafficAnnotationTag&
            partial_traffic_annotation) const {
  // TODO(crbug.com/397407934): Update the `data` and `user_data` fields after
  // additional Android-specific data is added to ClientDownloadRequest.
  return net::BranchedCompleteNetworkTrafficAnnotation(
      "client_download_request_android", "client_download_request_for_platform",
      partial_traffic_annotation, R"(
          semantics {
            description:
              "Chromium checks whether a given APK download is likely to be "
              "dangerous by sending this client download request to Google's "
              "Android Chrome protection server. The server will respond to "
              "this request by sending back a verdict, indicating if this "
              "download is safe or the danger type of this download (e.g. "
              "dangerous content, uncommon content, potentially harmful, etc)."
            trigger:
              "This request may be triggered when an eligible download is "
              "about to complete, for a random sample of eligible downloads "
              "at a sampling rate between 0% and 100% configured via "
              "fieldtrial. A download is eligible if the download URL is valid "
              "and its file extension matches '.apk'."
            data:
              "URL of the file to be downloaded, its referrer chain, digest "
              "and other features extracted from the downloaded file. Refer to "
              "ClientDownloadRequest message in https://cs.chromium.org/"
              "chromium/src/components/safe_browsing/csd.proto for all "
              "submitted features."
            user_data {
              type: SENSITIVE_URL
              type: WEB_CONTENT
            }
            last_reviewed: "2025-02-27"
          })");
}

float DownloadProtectionDelegateAndroid::GetAllowlistedDownloadSampleRate()
    const {
  // TODO(chlily): The allowlist is not implemented yet for Android download
  // protection.
  return 0.0;
}

float DownloadProtectionDelegateAndroid::GetUnsupportedFileSampleRate(
    const base::FilePath& filename) const {
  // Unsupported file sampling is disabled on Android. See ShouldSample() above
  // for a separate sampling mechanism (but those sampled pings are not
  // "sanitized").
  return 0.0;
}

void DownloadProtectionDelegateAndroid::SetNextShouldSampleForTesting(
    bool should_sample) {
  should_sample_override_ = should_sample;
}

}  // namespace safe_browsing
