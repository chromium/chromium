// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class BrowserContext;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chromeos {
namespace platform_keys {

class KeyPermissionsService;

// KeyPermissionsServiceFactory can be used for retrieving KeyPermissionsService
// instances for a specific BrowserContext.
//
// Note: Service instances provided by this factory are only valid during the
// lifetime of the given BrowserContext.
class KeyPermissionsServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static KeyPermissionsService* GetForBrowserContext(
      content::BrowserContext* context);
  static KeyPermissionsServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<KeyPermissionsServiceFactory>;

  KeyPermissionsServiceFactory();
  ~KeyPermissionsServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace platform_keys
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_SERVICE_FACTORY_H_
