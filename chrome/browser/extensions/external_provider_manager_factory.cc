// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/external_provider_manager_factory.h"

#include "base/check.h"
#include "chrome/browser/extensions/extension_error_controller_factory.h"
#include "chrome/browser/extensions/external_install_manager_factory.h"
#include "chrome/browser/extensions/external_provider_manager.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registrar_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/pending_extension_manager_factory.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/extensions/install_limiter_factory.h"
#endif

using content::BrowserContext;

namespace extensions {

// static
ExternalProviderManager* ExternalProviderManagerFactory::GetForBrowserContext(
    BrowserContext* context) {
  return static_cast<ExternalProviderManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExternalProviderManagerFactory* ExternalProviderManagerFactory::GetInstance() {
  static base::NoDestructor<ExternalProviderManagerFactory> instance;
  return instance.get();
}

ExternalProviderManagerFactory::ExternalProviderManagerFactory()
    : ProfileKeyedServiceFactory(
          "ExternalProviderManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Audit whether these should be
              // redirected or should have their own instance.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ExtensionErrorControllerFactory::GetInstance());
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(ExtensionRegistrarFactory::GetInstance());
  DependsOn(ExternalInstallManagerFactory::GetInstance());
  DependsOn(PendingExtensionManagerFactory::GetInstance());
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(InstallStageTrackerFactory::GetInstance());
#if BUILDFLAG(IS_CHROMEOS)
  DependsOn(InstallLimiterFactory::GetInstance());
#endif
}

ExternalProviderManagerFactory::~ExternalProviderManagerFactory() = default;

std::unique_ptr<KeyedService>
ExternalProviderManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ExternalProviderManager>(context);
}

}  // namespace extensions
