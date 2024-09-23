// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/arc_apps_factory.h"

#include "base/feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/arc_apps.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"

namespace apps {

// static
ArcApps* ArcAppsFactory::GetForProfile(Profile* profile) {
  return static_cast<ArcApps*>(
      ArcAppsFactory::GetInstance()->GetServiceForBrowserContext(
          profile, true /* create */));
}

// static
ArcAppsFactory* ArcAppsFactory::GetInstance() {
  static base::NoDestructor<ArcAppsFactory> instance;
  return instance.get();
}

// static
void ArcAppsFactory::ShutDownForTesting(content::BrowserContext* context) {
  auto* factory = GetInstance();
  factory->BrowserContextShutdown(context);
  factory->BrowserContextDestroyed(context);
}

ArcAppsFactory::ArcAppsFactory()
    : ProfileKeyedServiceFactory(
          "ArcApps",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(AppServiceProxyFactory::GetInstance());
  DependsOn(ArcAppListPrefsFactory::GetInstance());
  DependsOn(arc::ArcIntentHelperBridge::GetFactory());
}

KeyedService* ArcAppsFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto* arc_apps = new ArcApps(AppServiceProxyFactory::GetForProfile(
      Profile::FromBrowserContext(context)));
  arc_apps->Initialize();
  return arc_apps;
}

}  // namespace apps
