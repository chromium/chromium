// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_system_factory.h"

#include "chrome/browser/extensions/blocklist_factory.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker_factory.h"
#include "chrome/browser/extensions/install_verifier_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/event_router_factory.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/renderer_startup_helper.h"

namespace extensions {

// ExtensionSystemSharedFactory

// static
ExtensionSystemImpl::Shared*
ExtensionSystemSharedFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionSystemImpl::Shared*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionSystemSharedFactory* ExtensionSystemSharedFactory::GetInstance() {
  static base::NoDestructor<ExtensionSystemSharedFactory> instance;
  return instance.get();
}

ExtensionSystemSharedFactory::ExtensionSystemSharedFactory()
    : ProfileKeyedServiceFactory(
          "ExtensionSystemShared",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionManagementFactory::GetInstance());
  // This depends on ExtensionService, which depends on ExtensionRegistry.
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(GlobalErrorServiceFactory::GetInstance());
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

ExtensionSystemSharedFactory::~ExtensionSystemSharedFactory() = default;

std::unique_ptr<KeyedService>
ExtensionSystemSharedFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<ExtensionSystemImpl::Shared>(
      static_cast<Profile*>(context));
}

// ExtensionSystemFactory

// static
ExtensionSystem* ExtensionSystemFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ExtensionSystem*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
ExtensionSystemFactory* ExtensionSystemFactory::GetInstance() {
  static base::NoDestructor<ExtensionSystemFactory> instance;
  return instance.get();
}

ExtensionSystemFactory::ExtensionSystemFactory()
    : ExtensionSystemProvider("ExtensionSystem",
                              BrowserContextDependencyManager::GetInstance()) {
  DCHECK(ExtensionsBrowserClient::Get())
      << "ExtensionSystemFactory must be initialized after BrowserProcess";
  DependsOn(ExtensionSystemSharedFactory::GetInstance());
}

ExtensionSystemFactory::~ExtensionSystemFactory() = default;

KeyedService* ExtensionSystemFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ExtensionSystemImpl(static_cast<Profile*>(context));
}

content::BrowserContext* ExtensionSystemFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return ProfileSelections::Builder()
      .WithRegular(ProfileSelection::kOwnInstance)
      // TODO(crbug.com/40257657): Check if this service is needed in
      // Guest mode.
      .WithGuest(ProfileSelection::kOwnInstance)
      // TODO(crbug.com/41488885): Check if this service is needed for
      // Ash Internals.
      .WithAshInternals(ProfileSelection::kOwnInstance)
      .Build()
      .ApplyProfileSelection(Profile::FromBrowserContext(context));
}

bool ExtensionSystemFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions
