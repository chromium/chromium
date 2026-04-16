// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_UTILS_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_UTILS_H_

#include <optional>
#include <string>

#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/connectors/core/cloud_content_scanning/common.h"
#include "components/enterprise/connectors/core/common.h"
#include "url/gurl.h"

namespace enterprise_connectors {
class ContentAnalysisResponse;
}  // namespace enterprise_connectors

namespace safe_browsing {

// Helper function to make ContentAnalysisResponses for tests.
enterprise_connectors::ContentAnalysisResponse
SimpleContentAnalysisResponseForTesting(std::optional<bool> dlp_success,
                                        std::optional<bool> malware_success,
                                        bool has_custom_rule_message);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_DEEP_SCANNING_UTILS_H_
