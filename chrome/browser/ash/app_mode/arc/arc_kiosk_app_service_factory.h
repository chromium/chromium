// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_ARC_ARC_KIOSK_APP_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_APP_MODE_ARC_ARC_KIOSK_APP_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace ash {

class ArcKioskAppService;

// Singleton that owns all ArcKioskAppServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated ArcKioskAppService.
class ArcKioskAppServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ArcKioskAppService* GetForBrowserContext(
      content::BrowserContext* context);

  static ArcKioskAppServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ArcKioskAppServiceFactory>;

  ArcKioskAppServiceFactory();
  ~ArcKioskAppServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ArcKioskAppServiceFactory);
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the //chrome/browser/chromeos
// source code migration is finished.
namespace chromeos {
using ::ash::ArcKioskAppServiceFactory;
}

#endif  // CHROME_BROWSER_ASH_APP_MODE_ARC_ARC_KIOSK_APP_SERVICE_FACTORY_H_
