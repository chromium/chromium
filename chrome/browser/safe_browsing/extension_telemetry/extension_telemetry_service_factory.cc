// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/extension_telemetry_event_router_factory.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "chrome/browser/safe_browsing/network_context_service.h"
#include "chrome/browser/safe_browsing/network_context_service_factory.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"

namespace safe_browsing {

// static
ExtensionTelemetryService* ExtensionTelemetryServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<ExtensionTelemetryService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /* create= */ true));
}

// static
ExtensionTelemetryServiceFactory*
ExtensionTelemetryServiceFactory::GetInstance() {
  static base::NoDestructor<ExtensionTelemetryServiceFactory> instance;
  return instance.get();
}

ExtensionTelemetryServiceFactory::ExtensionTelemetryServiceFactory()
    : ProfileKeyedServiceFactory(
          // AshInternals should be 'kNone' since ExtensionTelemetryService
          // should not be initialized for internal profiles on ChromeOS Ash.
          "ExtensionTelemetryService",
          ProfileSelections::BuildForRegularProfile()) {
  DependsOn(NetworkContextServiceFactory::GetInstance());
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(extensions::ExtensionManagementFactory::GetInstance());
  DependsOn(extensions::ExtensionSystemFactory::GetInstance());
  DependsOn(enterprise_connectors::ConnectorsServiceFactory::GetInstance());
  DependsOn(enterprise_connectors::ExtensionTelemetryEventRouterFactory::
                GetInstance());
}

std::unique_ptr<KeyedService>
ExtensionTelemetryServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  NetworkContextService* network_service =
      NetworkContextServiceFactory::GetForBrowserContext(context);
  if (!network_service) {
    return nullptr;
  }
  return std::make_unique<ExtensionTelemetryService>(
      Profile::FromBrowserContext(context),
      network_service->GetURLLoaderFactory());
}

bool ExtensionTelemetryServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool ExtensionTelemetryServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
}  // namespace safe_browsing
