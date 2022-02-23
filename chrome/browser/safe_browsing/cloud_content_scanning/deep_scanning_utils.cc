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
#include "components/crash/core/common/crash_key.h"
#include "components/signin/public/identity_manager/identity_manager.h"

namespace safe_browsing {

namespace {

constexpr int kMinBytesPerSecond = 1;
constexpr int kMaxBytesPerSecond = 100 * 1024 * 1024;  // 100 MB/s

std::string MaybeGetUnscannedReason(BinaryUploadService::Result result) {
  switch (result) {
    case BinaryUploadService::Result::SUCCESS:
    case BinaryUploadService::Result::UNAUTHORIZED:
      // Don't report an unscanned file event on these results.
      return "";

    case BinaryUploadService::Result::FILE_TOO_LARGE:
      return "FILE_TOO_LARGE";
    case BinaryUploadService::Result::TOO_MANY_REQUESTS:
      return "TOO_MANY_REQUESTS";
    case BinaryUploadService::Result::TIMEOUT:
      return "TIMEOUT";
    case BinaryUploadService::Result::UNKNOWN:
    case BinaryUploadService::Result::UPLOAD_FAILURE:
    case BinaryUploadService::Result::FAILED_TO_GET_TOKEN:
      return "SERVICE_UNAVAILABLE";
    case BinaryUploadService::Result::FILE_ENCRYPTED:
      return "FILE_PASSWORD_PROTECTED";
    case BinaryUploadService::Result::DLP_SCAN_UNSUPPORTED_FILE_TYPE:
      return "DLP_SCAN_UNSUPPORTED_FILE_TYPE";
  }
}

crash_reporter::CrashKeyString<7>* GetScanCrashKey(ScanningCrashKey key) {
  static crash_reporter::CrashKeyString<7> pending_file_uploads(
      "pending-file-upload-scans");
  static crash_reporter::CrashKeyString<7> pending_text_uploads(
      "pending-text-upload-scans");
  static crash_reporter::CrashKeyString<7> pending_file_downloads(
      "pending-file-download-scans");
  static crash_reporter::CrashKeyString<7> pending_prints(
      "pending-print-scans");
  static crash_reporter::CrashKeyString<7> total_file_uploads(
      "total-file-upload-scans");
  static crash_reporter::CrashKeyString<7> total_text_uploads(
      "total-text-upload-scans");
  static crash_reporter::CrashKeyString<7> total_file_downloads(
      "total-file-download-scans");
  static crash_reporter::CrashKeyString<7> total_prints("total-print-scans");
  switch (key) {
    case ScanningCrashKey::PENDING_FILE_UPLOADS:
      return &pending_file_uploads;
    case ScanningCrashKey::PENDING_TEXT_UPLOADS:
      return &pending_text_uploads;
    case ScanningCrashKey::PENDING_FILE_DOWNLOADS:
      return &pending_file_downloads;
    case ScanningCrashKey::PENDING_PRINTS:
      return &pending_prints;
    case ScanningCrashKey::TOTAL_FILE_UPLOADS:
      return &total_file_uploads;
    case ScanningCrashKey::TOTAL_TEXT_UPLOADS:
      return &total_text_uploads;
    case ScanningCrashKey::TOTAL_FILE_DOWNLOADS:
      return &total_file_downloads;
    case ScanningCrashKey::TOTAL_PRINTS:
      return &total_prints;
  }
}

int* GetScanCrashKeyCount(ScanningCrashKey key) {
  static int pending_file_uploads = 0;
  static int pending_text_uploads = 0;
  static int pending_file_downloads = 0;
  static int pending_prints = 0;
  static int total_file_uploads = 0;
  static int total_text_uploads = 0;
  static int total_file_downloads = 0;
  static int total_prints = 0;
  switch (key) {
    case ScanningCrashKey::PENDING_FILE_UPLOADS:
      return &pending_file_uploads;
    case ScanningCrashKey::PENDING_TEXT_UPLOADS:
      return &pending_text_uploads;
    case ScanningCrashKey::PENDING_FILE_DOWNLOADS:
      return &pending_file_downloads;
    case ScanningCrashKey::PENDING_PRINTS:
      return &pending_prints;
    case ScanningCrashKey::TOTAL_FILE_UPLOADS:
      return &total_file_uploads;
    case ScanningCrashKey::TOTAL_TEXT_UPLOADS:
      return &total_text_uploads;
    case ScanningCrashKey::TOTAL_FILE_DOWNLOADS:
      return &total_file_downloads;
    case ScanningCrashKey::TOTAL_PRINTS:
      return &total_prints;
  }
}

void ModifyKey(ScanningCrashKey key, int delta) {
  int* key_value = GetScanCrashKeyCount(key);

  // Since the crash key string length is determined at compile time, ensure the
  // given number is restricted to 6 digits (char 7 is for null terminating the
  // string).
  int new_value = (*key_value) + delta;
  new_value = std::max(0, new_value);
  new_value = std::min(999999, new_value);

  *key_value = new_value;
  crash_reporter::CrashKeyString<7>* crash_key = GetScanCrashKey(key);
  DCHECK(crash_key);

  if (new_value == 0)
    crash_key->Clear();
  else
    crash_key->Set(base::NumberToString(new_value));
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

  for (const auto& response_result : response.results()) {
    if (response_result.status() !=
        enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS) {
      unscanned_reason = "UNSCANNED_REASON_UNKNOWN";
      if (response_result.tag() == "malware")
        unscanned_reason = "MALWARE_SCAN_FAILED";
      else if (response_result.tag() == "dlp")
        unscanned_reason = "DLP_SCAN_FAILED";

      router->OnUnscannedFileEvent(url, file_name, download_digest_sha256,
                                   mime_type, trigger, access_point,
                                   std::move(unscanned_reason), content_size,
                                   event_result);
    } else if (response_result.triggered_rules_size() > 0) {
      router->OnAnalysisConnectorResult(
          url, file_name, download_digest_sha256, mime_type, trigger,
          response.request_token(), access_point, response_result, content_size,
          event_result);
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
    const enterprise_connectors::ContentAnalysisResponse& response,
    absl::optional<std::u16string> user_justification) {
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
        response.request_token(), access_point, result, content_size,
        user_justification);
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
    case DeepScanAccessPoint::PRINT:
      return "Print";
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
  for (const auto& response_result : response.results()) {
    if (response_result.tag() == "dlp" &&
        response_result.status() !=
            enterprise_connectors::ContentAnalysisResponse::Result::SUCCESS) {
      dlp_verdict_success = false;
    }
    if (response_result.tag() == "malware" &&
        response_result.status() !=
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
                                duration, base::Milliseconds(1),
                                base::Minutes(30), 50);
  base::UmaHistogramCustomTimes(
      "SafeBrowsing.DeepScan." + access_point_string + ".Duration", duration,
      base::Milliseconds(1), base::Minutes(30), 50);
}

const std::array<const base::FilePath::CharType*, 26>& SupportedDlpFileTypes() {
  // Keep sorted for efficient access.
  static constexpr const std::array<const base::FilePath::CharType*, 26>
      kSupportedDLPFileTypes = {
          FILE_PATH_LITERAL(".7z"),   FILE_PATH_LITERAL(".bz2"),
          FILE_PATH_LITERAL(".bzip"), FILE_PATH_LITERAL(".cab"),
          FILE_PATH_LITERAL(".csv"),  FILE_PATH_LITERAL(".doc"),
          FILE_PATH_LITERAL(".docx"), FILE_PATH_LITERAL(".eps"),
          FILE_PATH_LITERAL(".gz"),   FILE_PATH_LITERAL(".gzip"),
          FILE_PATH_LITERAL(".htm"),  FILE_PATH_LITERAL(".html"),
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

  const auto& dlp_types = SupportedDlpFileTypes();
  return std::binary_search(dlp_types.begin(), dlp_types.end(), extension);
}

const std::array<const char*, 38>& SupportedDlpMimeTypes() {
  // Keep sorted for efficient access.
  static constexpr const std::array<const char*, 38> kSupportedDLPMimeTypes = {
      "application/gzip",
      "application/msexcel",
      "application/mspowerpoint",
      "application/msword",
      "application/octet-stream",
      "application/pdf",
      "application/postscript",
      "application/rtf",
      "application/vnd.google-apps.document.internal",
      "application/vnd.google-apps.spreadsheet.internal",
      "application/vnd.ms-cab-compressed",
      "application/vnd.ms-excel",
      "application/vnd.ms-excel.sheet.macroenabled.12",
      "application/vnd.ms-excel.template.macroenabled.12",
      "application/vnd.ms-powerpoint",
      "application/vnd.ms-powerpoint.presentation.macroenabled.12",
      "application/vnd.ms-word",
      "application/vnd.ms-word.document.12",
      "application/vnd.ms-word.document.macroenabled.12",
      "application/vnd.ms-word.template.macroenabled.12",
      "application/vnd.ms-xpsdocument",
      "application/vnd.msword",
      "application/vnd.oasis.opendocument.text",
      "application/"
      "vnd.openxmlformats-officedocument.presentationml.presentation",
      "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
      "application/vnd.openxmlformats-officedocument.spreadsheetml.template",
      "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
      "application/vnd.openxmlformats-officedocument.wordprocessingml.template",
      "application/vnd.rar",
      "application/vnd.wordperfect",
      "application/x-7z-compressed",
      "application/x-bzip",
      "application/x-bzip2",
      "application/x-gzip",
      "application/x-rar-compressed",
      "application/x-tar",
      "application/x-zip-compressed",
      "application/zip"};
  // TODO: Replace this DCHECK with a static assert once std::is_sorted is
  // constexpr in C++20.
  DCHECK(std::is_sorted(kSupportedDLPMimeTypes.begin(),
                        kSupportedDLPMimeTypes.end(),
                        [](const std::string& a, const std::string& b) {
                          return a.compare(b) < 0;
                        }));

  return kSupportedDLPMimeTypes;
}

// Returns true if the given mime type is supported for DLP scanning.
bool MimeTypeSupportedForDlp(const std::string& mime_type) {
  // All text mime type are considered scannable for DLP.
  if (mime_type.size() >= 5 && mime_type.substr(0, 5) == "text/")
    return true;

  const auto& mime_types = SupportedDlpMimeTypes();
  return std::binary_search(mime_types.begin(), mime_types.end(), mime_type);
}

enterprise_connectors::ContentAnalysisResponse
SimpleContentAnalysisResponseForTesting(absl::optional<bool> dlp_success,
                                        absl::optional<bool> malware_success) {
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

void IncrementCrashKey(ScanningCrashKey key, int delta) {
  DCHECK_GE(delta, 0);
  ModifyKey(key, delta);
}

void DecrementCrashKey(ScanningCrashKey key, int delta) {
  DCHECK_GE(delta, 0);
  ModifyKey(key, -delta);
}

}  // namespace safe_browsing
