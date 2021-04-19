// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/lacros_web_apps_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/lacros_web_apps.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace apps {

// static
LacrosWebApps* LacrosWebAppsFactory::GetForProfile(Profile* profile) {
  return static_cast<LacrosWebApps*>(
      LacrosWebAppsFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
}

// static
LacrosWebAppsFactory* LacrosWebAppsFactory::GetInstance() {
  return base::Singleton<LacrosWebAppsFactory>::get();
}

// static
void LacrosWebAppsFactory::ShutDownForTesting(
    content::BrowserContext* context) {
  auto* factory = GetInstance();
  factory->BrowserContextShutdown(context);
  factory->BrowserContextDestroyed(context);
}

LacrosWebAppsFactory::LacrosWebAppsFactory()
    : BrowserContextKeyedServiceFactory(
          "LacrosWebApps",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

KeyedService* LacrosWebAppsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new LacrosWebApps(Profile::FromBrowserContext(context));
}

}  // namespace apps
