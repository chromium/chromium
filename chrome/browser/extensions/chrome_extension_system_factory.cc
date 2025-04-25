// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/chrome_extension_system_factory.h"

#include "chrome/browser/extensions/blocklist_factory.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker_factory.h"
#include "chrome/browser/extensions/install_verifier_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/renderer_startup_helper.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#endif

namespace extensions {

// ChromeExtensionSystemSharedFactory

// static
ChromeExtensionSystem::Shared*
ChromeExtensionSystemSharedFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ChromeExtensionSystem::Shared*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ChromeExtensionSystemSharedFactory*
ChromeExtensionSystemSharedFactory::GetInstance() {
  static base::NoDestructor<ChromeExtensionSystemSharedFactory> instance;
  return instance.get();
}

ChromeExtensionSystemSharedFactory::ChromeExtensionSystemSharedFactory()
    : ProfileKeyedServiceFactory(
          "ExtensionSystemShared",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Audit whether these should be
              // redirected or should have their own instance.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionManagementFactory::GetInstance());
  // This depends on ExtensionService, which depends on ExtensionRegistry.
  DependsOn(ExtensionRegistryFactory::GetInstance());
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // GlobalErrorService is only used on Win/Mac/Linux.
  DependsOn(GlobalErrorServiceFactory::GetInstance());
#endif
  DependsOn(InstallVerifierFactory::GetInstance());
  DependsOn(ProcessManagerFactory::GetInstance());
  DependsOn(RendererStartupHelperFactory::GetInstance());
  DependsOn(BlocklistFactory::GetInstance());
  DependsOn(EventRouterFactory::GetInstance());
  // This depends on ExtensionDownloader, which depends on
  // IdentityManager for webstore authentication.
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(InstallStageTrackerFactory::GetInstance());
  // ExtensionService (owned by the ExtensionSystem) depends on
  // ExtensionHostRegistry.
  DependsOn(ExtensionHostRegistry::GetFactory());
}

ChromeExtensionSystemSharedFactory::~ChromeExtensionSystemSharedFactory() =
    default;

std::unique_ptr<KeyedService>
ChromeExtensionSystemSharedFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ChromeExtensionSystem::Shared>(
      static_cast<Profile*>(context));
}

// ChromeExtensionSystemFactory

// static
ExtensionSystem* ChromeExtensionSystemFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionSystem*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ChromeExtensionSystemFactory* ChromeExtensionSystemFactory::GetInstance() {
  static base::NoDestructor<ChromeExtensionSystemFactory> instance;
  return instance.get();
}

ChromeExtensionSystemFactory::ChromeExtensionSystemFactory()
    : ExtensionSystemProvider("ExtensionSystem",
                              BrowserContextDependencyManager::GetInstance()) {
  DCHECK(ExtensionsBrowserClient::Get())
      << "ChromeExtensionSystemFactory must be initialized after "
         "BrowserProcess";
  DependsOn(ChromeExtensionSystemSharedFactory::GetInstance());
}

ChromeExtensionSystemFactory::~ChromeExtensionSystemFactory() = default;

std::unique_ptr<KeyedService>
ChromeExtensionSystemFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ChromeExtensionSystem>(
      static_cast<Profile*>(context));
}

content::BrowserContext* ChromeExtensionSystemFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kOwnInstance)
      .WithGuest(ProfileSelection::kOwnInstance)
      // TODO(crbug.com/41488885): Check if this service is needed for
      // Ash Internals.
      .WithAshInternals(ProfileSelection::kOwnInstance)
      .Build()
      .ApplyProfileSelection(Profile::FromBrowserContext(context));
}

bool ChromeExtensionSystemFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions
