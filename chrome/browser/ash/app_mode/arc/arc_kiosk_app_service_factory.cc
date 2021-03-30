// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_service_factory.h"

#include "chrome/browser/ash/app_mode/arc/arc_kiosk_app_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

namespace ash {

// static
ArcKioskAppService* ArcKioskAppServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ArcKioskAppService*>(
      GetInstance()->GetServiceForBrowserContext(context, true /* create */));
}

// static
ArcKioskAppServiceFactory* ArcKioskAppServiceFactory::GetInstance() {
  return base::Singleton<ArcKioskAppServiceFactory>::get();
}

ArcKioskAppServiceFactory::ArcKioskAppServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ArcKioskAppService",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ArcAppListPrefsFactory::GetInstance());
}

ArcKioskAppServiceFactory::~ArcKioskAppServiceFactory() = default;

KeyedService* ArcKioskAppServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  DCHECK(profile);

  return ArcKioskAppService::Create(profile);
}

}  // namespace ash
