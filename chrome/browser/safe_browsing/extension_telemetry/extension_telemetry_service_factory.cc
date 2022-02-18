// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service_factory.h"

#include "base/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_service.h"
#include "chrome/browser/safe_browsing/network_context_service.h"
#include "chrome/browser/safe_browsing/network_context_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
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
  // The feature enable check happens in BuildServiceInstanceFor too but
  // added this check here for expediency when the feature is disabled.
  if (!base::FeatureList::IsEnabled(kExtensionTelemetry))
    return nullptr;
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
    : BrowserContextKeyedServiceFactory(
          "ExtensionTelemetryService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(extensions::ExtensionManagementFactory::GetInstance());
}

KeyedService* ExtensionTelemetryServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(kExtensionTelemetry))
    return nullptr;
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile->IsRegularProfile())
    return nullptr;
  NetworkContextService* network_service =
      NetworkContextServiceFactory::GetForBrowserContext(context);
  if (!network_service)
    return nullptr;
  return new ExtensionTelemetryService(
      profile, network_service->GetURLLoaderFactory(),
      extensions::ExtensionRegistry::Get(context),
      extensions::ExtensionPrefs::Get(context));
}

bool ExtensionTelemetryServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace safe_browsing
