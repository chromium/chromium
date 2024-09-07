// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_UTILS_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_UTILS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "url/gurl.h"

class Profile;

namespace enterprise_connectors {
class ContentAnalysisResponse;
}  // namespace enterprise_connectors

namespace safe_browsing {

// Access points used to record UMA metrics and specify which code location is
// initiating a deep scan. Any new caller of
// ContentAnalysisDelegate::CreateForWebContents should add an access point
// here instead of re-using an existing value. histograms.xml should also be
// updated by adding histograms with names
//   "SafeBrowsing.DeepScan.<access-point>.BytesPerSeconds"
//   "SafeBrowsing.DeepScan.<access-point>.Duration"
//   "SafeBrowsing.DeepScan.<access-point>.<result>.Duration"
// for the new access point and every possible result.
enum class DeepScanAccessPoint {
  // A deep scan was initiated from downloading 1+ file(s).
  DOWNLOAD,

  // A deep scan was initiated from uploading 1+ file(s) via a system dialog.
  UPLOAD,

  // A deep scan was initiated from drag-and-dropping text or 1+ file(s).
  DRAG_AND_DROP,

  // A deep scan was initiated from pasting text.
  PASTE,

  // A deep scan was initiated from printing a page.
  PRINT,

  // A deep scan was initiated from transferring 1+ file(s) within ChromeOS.
  FILE_TRANSFER,
};
std::string DeepScanAccessPointToString(DeepScanAccessPoint access_point);

// The resulting action that chrome performed in response to a scan request.
// This maps to the event result in the real-time reporting.
enum class EventResult {
  UNKNOWN,

  // The user was allowed to use the data without restriction.
  ALLOWED,

  // The user was allowed to use the data but was warned that it may violate
  // enterprise rules.
  WARNED,

  // The user was not allowed to use the data.
  BLOCKED,

  // The user has chosen to use the data even though it violated enterprise
  // rules.
  BYPASSED,
};

// Helper function to examine a ContentAnalysisResponse and report the
// appropriate events to the enterprise admin. |download_digest_sha256| must be
// encoded using base::HexEncode.  |event_result| indicates whether the user was
// ultimately allowed to access the text or file.
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
    EventResult event_result);

// Helper function to report the user bypassed a warning to the enterprise
// admin. This is split from MaybeReportDeepScanningVerdict since it happens
// after getting a response. |download_digest_sha256| must be encoded using
// base::HexEncode.
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
    std::optional<std::u16string> user_justification);

// Helper functions to record DeepScanning UMA metrics for the duration of the
// request split by its result and bytes/sec for successful requests.
void RecordDeepScanMetrics(
    bool is_cloud,
    DeepScanAccessPoint access_point,
    base::TimeDelta duration,
    int64_t total_bytes,
    const BinaryUploadService::Result& result,
    const enterprise_connectors::ContentAnalysisResponse& response);
void RecordDeepScanMetrics(bool is_cloud,
                           DeepScanAccessPoint access_point,
                           base::TimeDelta duration,
                           int64_t total_bytes,
                           const std::string& result,
                           bool success);

// Helper function to make ContentAnalysisResponses for tests.
enterprise_connectors::ContentAnalysisResponse
SimpleContentAnalysisResponseForTesting(std::optional<bool> dlp_success,
                                        std::optional<bool> malware_success,
                                        bool has_custom_rule_message);

// Helper function to convert a EventResult to a string that.  The format of
// string returned is processed by the sever.
std::string EventResultToString(EventResult result);

// Helper function to convert a BinaryUploadService::Result to a CamelCase
// string.
std::string BinaryUploadServiceResultToString(
    const BinaryUploadService::Result& result,
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
