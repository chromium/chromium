// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/enterprise/browser_management/management_service_factory.h"
#include "chrome/browser/extensions/chrome_extension_system_factory.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "chrome/browser/safe_browsing/network_context_service.h"
#include "chrome/browser/safe_browsing/network_context_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/safe_browsing/core/common/features.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_factory.h"

#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/connectors/reporting/extension_telemetry_event_router.h"
#include "chrome/browser/enterprise/connectors/reporting/extension_telemetry_event_router_factory.h"
#include "components/enterprise/isolated_mode/isolated_mode_features.h"
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)

namespace safe_browsing {

namespace {

// Enterprise Isolated Mode profiles only report to the enterprise pipeline,
// through their own `ExtensionTelemetryEventRouter`. The service constructor
// dereferences that router, so it must exist.
bool ShouldBuildForIsolatedModeProfile(Profile* profile) {
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  return enterprise_isolated_mode::
             IsExtensionTelemetryEnabledInIsolatedMode() &&
         enterprise_connectors::ExtensionTelemetryEventRouter::Get(profile);
#else
  return false;
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
}

}  // namespace

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
          "ExtensionTelemetryService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              // AshInternals should be 'kNone' since ExtensionTelemetryService
              // should not be initialized for internal profiles on ChromeOS
              // Ash.
              .WithAshInternals(ProfileSelection::kNone)
              // Gated in BuildServiceInstanceForBrowserContext().
              .WithIsolatedMode(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(NetworkContextServiceFactory::GetInstance());
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(extensions::ExtensionManagementFactory::GetInstance());
  DependsOn(extensions::ChromeExtensionSystemFactory::GetInstance());
  DependsOn(TemplateURLServiceFactory::GetInstance());
  DependsOn(policy::ManagementServiceFactory::GetInstance());
#if BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
  DependsOn(enterprise_connectors::ConnectorsServiceFactory::GetInstance());
  DependsOn(enterprise_connectors::ExtensionTelemetryEventRouterFactory::
                GetInstance());
#endif  // BUILDFLAG(ENTERPRISE_CLOUD_CONTENT_ANALYSIS)
}

std::unique_ptr<KeyedService>
ExtensionTelemetryServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (profile->IsEnterpriseIsolatedModeProfile()) {
    if (!ShouldBuildForIsolatedModeProfile(profile)) {
      return nullptr;
    }
    // This service never uploads to Safe Browsing, so it gets no
    // `URLLoaderFactory`.
    return std::make_unique<ExtensionTelemetryService>(
        profile, /*url_loader_factory=*/nullptr);
  }

  NetworkContextService* network_service =
      NetworkContextServiceFactory::GetForBrowserContext(
          context, /*use_same_instance_in_off_the_record=*/true);
  if (!network_service) {
    return nullptr;
  }
  return std::make_unique<ExtensionTelemetryService>(
      profile, network_service->GetURLLoaderFactory());
}

bool ExtensionTelemetryServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool ExtensionTelemetryServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
}  // namespace safe_browsing
