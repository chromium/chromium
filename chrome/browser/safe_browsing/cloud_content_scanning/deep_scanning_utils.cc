// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"

#include <algorithm>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/reporting/reporting_event_router_factory.h"
#include "components/crash/core/common/crash_key.h"
#include "components/enterprise/connectors/core/reporting_constants.h"

namespace safe_browsing {

namespace {

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

enterprise_connectors::DeepScanAccessPoint AccessPointFromRequest(
    enterprise_connectors::AnalysisConnector connector,
    enterprise_connectors::ContentAnalysisRequest::Reason reason) {
  switch (connector) {
    case enterprise_connectors::FILE_DOWNLOADED:
      return enterprise_connectors::DeepScanAccessPoint::DOWNLOAD;
    case enterprise_connectors::FILE_ATTACHED:
      if (reason ==
          enterprise_connectors::ContentAnalysisRequest::DRAG_AND_DROP) {
        return enterprise_connectors::DeepScanAccessPoint::DRAG_AND_DROP;
      }
      if (reason ==
          enterprise_connectors::ContentAnalysisRequest::CLIPBOARD_PASTE) {
        return enterprise_connectors::DeepScanAccessPoint::PASTE;
      }
      return enterprise_connectors::DeepScanAccessPoint::UPLOAD;
    case enterprise_connectors::BULK_DATA_ENTRY:
      return enterprise_connectors::DeepScanAccessPoint::PASTE;
    case enterprise_connectors::PRINT:
      return enterprise_connectors::DeepScanAccessPoint::PRINT;
    case enterprise_connectors::FILE_TRANSFER:
      return enterprise_connectors::DeepScanAccessPoint::FILE_TRANSFER;
    case enterprise_connectors::ANALYSIS_CONNECTOR_UNSPECIFIED:
      return enterprise_connectors::DeepScanAccessPoint::UPLOAD;
  }
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

void IncrementCrashKey(ScanningCrashKey key, int delta) {
  DCHECK_GE(delta, 0);
  ModifyKey(key, delta);
}

void DecrementCrashKey(ScanningCrashKey key, int delta) {
  DCHECK_GE(delta, 0);
  ModifyKey(key, -delta);
}

}  // namespace safe_browsing
