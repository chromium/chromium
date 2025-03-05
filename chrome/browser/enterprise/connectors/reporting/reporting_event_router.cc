// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/reporting_event_router.h"

#include "base/containers/contains.h"
#include "base/memory/singleton.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/enterprise/connectors/core/reporting_utils.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/url_matcher/url_matcher.h"

namespace enterprise_connectors {

namespace {

bool IsEventInReportingSettings(const std::string& event,
                                std::optional<ReportingSettings> settings) {
  if (!settings.has_value()) {
    return false;
  }
  if (base::Contains(kAllReportingEnabledEvents, event)) {
    return settings->enabled_event_names.count(event) > 0;
  }
  if (base::Contains(kAllReportingOptInEvents, event)) {
    return settings->enabled_opt_in_events.count(event) > 0;
  }
  return false;
}

}  // namespace

ReportingEventRouter::ReportingEventRouter(content::BrowserContext* context)
    : context_(context) {
  reporting_client_ = RealtimeReportingClientFactory::GetForProfile(context);
}

ReportingEventRouter::~ReportingEventRouter() = default;

bool ReportingEventRouter::IsEventEnabled(const std::string& event) {
  if (!reporting_client_) {
    return false;
  }
  return IsEventInReportingSettings(event,
                                    reporting_client_->GetReportingSettings());
}

void ReportingEventRouter::OnLoginEvent(
    const GURL& url,
    bool is_federated,
    const url::SchemeHostPort& federated_origin,
    const std::u16string& username) {
  std::optional<enterprise_connectors::ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value()) {
    return;
  }

  std::unique_ptr<url_matcher::URLMatcher> matcher =
      CreateURLMatcherForOptInEvent(settings.value(),
                                    enterprise_connectors::kKeyLoginEvent);
  if (!IsUrlMatched(matcher.get(), url)) {
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyUrl, url.spec());
  event.Set(kKeyIsFederated, is_federated);
  if (is_federated) {
    event.Set(kKeyFederatedOrigin, federated_origin.Serialize());
  }
  event.Set(kKeyLoginUserName, MaskUsername(username));

  reporting_client_->ReportRealtimeEvent(
      kKeyLoginEvent, std::move(settings.value()), std::move(event));
}

void ReportingEventRouter::OnPasswordBreach(
    const std::string& trigger,
    const std::vector<std::pair<GURL, std::u16string>>& identities) {
  std::optional<ReportingSettings> settings =
      reporting_client_->GetReportingSettings();
  if (!settings.has_value()) {
    return;
  }

  std::unique_ptr<url_matcher::URLMatcher> matcher =
      CreateURLMatcherForOptInEvent(settings.value(), kKeyPasswordBreachEvent);
  if (!matcher) {
    return;
  }

  base::Value::List identities_list;
  for (const std::pair<GURL, std::u16string>& i : identities) {
    if (!IsUrlMatched(matcher.get(), i.first)) {
      continue;
    }

    base::Value::Dict identity;
    identity.Set(kKeyPasswordBreachIdentitiesUrl, i.first.spec());
    identity.Set(kKeyPasswordBreachIdentitiesUsername, MaskUsername(i.second));
    identities_list.Append(std::move(identity));
  }

  if (identities_list.empty()) {
    // Don't send an empty event if none of the breached identities matched a
    // pattern in the URL filters.
    return;
  }

  base::Value::Dict event;
  event.Set(kKeyTrigger, trigger);
  event.Set(kKeyPasswordBreachIdentities, std::move(identities_list));

  reporting_client_->ReportRealtimeEvent(
      kKeyPasswordBreachEvent, std::move(settings.value()), std::move(event));
}

// ---------------------------------------
// ReportingEventRouterFactory implementation
// ---------------------------------------

// static
ReportingEventRouterFactory* ReportingEventRouterFactory::GetInstance() {
  return base::Singleton<ReportingEventRouterFactory>::get();
}

ReportingEventRouter* ReportingEventRouterFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ReportingEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

ReportingEventRouterFactory::ReportingEventRouterFactory()
    : BrowserContextKeyedServiceFactory(
          "ReportingEventRouter",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(RealtimeReportingClientFactory::GetInstance());
}

ReportingEventRouterFactory::~ReportingEventRouterFactory() = default;

std::unique_ptr<KeyedService>
ReportingEventRouterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ReportingEventRouter>(context);
}

content::BrowserContext* ReportingEventRouterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace enterprise_connectors
