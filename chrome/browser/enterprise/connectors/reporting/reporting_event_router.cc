// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/reporting_event_router.h"

#include "base/containers/contains.h"
#include "base/memory/singleton.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

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
