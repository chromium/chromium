// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/app_load_service_factory.h"

#include "chrome/browser/apps/platform_apps/app_load_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/browser/extensions_browser_client.h"

namespace apps {

// static
AppLoadService* AppLoadServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AppLoadService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

AppLoadServiceFactory* AppLoadServiceFactory::GetInstance() {
  return base::Singleton<AppLoadServiceFactory>::get();
}

AppLoadServiceFactory::AppLoadServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "AppLoadService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::AppWindowRegistry::Factory::GetInstance());
  DependsOn(extensions::ExtensionPrefsFactory::GetInstance());
  DependsOn(extensions::ExtensionRegistryFactory::GetInstance());
  DependsOn(extensions::ExtensionHostRegistry::GetFactory());
  DependsOn(
      extensions::ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
}

AppLoadServiceFactory::~AppLoadServiceFactory() {}

KeyedService* AppLoadServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AppLoadService(context);
}

bool AppLoadServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

content::BrowserContext* AppLoadServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Redirected in incognito.
  return extensions::ExtensionsBrowserClient::Get()->GetOriginalContext(
      context);
}

}  // namespace apps
