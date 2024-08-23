// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"

#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "components/crash/core/common/crash_key.h"

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
    case BinaryUploadService::Result::INCOMPLETE_RESPONSE:
      return "SERVICE_UNAVAILABLE";
    case BinaryUploadService::Result::FILE_ENCRYPTED:
      return "FILE_PASSWORD_PROTECTED";
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

void AddCustomMessageRule(
    enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule&
        rule) {
  enterprise_connectors::ContentAnalysisResponse::Result::TriggeredRule::
      CustomRuleMessage custom_message;
  auto* custom_segments = custom_message.add_message_segments();
  custom_segments->set_text("Custom rule message");
  custom_segments->set_link("http://example.com");
  *rule.mutable_custom_rule_message() = custom_message;
}

}  // namespace

void MaybeReportDeepScanningVerdict(
    Profile* profile,
    const GURL& url,
    const GURL& tab_url,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& content_transfer_method,
    DeepScanAccessPoint access_point,
    const int64_t content_size,
    BinaryUploadService::Result result,
    const enterprise_connectors::ContentAnalysisResponse& response,
    EventResult event_result) {
  DCHECK(base::ranges::all_of(download_digest_sha256, base::IsHexDigit<char>));
  auto* router =
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile);
  if (!router)
    return;

  std::string unscanned_reason = MaybeGetUnscannedReason(result);
  if (!unscanned_reason.empty()) {
    router->OnUnscannedFileEvent(
        url, tab_url, source, destination, file_name, download_digest_sha256,
        mime_type, trigger, access_point, unscanned_reason,
        content_transfer_method, content_size, event_result);
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

      router->OnUnscannedFileEvent(
          url, tab_url, source, destination, file_name, download_digest_sha256,
          mime_type, trigger, access_point, std::move(unscanned_reason),
          content_transfer_method, content_size, event_result);
    } else if (response_result.triggered_rules_size() > 0) {
      router->OnAnalysisConnectorResult(
          url, tab_url, source, destination, file_name, download_digest_sha256,
          mime_type, trigger, response.request_token(), content_transfer_method,
          access_point, response_result, content_size, event_result);
    }
  }
}

void ReportAnalysisConnectorWarningBypass(
    Profile* profile,
    const GURL& url,
    const GURL& tab_url,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& content_transfer_method,
    DeepScanAccessPoint access_point,
    const int64_t content_size,
    const enterprise_connectors::ContentAnalysisResponse& response,
    std::optional<std::u16string> user_justification) {
  DCHECK(base::ranges::all_of(download_digest_sha256, base::IsHexDigit<char>));
  auto* router =
      extensions::SafeBrowsingPrivateEventRouterFactory::GetForProfile(profile);
  if (!router)
    return;

  for (const auto& result : response.results()) {
    // Only report results with triggered rules.
    if (result.triggered_rules().empty())
      continue;

    router->OnAnalysisConnectorWarningBypassed(
        url, tab_url, source, destination, file_name, download_digest_sha256,
        mime_type, trigger, response.request_token(), content_transfer_method,
        access_point, result, content_size, user_justification);
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
  NOTREACHED_IN_MIGRATION();
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
    case DeepScanAccessPoint::FILE_TRANSFER:
      return "FileTransfer";
  }
  NOTREACHED_IN_MIGRATION();
  return "";
}

void RecordDeepScanMetrics(
    bool is_cloud,
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

  RecordDeepScanMetrics(is_cloud, access_point, duration, total_bytes,
                        result_value, success);
}

void RecordDeepScanMetrics(bool is_cloud,
                           DeepScanAccessPoint access_point,
                           base::TimeDelta duration,
                           int64_t total_bytes,
                           const std::string& result,
                           bool success) {
  // Don't record metrics if the duration is unusable.
  if (duration.InMilliseconds() == 0)
    return;

  const char* prefix =
      is_cloud ? "SafeBrowsing.DeepScan." : "SafeBrowsing.LocalDeepScan.";

  std::string access_point_string = DeepScanAccessPointToString(access_point);
  if (success) {
    base::UmaHistogramCustomCounts(
        prefix + access_point_string + ".BytesPerSeconds",
        (1000 * total_bytes) / duration.InMilliseconds(),
        /*min=*/kMinBytesPerSecond,
        /*max=*/kMaxBytesPerSecond,
        /*buckets=*/50);
  }

  // The scanning timeout is 5 minutes, so the bucket maximum time is 30 minutes
  // in order to be lenient and avoid having lots of data in the overflow
  // bucket.
  base::UmaHistogramCustomTimes(
      prefix + access_point_string + "." + result + ".Duration", duration,
      base::Milliseconds(1), base::Minutes(30), 50);
  base::UmaHistogramCustomTimes(prefix + access_point_string + ".Duration",
                                duration, base::Milliseconds(1),
                                base::Minutes(30), 50);
}

enterprise_connectors::ContentAnalysisResponse
SimpleContentAnalysisResponseForTesting(std::optional<bool> dlp_success,
                                        std::optional<bool> malware_success,
                                        bool has_custom_rule_message) {
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
      if (has_custom_rule_message) {
        AddCustomMessageRule(*rule);
      }
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
      if (has_custom_rule_message) {
        AddCustomMessageRule(*rule);
      }
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
    case BinaryUploadService::Result::TOO_MANY_REQUESTS:
      return "TooManyRequests";
    case BinaryUploadService::Result::INCOMPLETE_RESPONSE:
      return "IncompleteResponse";
  }
}

void IncrementCrashKey(ScanningCrashKey key, int delta) {
  DCHECK_GE(delta, 0);
  ModifyKey(key, delta);
}

void DecrementCrashKey(ScanningCrashKey key, int delta) {
  DCHECK_GE(delta, 0);
  ModifyKey(key, -delta);
}

bool IsConsumerScanRequest(const BinaryUploadService::Request& request) {
  if (request.cloud_or_local_settings().is_local_analysis()) {
    return false;
  }

  for (const std::string& tag : request.content_analysis_request().tags()) {
    if (tag == "dlp") {
      return false;
    }
  }
  return request.device_token().empty();
}

}  // namespace safe_browsing
