// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"

#include <algorithm>

#include "base/strings/string_util.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_info.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/reporting/reporting_event_router_factory.h"
#include "components/enterprise/connectors/core/reporting_constants.h"

namespace safe_browsing {

namespace {

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

}  // namespace safe_browsing
