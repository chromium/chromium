// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"

#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/safe_browsing/core/common/features.h"

namespace enterprise_connectors {

void ContentAnalysisInfo::InitializeRequest(
    safe_browsing::BinaryUploadService::Request* request,
    bool include_enterprise_only_fields) {
  if (include_enterprise_only_fields) {
    if (settings().cloud_or_local_settings.is_cloud_analysis()) {
      request->set_device_token(settings().cloud_or_local_settings.dm_token());
    }

    // Include tab page title, user action id, and count of requests per user
    // action in local content analysis requests.
    if (settings().cloud_or_local_settings.is_local_analysis()) {
      request->set_user_action_requests_count(user_action_requests_count());
      request->set_tab_title(tab_title());
      request->set_user_action_id(user_action_id());
    }

    if (settings().client_metadata) {
      request->set_client_metadata(*settings().client_metadata);
    }

    request->set_per_profile_request(settings().per_profile);

    if (reason() != ContentAnalysisRequest::UNKNOWN) {
      request->set_reason(reason());
    }
  }

  request->set_email(email());
  request->set_url(url());
  request->set_tab_url(tab_url());

  for (const auto& tag : settings().tags) {
    request->add_tag(tag.first);
  }

  if (base::FeatureList::IsEnabled(safe_browsing::kLocalIpAddressInEvents)) {
    for (const auto& ip_address :
         enterprise_connectors::GetLocalIpAddresses()) {
      request->add_local_ips(ip_address);
    }
  }

  request->set_blocking(settings().block_until_verdict !=
                        BlockUntilVerdict::kNoBlock);
}

}  // namespace enterprise_connectors
