// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_UPDATE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_UPDATE_SERVICE_FACTORY_H_

#include <memory>

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash {

class KioskAppUpdateService;

// Singleton that owns all KioskAppUpdateServices and associates them with
// profiles.
class KioskAppUpdateServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the KioskAppUpdateService for `profile`, creating it if it is not
  // yet created.
  static KioskAppUpdateService* GetForProfile(Profile* profile);

  // Returns the KioskAppUpdateServiceFactory instance.
  static KioskAppUpdateServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<KioskAppUpdateServiceFactory>;

  KioskAppUpdateServiceFactory();
  ~KioskAppUpdateServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_KIOSK_APP_UPDATE_SERVICE_FACTORY_H_
