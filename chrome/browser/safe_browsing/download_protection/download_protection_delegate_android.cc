// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/download_protection_delegate_android.h"

#include <algorithm>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/rand_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/android/download_protection_metrics_data.h"
#include "chrome/browser/safe_browsing/android/safe_browsing_referring_app_bridge_android.h"
#include "chrome/browser/safe_browsing/download_protection/check_client_download_request.h"
#include "chrome/browser/safe_browsing/download_protection/check_file_system_access_write_request.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_util.h"
#include "components/download/public/common/download_item.h"
#include "components/google/core/common/google_util.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#include "components/safe_browsing/core/browser/referring_app_info.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/version_info/android/channel_getter.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_item_utils.h"
#include "google_apis/common/api_key_request_util.h"
#include "google_apis/google_api_keys.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/url_constants.h"

namespace safe_browsing {
namespace {

using Outcome = DownloadProtectionMetricsData::AndroidDownloadProtectionOutcome;

// Default URL for download check server.
const char kDownloadRequestDefaultUrl[] =
    "https://androidchromeprotect.pa.googleapis.com/v1/download";

// Content-Type HTTP header field for the request.
const char kProtobufContentType[] = "application/x-protobuf";

// We sample 1% of allowlisted downloads to still send out download pings if
// other conditions are met.
const double kAllowlistDownloadSampleRate = 0.01;

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
bool IsAndroidDownloadProtectionEnabled(Profile* profile) {
  if (!base::FeatureList::IsEnabled(kMaliciousApkDownloadCheck)) {
    return false;
  }

  if (!profile || !profile->GetPrefs()) {
    return false;
  }

  // Android download protection should only ever be enabled if Safe Browsing is
  // enabled.
  if (!IsSafeBrowsingEnabled(*profile->GetPrefs())) {
    return false;
  }

  // In telemetry-only mode, APK download checks should only be active for
  // Enhanced Protection users.
  if (kMaliciousApkDownloadCheckTelemetryOnly.Get() &&
      !IsEnhancedProtectionEnabled(*profile->GetPrefs())) {
    return false;
  }
  return true;
}

// Determines whether Android download protection should be active.
// Also sets the metrics outcome if the result is disabled.
bool IsAndroidDownloadProtectionEnabledForDownloadProfile(
    download::DownloadItem* item) {
  CHECK(item);
  bool enabled = IsAndroidDownloadProtectionEnabled(Profile::FromBrowserContext(
      content::DownloadItemUtils::GetBrowserContext(item)));
  if (!enabled) {
    DownloadProtectionMetricsData::SetOutcome(
        item, Outcome::kDownloadProtectionDisabled);
  }
  return enabled;
}

void LogGetReferringAppInfoResult(internal::GetReferringAppInfoResult result) {
  base::UmaHistogramEnumeration(
      "SBClientDownload.Android.GetReferringAppInfo.Result", result);
}

void PopulateReferringAppInfoInProto(internal::ReferringAppInfo info,
                                     ClientDownloadRequest* request_proto) {
  *request_proto->mutable_referring_app_info() = GetReferringAppInfoProto(info);
}

void MaybePopulateReferringAppInfo(const download::DownloadItem* item,
                                   CollectModificationCallback callback) {
  CHECK(item);
  // Note: The web_contents will be null if the original download page has
  // been navigated away from.
  content::WebContents* web_contents =
      content::DownloadItemUtils::GetWebContents(item);
  if (!web_contents) {
    LogGetReferringAppInfoResult(
        internal::GetReferringAppInfoResult::kNotAttempted);
    std::move(callback).Run(NoModificationToRequestProto());
    return;
  }
  internal::ReferringAppInfo info =
      GetReferringAppInfo(web_contents, /*get_webapk_info=*/true);
  LogGetReferringAppInfoResult(internal::ReferringAppInfoToResult(info));
  if (!info.has_referring_app() && !info.has_referring_webapk()) {
    std::move(callback).Run(NoModificationToRequestProto());
    return;
  }

  std::move(callback).Run(
      base::BindOnce(&PopulateReferringAppInfoInProto, std::move(info)));
}

void PopulateRateLimitingKeyInProto(const std::string& rate_limiting_key,
                                    ClientDownloadRequest* request_proto) {
  request_proto->set_rate_limiting_key(rate_limiting_key);
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

bool DownloadProtectionDelegateAndroid::MayCheckClientDownload(
    download::DownloadItem* item) const {
  bool is_enabled = IsAndroidDownloadProtectionEnabledForDownloadProfile(item);
  if (is_enabled && !IsDownloadRequestUrlValid(download_request_url_)) {
    DownloadProtectionMetricsData::SetOutcome(item, Outcome::kMisconfigured);
    return false;
  }
  if (!is_enabled) {
    return false;
  }

  MayCheckDownloadResult may_check_download_result =
      IsSupportedDownload(*item, item->GetFileNameToReportUser());
  return MayCheckItem(may_check_download_result, /*download_item=*/item);
}

bool DownloadProtectionDelegateAndroid::MayCheckFileSystemAccessWrite(
    content::FileSystemAccessWriteItem* item) const {
  Profile* profile = Profile::FromBrowserContext(item->browser_context);
  if (!IsAndroidDownloadProtectionEnabled(profile)) {
    return false;
  }
  if (!IsDownloadRequestUrlValid(download_request_url_)) {
    return false;
  }
  DownloadCheckResultReason ignored_reason = REASON_MAX;
  MayCheckDownloadResult may_check_download_result =
      CheckFileSystemAccessWriteRequest::IsSupportedDownload(
          item->target_file_path, &ignored_reason);
  return MayCheckItem(may_check_download_result);
}

MayCheckDownloadResult DownloadProtectionDelegateAndroid::IsSupportedDownload(
    download::DownloadItem& item,
    const base::FilePath& target_path) const {
  // On Android, the target path is likely a content-URI. Therefore, use the
  // display name instead. This assumes the DownloadItem's display name has
  // already been populated by InProgressDownloadManager.
  // TODO(chlily): The display name may not be populated properly at the point
  // when this is called.
  base::FilePath file_name = item.GetFileNameToReportUser();

  DownloadCheckResultReason reason = REASON_MAX;
  MayCheckDownloadResult may_check_download_result =
      CheckClientDownloadRequest::IsSupportedDownload(item, file_name, &reason);
  if (may_check_download_result != MayCheckDownloadResult::kMayCheckDownload) {
    DownloadProtectionMetricsData::SetOutcome(
        &item, DownloadProtectionMetricsData::ConvertDownloadCheckResultReason(
                   reason));
  }
  return may_check_download_result;
}

std::vector<PendingClientDownloadRequestModification>
DownloadProtectionDelegateAndroid::ProduceClientDownloadRequestModifications(
    const download::DownloadItem* item,
    Profile* profile) {
  std::vector<PendingClientDownloadRequestModification> modifications;

  // Populate referring_app_info.
  if (item) {
    modifications.emplace_back(
        base::BindOnce(&MaybePopulateReferringAppInfo, item));
  }

  // Populate rate_limiting_key.
  if (profile) {
    modifications.emplace_back(base::BindOnce(
        &DownloadProtectionDelegateAndroid::PopulateRateLimitingKey,
        weak_factory_.GetWeakPtr(), profile->UniqueId()));
  }

  return modifications;
}

void DownloadProtectionDelegateAndroid::FinalizeResourceRequest(
    network::ResourceRequest& resource_request) {
  resource_request.headers.SetHeader(net::HttpRequestHeaders::kContentType,
                                     kProtobufContentType);
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
  return kAllowlistDownloadSampleRate;
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

bool DownloadProtectionDelegateAndroid::ShouldSampleEligibleFile() const {
  // Use the value overridden for testing.
  if (should_sample_override_.has_value()) {
    bool should_sample = *should_sample_override_;
    should_sample_override_.reset();
    return should_sample;
  }

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

bool DownloadProtectionDelegateAndroid::MayCheckItem(
    MayCheckDownloadResult may_check_download_result,
    download::DownloadItem* item) const {
  switch (may_check_download_result) {
    case MayCheckDownloadResult::kMayNotCheckDownload:
      return false;

    case MayCheckDownloadResult::kMayCheckDownload: {
      // Apply random sampling only to eligible files (APK files).
      bool should_sample = ShouldSampleEligibleFile();
      if (item && !should_sample) {
        DownloadProtectionMetricsData::SetOutcome(item, Outcome::kNotSampled);
      }
      return should_sample;
    }

    case MayCheckDownloadResult::kMaySendSampledPingOnly:
      // "Light" sampled pings for unsupported filetypes are not supported on
      // Android. GetUnsupportedFileSampleRate() enforces that later, so return
      // true here to be consistent with the semantics of
      // MayCheckDownloadResult.
      return true;
  }
}

void DownloadProtectionDelegateAndroid::PopulateRateLimitingKey(
    const std::string& profile_id,
    CollectModificationCallback callback) {
  if (!rate_limiting_key_manager_) {
    base::OnceCallback<void(const std::string&)> on_got_safety_net_id =
        base::BindOnce(
            &DownloadProtectionDelegateAndroid::InitRateLimitingKeyManager,
            weak_factory_.GetWeakPtr());
    base::OnceClosure try_populate_again = base::BindOnce(
        &DownloadProtectionDelegateAndroid::PopulateRateLimitingKey,
        weak_factory_.GetWeakPtr(), profile_id, std::move(callback));
    // Kick off a lookup to get the safety_net_id and initialize the manager,
    // then try again.
    SafeBrowsingApiHandlerBridge::GetInstance().StartGetSafetyNetId(
        std::move(on_got_safety_net_id).Then(std::move(try_populate_again)));
    return;
  }

  CHECK(rate_limiting_key_manager_);
  std::move(callback).Run(base::BindOnce(
      &PopulateRateLimitingKeyInProto,
      rate_limiting_key_manager_->GetCurrentRateLimitingKey(profile_id)));
}

void DownloadProtectionDelegateAndroid::InitRateLimitingKeyManager(
    const std::string& safety_net_id) {
  if (!rate_limiting_key_manager_) {
    rate_limiting_key_manager_.emplace(safety_net_id);
  }
}

}  // namespace safe_browsing
