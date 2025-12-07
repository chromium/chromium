// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/arcvm_app/kiosk_arcvm_app_service_factory.h"

#include "base/check.h"
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs_factory.h"
#include "chrome/browser/ash/app_mode/arcvm_app/kiosk_arcvm_app_service.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {

// static
KioskArcvmAppService* KioskArcvmAppServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  // TODO (crbug.com/418892211): Refactor to use
  // BrowserContextKeyedServiceFactory OR get rid of KeyedService entirely.
  return static_cast<KioskArcvmAppService*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
KioskArcvmAppServiceFactory* KioskArcvmAppServiceFactory::GetInstance() {
  static base::NoDestructor<KioskArcvmAppServiceFactory> instance;
  return instance.get();
}

KioskArcvmAppServiceFactory::KioskArcvmAppServiceFactory()
    : ProfileKeyedServiceFactory(
          "KioskArcvmAppService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(ArcAppListPrefsFactory::GetInstance());
}

KioskArcvmAppServiceFactory::~KioskArcvmAppServiceFactory() = default;

std::unique_ptr<KeyedService>
KioskArcvmAppServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  CHECK(profile);

  return std::make_unique<KioskArcvmAppService>(profile);
}

}  // namespace ash
