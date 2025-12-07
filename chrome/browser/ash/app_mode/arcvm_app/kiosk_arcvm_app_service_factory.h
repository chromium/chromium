// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ASH_APP_MODE_ARCVM_APP_KIOSK_ARCVM_APP_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_APP_MODE_ARCVM_APP_KIOSK_ARCVM_APP_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace ash {
class KioskArcvmAppService;
// Singleton that owns all KioskArcvmAppServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated KioskArcvmAppService.
// TODO (crbug.com/418892211): Refactor to use BrowserContextKeyedServiceFactory
// OR get rid of KeyedService entirely.
class KioskArcvmAppServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static KioskArcvmAppService* GetForBrowserContext(
      content::BrowserContext* context);
  static KioskArcvmAppServiceFactory* GetInstance();

  KioskArcvmAppServiceFactory(const KioskArcvmAppServiceFactory&) = delete;
  KioskArcvmAppServiceFactory& operator=(const KioskArcvmAppServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<KioskArcvmAppServiceFactory>;

  KioskArcvmAppServiceFactory();

  ~KioskArcvmAppServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash
#endif  // CHROME_BROWSER_ASH_APP_MODE_ARCVM_APP_KIOSK_ARCVM_APP_SERVICE_FACTORY_H_
