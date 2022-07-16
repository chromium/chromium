// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps_factory.h"

#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace apps {

// static
StandaloneBrowserExtensionApps*
StandaloneBrowserExtensionAppsFactory::GetForProfile(Profile* profile) {
  return static_cast<StandaloneBrowserExtensionApps*>(
      StandaloneBrowserExtensionAppsFactory::GetInstance()
          ->GetServiceForBrowserContext(profile, true /* create */));
}

// static
StandaloneBrowserExtensionAppsFactory*
StandaloneBrowserExtensionAppsFactory::GetInstance() {
  return base::Singleton<StandaloneBrowserExtensionAppsFactory>::get();
}

// static
void StandaloneBrowserExtensionAppsFactory::ShutDownForTesting(
    content::BrowserContext* context) {
  auto* factory = GetInstance();
  factory->BrowserContextShutdown(context);
  factory->BrowserContextDestroyed(context);
}

StandaloneBrowserExtensionAppsFactory::StandaloneBrowserExtensionAppsFactory()
    : BrowserContextKeyedServiceFactory(
          "StandaloneBrowserExtensionApps",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

KeyedService* StandaloneBrowserExtensionAppsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new StandaloneBrowserExtensionApps(
      AppServiceProxyFactory::GetForProfile(
          Profile::FromBrowserContext(context)));
}

}  // namespace apps
