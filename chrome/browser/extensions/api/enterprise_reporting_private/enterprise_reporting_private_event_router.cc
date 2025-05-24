// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/enterprise_reporting_private/enterprise_reporting_private_event_router.h"

#include <memory>

#include "base/check_is_test.h"
#include "chrome/common/extensions/api/enterprise_reporting_private.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/event_router_factory.h"

namespace extensions {

namespace {

api::enterprise_reporting_private::TriggeredRuleInfo GetTriggeredRuleInfo(
    const safe_browsing::MatchedUrlNavigationRule& rule) {
  api::enterprise_reporting_private::TriggeredRuleInfo rule_info;
  rule_info.rule_id = rule.rule_id();
  rule_info.rule_name = rule.rule_name();

  return rule_info;
}

}  // namespace

EnterpriseReportingPrivateEventRouter::EnterpriseReportingPrivateEventRouter(
    content::BrowserContext* context)
    : event_router_(EventRouter::Get(context)) {}

EnterpriseReportingPrivateEventRouter::
    ~EnterpriseReportingPrivateEventRouter() = default;

void EnterpriseReportingPrivateEventRouter::OnUrlFilteringVerdict(
    const GURL& url,
    const safe_browsing::RTLookupResponse& response) {
  if (!event_router_) {
    CHECK_IS_TEST();
    return;
  }

  std::vector<api::enterprise_reporting_private::DataMaskingRule> rules_list;
  for (const auto& threat_info : response.threat_info()) {
    const auto& data_masking_actions =
        threat_info.matched_url_navigation_rule().data_masking_actions();

    for (const auto& data_masking_action : data_masking_actions) {
      api::enterprise_reporting_private::DataMaskingRule rule;
      rule.level = data_masking_action.mask_type();
      rule.regex_pattern = data_masking_action.pattern();
      rule.triggered_rule_info =
          GetTriggeredRuleInfo(threat_info.matched_url_navigation_rule());
      rule.url = url.spec();

      rules_list.push_back(std::move(rule));
    }
  }

  if (rules_list.empty()) {
    return;
  }

  event_router_->BroadcastEvent(std::make_unique<Event>(
      events::ENTERPRISE_REPORTING_PRIVATE_ON_DATA_MASKING_RULES_TRIGGERED,
      api::enterprise_reporting_private::OnDataMaskingRulesTriggered::
          kEventName,
      api::enterprise_reporting_private::OnDataMaskingRulesTriggered::Create(
          std::move(rules_list))));
}

// static
EnterpriseReportingPrivateEventRouterFactory*
EnterpriseReportingPrivateEventRouterFactory::GetInstance() {
  static base::NoDestructor<EnterpriseReportingPrivateEventRouterFactory>
      instance;
  return instance.get();
}

// static
EnterpriseReportingPrivateEventRouter*
EnterpriseReportingPrivateEventRouterFactory::GetForProfile(
    content::BrowserContext* context) {
  return static_cast<EnterpriseReportingPrivateEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

EnterpriseReportingPrivateEventRouterFactory::
    EnterpriseReportingPrivateEventRouterFactory()
    : ProfileKeyedServiceFactory(
          "EnterpriseReportingPrivateEventRouter",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(EventRouterFactory::GetInstance());
}

EnterpriseReportingPrivateEventRouterFactory::
    ~EnterpriseReportingPrivateEventRouterFactory() = default;

std::unique_ptr<KeyedService> EnterpriseReportingPrivateEventRouterFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* profile) const {
  return std::make_unique<EnterpriseReportingPrivateEventRouter>(profile);
}

}  // namespace extensions
