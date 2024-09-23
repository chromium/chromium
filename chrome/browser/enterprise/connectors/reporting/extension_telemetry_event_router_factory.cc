// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/reporting/extension_telemetry_event_router_factory.h"

#include "chrome/browser/enterprise/connectors/reporting/extension_telemetry_event_router.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client_factory.h"
#include "chrome/browser/profiles/profile.h"

namespace enterprise_connectors {

// static
ExtensionTelemetryEventRouter*
ExtensionTelemetryEventRouterFactory::GetForProfile(
    content::BrowserContext* context) {
  return static_cast<ExtensionTelemetryEventRouter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionTelemetryEventRouterFactory*
ExtensionTelemetryEventRouterFactory::GetInstance() {
  static base::NoDestructor<ExtensionTelemetryEventRouterFactory> instance;
  return instance.get();
}

ExtensionTelemetryEventRouterFactory::ExtensionTelemetryEventRouterFactory()
    : ProfileKeyedServiceFactory(
          "ExtensionTelemetryEventRouter",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(RealtimeReportingClientFactory::GetInstance());
}

ExtensionTelemetryEventRouterFactory::~ExtensionTelemetryEventRouterFactory() =
    default;

std::unique_ptr<KeyedService>
ExtensionTelemetryEventRouterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ExtensionTelemetryEventRouter>(context);
}

bool ExtensionTelemetryEventRouterFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace enterprise_connectors
