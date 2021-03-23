// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/arc_apps_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/arc_apps.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace apps {

// static
ArcApps* ArcAppsFactory::GetForProfile(Profile* profile) {
  return static_cast<ArcApps*>(
      ArcAppsFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
}

// static
ArcAppsFactory* ArcAppsFactory::GetInstance() {
  return base::Singleton<ArcAppsFactory>::get();
}

// static
void ArcAppsFactory::ShutDownForTesting(content::BrowserContext* context) {
  auto* factory = GetInstance();
  factory->BrowserContextShutdown(context);
  factory->BrowserContextDestroyed(context);
}

ArcAppsFactory::ArcAppsFactory()
    : BrowserContextKeyedServiceFactory(
          "ArcApps",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ArcAppListPrefsFactory::GetInstance());
  DependsOn(arc::ArcIntentHelperBridge::GetFactory());
  DependsOn(apps::AppServiceProxyFactory::GetInstance());
}

KeyedService* ArcAppsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ArcApps(Profile::FromBrowserContext(context));
}

}  // namespace apps
