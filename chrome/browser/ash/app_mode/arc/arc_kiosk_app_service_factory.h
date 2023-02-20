// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_ARC_ARC_KIOSK_APP_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_APP_MODE_ARC_ARC_KIOSK_APP_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace ash {

class ArcKioskAppService;

// Singleton that owns all ArcKioskAppServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated ArcKioskAppService.
class ArcKioskAppServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ArcKioskAppService* GetForBrowserContext(
      content::BrowserContext* context);

  static ArcKioskAppServiceFactory* GetInstance();

  ArcKioskAppServiceFactory(const ArcKioskAppServiceFactory&) = delete;
  ArcKioskAppServiceFactory& operator=(const ArcKioskAppServiceFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<ArcKioskAppServiceFactory>;

  ArcKioskAppServiceFactory();
  ~ArcKioskAppServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_ARC_ARC_KIOSK_APP_SERVICE_FACTORY_H_
