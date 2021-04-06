// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace safe_browsing {

namespace {

constexpr int kMinBytesPerSecond = 1;
constexpr int kMaxBytesPerSecond = 100 * 1024 * 1024;  // 100 MB/s

std::string MaybeGetUnscannedReason(BinaryUploadService::Result result) {
  std::string unscanned_reason;
  switch (result) {
    case BinaryUploadService::Result::SUCCESS:
    case BinaryUploadService::Result::UNAUTHORIZED:
      // Don't report an unscanned file event on these results.
      break;

    case BinaryUploadService::Result::FILE_TOO_LARGE:
      unscanned_reason = "FILE_TOO_LARGE";
      break;
    case BinaryUploadService::Result::TIMEOUT:
    case BinaryUploadService::Result::UNKNOWN:
    case BinaryUploadService::Result::UPLOAD_FAILURE:
    case BinaryUploadService::Result::FAILED_TO_GET_TOKEN:
    // TODO(crbug.com/1191060): Update this string when the event is supported.
    case BinaryUploadService::Result::TOO_MANY_REQUESTS:
      unscanned_reason = "SERVICE_UNAVAILABLE";
      break;
    case BinaryUploadService::Result::FILE_ENCRYPTED:
      unscanned_reason = "FILE_PASSWORD_PROTECTED";
      break;
    case BinaryUploadService::Result::DLP_SCAN_UNSUPPORTED_FILE_TYPE:
      unscanned_reason = "DLP_SCAN_UNSUPPORTED_FILE_TYPE";
  }

  return unscanned_reason;
}

}  // namespace

void MaybeReportDeepScanningVerdict(
    Profile* profile,
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    DeepScanAccessPoint access_point,
    const int64_t content_size,
    BinaryUploadService::Result result,
    const enterprise_connectors::ContentAnalysisResponse& response,
    EventResult event_result) {
  DCHECK(std::all_of(download_digest_sha256.begin(),
                     download_digest_sha256.end(), [](const char& c) {
                       return (c >= '0' && c <= '9') ||
                              (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
                     }));
  auto* router =
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile);
  if (!router)
    return;

  std::string unscanned_reason = MaybeGetUnscannedReason(result);
  if (!unscanned_reason.empty()) {
    router->OnUnscannedFileEvent(url, file_name, download_digest_sha256,
                                 mime_type, trigger, access_point,
                                 unscanned_reason, content_size, event_result);
  }

  if (result != BinaryUploadService::Result::SUCCESS)
    return;

  for (const auto& result : response.results()) {
    if (result.status() !=
        enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS) {
      std::string unscanned_reason = "UNSCANNED_REASON_UNKNOWN";
      if (result.tag() == "malware")
        unscanned_reason = "MALWARE_SCAN_FAILED";
      else if (result.tag() == "dlp")
        unscanned_reason = "DLP_SCAN_FAILED";

      router->OnUnscannedFileEvent(url, file_name, download_digest_sha256,
                                   mime_type, trigger, access_point,
                                   std::move(unscanned_reason), content_size,
                                   event_result);
    } else if (result.triggered_rules_size() > 0) {
      router->OnAnalysisConnectorResult(url, file_name, download_digest_sha256,
                                        mime_type, trigger, access_point,
                                        result, content_size, event_result);
    }
  }
}

void ReportAnalysisConnectorWarningBypass(
    Profile* profile,
    const GURL& url,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    DeepScanAccessPoint access_point,
    const int64_t content_size,
    const enterprise_connectors::ContentAnalysisResponse& response) {
  DCHECK(std::all_of(download_digest_sha256.begin(),
                     download_digest_sha256.end(), [](const char& c) {
                       return (c >= '0' && c <= '9') ||
                              (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
                     }));
  auto* router =
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile);
  if (!router)
    return;

  for (const auto& result : response.results()) {
    // Only report results with triggered rules.
    if (result.triggered_rules().empty())
      continue;

    router->OnAnalysisConnectorWarningBypassed(
        url, file_name, download_digest_sha256, mime_type, trigger,
        access_point, result, content_size);
  }
}

std::string EventResultToString(EventResult result) {
  switch (result) {
    case EventResult::UNKNOWN:
      return "EVENT_RESULT_UNKNOWN";
    case EventResult::ALLOWED:
      return "EVENT_RESULT_ALLOWED";
    case EventResult::WARNED:
      return "EVENT_RESULT_WARNED";
    case EventResult::BLOCKED:
      return "EVENT_RESULT_BLOCKED";
    case EventResult::BYPASSED:
      return "EVENT_RESULT_BYPASSED";
  }
  NOTREACHED();
  return "";
}

std::string DeepScanAccessPointToString(DeepScanAccessPoint access_point) {
  switch (access_point) {
    case DeepScanAccessPoint::DOWNLOAD:
      return "Download";
    case DeepScanAccessPoint::UPLOAD:
      return "Upload";
    case DeepScanAccessPoint::DRAG_AND_DROP:
      return "DragAndDrop";
    case DeepScanAccessPoint::PASTE:
      return "Paste";
  }
  NOTREACHED();
  return "";
}

void RecordDeepScanMetrics(
    DeepScanAccessPoint access_point,
    base::TimeDelta duration,
    int64_t total_bytes,
    const BinaryUploadService::Result& result,
    const enterprise_connectors::ContentAnalysisResponse& response) {
  // Don't record UMA metrics for this result.
  if (result == BinaryUploadService::Result::UNAUTHORIZED)
    return;
  bool dlp_verdict_success = true;
  bool malware_verdict_success = true;
  for (const auto& result : response.results()) {
    if (result.tag() == "dlp" &&
        result.status() !=
            enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS) {
      dlp_verdict_success = false;
    }
    if (result.tag() == "malware" &&
        result.status() !=
            enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS) {
      malware_verdict_success = false;
    }
  }

  bool success = dlp_verdict_success && malware_verdict_success;
  std::string result_value = BinaryUploadServiceResultToString(result, success);

  // Update |success| so non-SUCCESS results don't log the bytes/sec metric.
  success &= (result == BinaryUploadService::Result::SUCCESS);

  RecordDeepScanMetrics(access_point, duration, total_bytes, result_value,
                        success);
}

void RecordDeepScanMetrics(DeepScanAccessPoint access_point,
                           base::TimeDelta duration,
                           int64_t total_bytes,
                           const std::string& result,
                           bool success) {
  // Don't record metrics if the duration is unusable.
  if (duration.InMilliseconds() == 0)
    return;

  std::string access_point_string = DeepScanAccessPointToString(access_point);
  if (success) {
    base::UmaHistogramCustomCounts(
        "SafeBrowsing.DeepScan." + access_point_string + ".BytesPerSeconds",
        (1000 * total_bytes) / duration.InMilliseconds(),
        /*min=*/kMinBytesPerSecond,
        /*max=*/kMaxBytesPerSecond,
        /*buckets=*/50);
  }

  // The scanning timeout is 5 minutes, so the bucket maximum time is 30 minutes
  // in order to be lenient and avoid having lots of data in the overlow bucket.
  base::UmaHistogramCustomTimes("SafeBrowsing.DeepScan." + access_point_string +
                                    "." + result + ".Duration",
                                duration, base::TimeDelta::FromMilliseconds(1),
                                base::TimeDelta::FromMinutes(30), 50);
  base::UmaHistogramCustomTimes(
      "SafeBrowsing.DeepScan." + access_point_string + ".Duration", duration,
      base::TimeDelta::FromMilliseconds(1), base::TimeDelta::FromMinutes(30),
      50);
}

std::array<const base::FilePath::CharType*, 24> SupportedDlpFileTypes() {
  // Keep sorted for efficient access.
  static constexpr const std::array<const base::FilePath::CharType*, 24>
      kSupportedDLPFileTypes = {
          FILE_PATH_LITERAL(".7z"),   FILE_PATH_LITERAL(".bz2"),
          FILE_PATH_LITERAL(".bzip"), FILE_PATH_LITERAL(".cab"),
          FILE_PATH_LITERAL(".csv"),  FILE_PATH_LITERAL(".doc"),
          FILE_PATH_LITERAL(".docx"), FILE_PATH_LITERAL(".eps"),
          FILE_PATH_LITERAL(".gz"),   FILE_PATH_LITERAL(".gzip"),
          FILE_PATH_LITERAL(".odt"),  FILE_PATH_LITERAL(".pdf"),
          FILE_PATH_LITERAL(".ppt"),  FILE_PATH_LITERAL(".pptx"),
          FILE_PATH_LITERAL(".ps"),   FILE_PATH_LITERAL(".rar"),
          FILE_PATH_LITERAL(".rtf"),  FILE_PATH_LITERAL(".tar"),
          FILE_PATH_LITERAL(".txt"),  FILE_PATH_LITERAL(".wpd"),
          FILE_PATH_LITERAL(".xls"),  FILE_PATH_LITERAL(".xlsx"),
          FILE_PATH_LITERAL(".xps"),  FILE_PATH_LITERAL(".zip")};
  // TODO: Replace this DCHECK with a static assert once std::is_sorted is
  // constexpr in C++20.
  DCHECK(std::is_sorted(
      kSupportedDLPFileTypes.begin(), kSupportedDLPFileTypes.end(),
      [](const base::FilePath::StringType& a,
         const base::FilePath::StringType& b) { return a.compare(b) < 0; }));

  return kSupportedDLPFileTypes;
}

bool FileTypeSupportedForDlp(const base::FilePath& path) {
  // Accept any file type in the supported list for DLP scans.
  base::FilePath::StringType extension(path.FinalExtension());
  std::transform(extension.begin(), extension.end(), extension.begin(),
                 tolower);

  auto dlp_types = SupportedDlpFileTypes();
  return std::binary_search(dlp_types.begin(), dlp_types.end(), extension);
}

enterprise_connectors::ContentAnalysisResponse
SimpleContentAnalysisResponseForTesting(base::Optional<bool> dlp_success,
                                        base::Optional<bool> malware_success) {
  enterprise_connectors::ContentAnalysisResponse response;

  if (dlp_success.has_value()) {
    auto* result = response.add_results();
    result->set_tag("dlp");
    result->set_status(
        enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
    if (!dlp_success.value()) {
      auto* rule = result->add_triggered_rules();
      rule->set_rule_name("dlp");
      rule->set_action(enterprise_connectors::TriggeredRule::BLOCK);
    }
  }

  if (malware_success.has_value()) {
    auto* result = response.add_results();
    result->set_tag("malware");
    result->set_status(
        enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS);
    if (!malware_success.value()) {
      auto* rule = result->add_triggered_rules();
      rule->set_rule_name("malware");
      rule->set_action(enterprise_connectors::TriggeredRule::BLOCK);
    }
  }

  return response;
}

std::string BinaryUploadServiceResultToString(
    const BinaryUploadService::Result& result,
    bool success) {
  switch (result) {
    case BinaryUploadService::Result::SUCCESS:
      if (success)
        return "Success";
      else
        return "FailedToGetVerdict";
    case BinaryUploadService::Result::UPLOAD_FAILURE:
      return "UploadFailure";
    case BinaryUploadService::Result::TIMEOUT:
      return "Timeout";
    case BinaryUploadService::Result::FILE_TOO_LARGE:
      return "FileTooLarge";
    case BinaryUploadService::Result::FAILED_TO_GET_TOKEN:
      return "FailedToGetToken";
    case BinaryUploadService::Result::UNKNOWN:
      return "Unknown";
    case BinaryUploadService::Result::UNAUTHORIZED:
      return "";
    case BinaryUploadService::Result::FILE_ENCRYPTED:
      return "FileEncrypted";
    case BinaryUploadService::Result::DLP_SCAN_UNSUPPORTED_FILE_TYPE:
      return "DlpScanUnsupportedFileType";
    case BinaryUploadService::Result::TOO_MANY_REQUESTS:
      return "TooManyRequests";
  }
}

std::string GetProfileEmail(Profile* profile) {
  return profile
             ? GetProfileEmail(IdentityManagerFactory::GetForProfile(profile))
             : std::string();
}

std::string GetProfileEmail(signin::IdentityManager* identity_manager) {
  // If the profile is not signed in, GetPrimaryAccountInfo() returns an
  // empty account info.
  return identity_manager
             ? identity_manager
                   ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                   .email
             : std::string();
}

}  // namespace safe_browsing
