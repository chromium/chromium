// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_UTILS_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_UTILS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "url/gurl.h"

class Profile;

namespace enterprise_connectors {
class ContentAnalysisResponse;
}  // namespace enterprise_connectors

namespace safe_browsing {

// Helper function to examine a ContentAnalysisResponse and report the
// appropriate events to the enterprise admin. |download_digest_sha256| must be
// encoded using base::HexEncode.  |event_result| indicates whether the user was
// ultimately allowed to access the text or file.
void MaybeReportDeepScanningVerdict(
    Profile* profile,
    const enterprise_connectors::ContentAnalysisInfo* content_analysis_info,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& content_transfer_method,
    const std::string& source_email,
    const int64_t content_size,
    const safe_browsing::ReferrerChain& referrer_chain,
    enterprise_connectors::ScanRequestUploadResult result,
    const enterprise_connectors::ContentAnalysisResponse& response,
    enterprise_connectors::EventResult event_result);

// Helper function to report the user bypassed a warning to the enterprise
// admin. This is split from MaybeReportDeepScanningVerdict since it happens
// after getting a response. |download_digest_sha256| must be encoded using
// base::HexEncode.
void ReportAnalysisConnectorWarningBypass(
    Profile* profile,
    const enterprise_connectors::ContentAnalysisInfo& content_analysis_info,
    const std::string& source,
    const std::string& destination,
    const std::string& file_name,
    const std::string& download_digest_sha256,
    const std::string& mime_type,
    const std::string& trigger,
    const std::string& content_transfer_method,
    const int64_t content_size,
    const safe_browsing::ReferrerChain& referrer_chain,
    const enterprise_connectors::ContentAnalysisResponse& response,
    std::optional<std::u16string> user_justification);

// Helper functions to record DeepScanning UMA metrics for the duration of the
// request split by its result and bytes/sec for successful requests.
void RecordDeepScanMetrics(
    bool is_cloud,
    enterprise_connectors::DeepScanAccessPoint access_point,
    base::TimeDelta duration,
    int64_t total_bytes,
    const enterprise_connectors::ScanRequestUploadResult& result,
    const enterprise_connectors::ContentAnalysisResponse& response);
void RecordDeepScanMetrics(
    bool is_cloud,
    enterprise_connectors::DeepScanAccessPoint access_point,
    base::TimeDelta duration,
    int64_t total_bytes,
    const std::string& result,
    bool success);

// Helper function to make ContentAnalysisResponses for tests.
enterprise_connectors::ContentAnalysisResponse
SimpleContentAnalysisResponseForTesting(std::optional<bool> dlp_success,
                                        std::optional<bool> malware_success,
                                        bool has_custom_rule_message);

// Helper function to convert a enterprise_connectors::ScanRequestUploadResult
// to a CamelCase string.
std::string BinaryUploadServiceResultToString(
    const enterprise_connectors::ScanRequestUploadResult& result,
    bool success);

// Helper enum and function to manipulate crash keys relevant to scanning.
// If a key would be set to 0, it is unset.
enum class ScanningCrashKey {
  PENDING_FILE_UPLOADS,
  PENDING_TEXT_UPLOADS,
  PENDING_FILE_DOWNLOADS,
  PENDING_PRINTS,
  TOTAL_FILE_UPLOADS,
  TOTAL_TEXT_UPLOADS,
  TOTAL_FILE_DOWNLOADS,
  TOTAL_PRINTS
};
void IncrementCrashKey(ScanningCrashKey key, int delta = 1);
void DecrementCrashKey(ScanningCrashKey key, int delta = 1);

// Returns true for consumer scans and not on enterprise scans.
bool IsConsumerScanRequest(
    const safe_browsing::BinaryUploadService::Request& request);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_UTILS_H_
