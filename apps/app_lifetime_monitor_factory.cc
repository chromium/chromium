// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_lifetime_monitor_factory.h"

#include "apps/app_lifetime_monitor.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/browser/extensions_browser_client.h"

namespace apps {

// static
AppLifetimeMonitor* AppLifetimeMonitorFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AppLifetimeMonitor*>(
      GetInstance()->GetServiceForBrowserContext(context, false));
}

AppLifetimeMonitorFactory* AppLifetimeMonitorFactory::GetInstance() {
  return base::Singleton<AppLifetimeMonitorFactory>::get();
}

AppLifetimeMonitorFactory::AppLifetimeMonitorFactory()
    : BrowserContextKeyedServiceFactory(
        "AppLifetimeMonitor",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(extensions::AppWindowRegistry::Factory::GetInstance());
  DependsOn(extensions::ExtensionHostRegistry::GetFactory());
}

AppLifetimeMonitorFactory::~AppLifetimeMonitorFactory() = default;

KeyedService* AppLifetimeMonitorFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AppLifetimeMonitor(context);
}

bool AppLifetimeMonitorFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

content::BrowserContext* AppLifetimeMonitorFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return extensions::ExtensionsBrowserClient::Get()
      ->GetContextRedirectedToOriginal(context, /*force_guest_profile=*/true);
}

}  // namespace apps
