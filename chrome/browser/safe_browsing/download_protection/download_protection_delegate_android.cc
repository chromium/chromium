// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_delegate_android.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/android/download_protection_metrics_data.h"
#include "chrome/browser/safe_browsing/android/safe_browsing_referring_app_bridge_android.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "components/download/public/common/download_item.h"
#include "components/google/core/common/google_util.h"
#include "components/safe_browsing/core/browser/referring_app_info.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/version_info/android/channel_getter.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "google_apis/common/api_key_request_util.h"
#include "google_apis/google_api_keys.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/url_constants.h"

namespace safe_browsing {
namespace {

using Outcome = DownloadProtectionMetricsData::AndroidDownloadProtectionOutcome;

// Default URL for download check server.
const char kDownloadRequestDefaultUrl[] =
    "https://androidchromeprotect.pa.googleapis.com/v1/download";

// File suffix for APKs.
const base::FilePath::CharType kApkSuffix[] = FILE_PATH_LITERAL(".apk");

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
  enabled = enabled && profile && profile->GetPrefs();

  // Android download protection should only ever be enabled if Safe Browsing is
  // enabled.
  enabled = enabled && IsSafeBrowsingEnabled(*profile->GetPrefs());

  // In telemetry-only mode, APK download checks should only be active for
  // Enhanced Protection users.
  enabled = enabled && (!kMaliciousApkDownloadCheckTelemetryOnly.Get() ||
                        IsEnhancedProtectionEnabled(*profile->GetPrefs()));

  if (!enabled) {
    DownloadProtectionMetricsData::SetOutcome(
        item, Outcome::kDownloadProtectionDisabled);
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

Outcome ConvertDownloadCheckResultReason(DownloadCheckResultReason reason) {
  switch (reason) {
    case DownloadCheckResultReason::REASON_EMPTY_URL_CHAIN:
      return Outcome::kEmptyUrlChain;
    case DownloadCheckResultReason::REASON_INVALID_URL:
      return Outcome::kInvalidUrl;
    case DownloadCheckResultReason::REASON_UNSUPPORTED_URL_SCHEME:
      return Outcome::kUnsupportedUrlScheme;
    case DownloadCheckResultReason::REASON_REMOTE_FILE:
      return Outcome::kRemoteFile;
    case DownloadCheckResultReason::REASON_LOCAL_FILE:
      return Outcome::kLocalFile;
    default:
      NOTREACHED();
  }
}

void LogGetReferringAppInfoResult(internal::GetReferringAppInfoResult result) {
  base::UmaHistogramEnumeration(
      "SBClientDownload.Android.GetReferringAppInfo.Result", result);
}

}  // namespace

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
    DownloadProtectionMetricsData::SetOutcome(item, Outcome::kMisconfigured);
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

  DownloadCheckResultReason reason = REASON_MAX;
  if (!CheckClientDownloadRequest::IsSupportedDownload(item, file_name,
                                                       &reason)) {
    DownloadProtectionMetricsData::SetOutcome(
        &item, ConvertDownloadCheckResultReason(reason));
    return false;
  }

  // For Android download protection, only check APK files (as defined by having
  // a filename ending in a ".apk" extension).
  if (!file_name.MatchesExtension(kApkSuffix)) {
    DownloadProtectionMetricsData::SetOutcome(
        &item, Outcome::kDownloadNotSupportedType);
    return false;
  }

  bool should_sample = should_sample_override_.value_or(ShouldSample());
  if (!should_sample) {
    DownloadProtectionMetricsData::SetOutcome(&item, Outcome::kNotSampled);
  }
  should_sample_override_ = std::nullopt;
  return should_sample;
}

void DownloadProtectionDelegateAndroid::PreSerializeRequest(
    const download::DownloadItem* item,
    safe_browsing::ClientDownloadRequest& request_proto) {
  if (!item) {
    return;
  }

  // Populate the ReferringAppInfo in the ClientDownloadRequest.
  // Note: The web_contents will be null if the original download page has
  // been navigated away from.
  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(item);
  if (!web_contents) {
    LogGetReferringAppInfoResult(
        internal::GetReferringAppInfoResult::kNotAttempted);
    return;
  }
  internal::ReferringAppInfo info =
      GetReferringAppInfo(web_contents, /*get_webapk_info=*/true);
  LogGetReferringAppInfoResult(internal::ReferringAppInfoToResult(info));
  if (!info.has_referring_app() && !info.has_referring_webapk()) {
    return;
  }
  *request_proto.mutable_referring_app_info() = GetReferringAppInfoProto(info);
}

void DownloadProtectionDelegateAndroid::FinalizeResourceRequest(
    network::ResourceRequest& resource_request) {
  google_apis::AddAPIKeyToRequest(
      resource_request,
      google_apis::GetAPIKey(version_info::android::GetChannel()));
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
            last_reviewed: "2025-03-10"
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
  // "Light" pings for a sample of unsupported files is disabled on Android.
  return 0.0;
}

void DownloadProtectionDelegateAndroid::SetNextShouldSampleForTesting(
    bool should_sample) {
  should_sample_override_ = should_sample;
}

}  // namespace safe_browsing
