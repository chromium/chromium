// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/app_restore_service_factory.h"

#include "apps/app_lifetime_monitor_factory.h"
#include "apps/app_restore_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extensions_browser_client.h"

namespace apps {

// static
AppRestoreService* AppRestoreServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AppRestoreService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

AppRestoreServiceFactory* AppRestoreServiceFactory::GetInstance() {
  return base::Singleton<AppRestoreServiceFactory>::get();
}

AppRestoreServiceFactory::AppRestoreServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "AppRestoreService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(AppLifetimeMonitorFactory::GetInstance());
}

AppRestoreServiceFactory::~AppRestoreServiceFactory() = default;

std::unique_ptr<KeyedService>
AppRestoreServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<AppRestoreService>(context);
}

bool AppRestoreServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

content::BrowserContext* AppRestoreServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return extensions::ExtensionsBrowserClient::Get()
      ->GetContextRedirectedToOriginal(context, /*force_guest_profile=*/true);
}

}  // namespace apps
