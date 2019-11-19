// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_system_factory.h"

#include "chrome/browser/extensions/blacklist_factory.h"
#include "chrome/browser/extensions/extension_management.h"
#include "chrome/browser/extensions/forced_extensions/installation_reporter_factory.h"
#include "chrome/browser/extensions/install_verifier_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "extensions/browser/declarative_user_script_manager_factory.h"
#include "extensions/browser/event_router_factory.h"
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
  return base::Singleton<ExtensionSystemSharedFactory>::get();
}

ExtensionSystemSharedFactory::ExtensionSystemSharedFactory()
    : BrowserContextKeyedServiceFactory(
        "ExtensionSystemShared",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionPrefsFactory::GetInstance());
  DependsOn(ExtensionManagementFactory::GetInstance());
  // This depends on ExtensionService, which depends on ExtensionRegistry.
  DependsOn(ExtensionRegistryFactory::GetInstance());
  DependsOn(GlobalErrorServiceFactory::GetInstance());
  DependsOn(InstallVerifierFactory::GetInstance());
  DependsOn(ProcessManagerFactory::GetInstance());
  DependsOn(RendererStartupHelperFactory::GetInstance());
  DependsOn(BlacklistFactory::GetInstance());
  DependsOn(DeclarativeUserScriptManagerFactory::GetInstance());
  DependsOn(EventRouterFactory::GetInstance());
  // This depends on ExtensionDownloader, which depends on
  // IdentityManager for webstore authentication.
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(InstallationReporterFactory::GetInstance());
}

ExtensionSystemSharedFactory::~ExtensionSystemSharedFactory() {
}

KeyedService* ExtensionSystemSharedFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ExtensionSystemImpl::Shared(static_cast<Profile*>(context));
}

content::BrowserContext* ExtensionSystemSharedFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Redirected in incognito.
  return ExtensionsBrowserClient::Get()->GetOriginalContext(context);
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
  return base::Singleton<ExtensionSystemFactory>::get();
}

ExtensionSystemFactory::ExtensionSystemFactory()
    : ExtensionSystemProvider("ExtensionSystem",
                              BrowserContextDependencyManager::GetInstance()) {
  DCHECK(ExtensionsBrowserClient::Get())
      << "ExtensionSystemFactory must be initialized after BrowserProcess";
  DependsOn(ExtensionSystemSharedFactory::GetInstance());
}

ExtensionSystemFactory::~ExtensionSystemFactory() {
}

KeyedService* ExtensionSystemFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ExtensionSystemImpl(static_cast<Profile*>(context));
}

content::BrowserContext* ExtensionSystemFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Separate instance in incognito.
  return context;
}

bool ExtensionSystemFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace extensions
