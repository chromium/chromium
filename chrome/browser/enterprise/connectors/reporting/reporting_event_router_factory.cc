// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/reporting_event_router_factory.h"

#include "base/memory/singleton.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "components/enterprise/connectors/core/reporting_event_router.h"
#include "content/public/browser/browser_context.h"

namespace enterprise_connectors {

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
  return std::make_unique<ReportingEventRouter>(
      RealtimeReportingClientFactory::GetForProfile(context));
}

content::BrowserContext* ReportingEventRouterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}

}  // namespace enterprise_connectors
