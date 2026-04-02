// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_UTILS_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_UTILS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/binary_upload_service.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "components/enterprise/connectors/core/common.h"
#include "components/enterprise/connectors/core/content_analysis_info_base.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "url/gurl.h"

namespace enterprise_connectors {
class ContentAnalysisResponse;
}  // namespace enterprise_connectors

namespace safe_browsing {

// Maps the request's connector and reason to the corresponding
// DeepScanAccessPoint.
enterprise_connectors::DeepScanAccessPoint AccessPointFromRequest(
    enterprise_connectors::AnalysisConnector connector,
    enterprise_connectors::ContentAnalysisRequest::Reason reason);

// Helper function to make ContentAnalysisResponses for tests.
enterprise_connectors::ContentAnalysisResponse
SimpleContentAnalysisResponseForTesting(std::optional<bool> dlp_success,
                                        std::optional<bool> malware_success,
                                        bool has_custom_rule_message);

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

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_UTILS_H_
