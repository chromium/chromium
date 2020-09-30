// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_MANAGER_USER_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_MANAGER_USER_SERVICE_H_

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/platform_keys/key_permissions/key_permissions_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chromeos {
namespace platform_keys {

// KeyPermissionsManagerUserService is a wrapper over KeyPermissionsManager
// (KPM) to provide KPMs keyed by profile. KPM is not a KeyedService by itself
// so as future work can introduce a global device-wide KPM instance.
class KeyPermissionsManagerUserService : public KeyedService {
 public:
  KeyPermissionsManagerUserService();
  ~KeyPermissionsManagerUserService() override;

  virtual KeyPermissionsManager* key_permissions_manager() = 0;
};

class KeyPermissionsManagerUserServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static KeyPermissionsManagerUserService* GetForBrowserContext(
      content::BrowserContext* context);
  static KeyPermissionsManagerUserServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<KeyPermissionsManagerUserServiceFactory>;

  KeyPermissionsManagerUserServiceFactory();
  ~KeyPermissionsManagerUserServiceFactory() override = default;

  // BrowserStateKeyedServiceFactory.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace platform_keys
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PLATFORM_KEYS_KEY_PERMISSIONS_KEY_PERMISSIONS_MANAGER_USER_SERVICE_H_
