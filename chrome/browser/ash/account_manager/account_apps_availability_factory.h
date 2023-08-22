// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_APPS_AVAILABILITY_FACTORY_H_
#define CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_APPS_AVAILABILITY_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

#include "base/no_destructor.h"

class Profile;
class KeyedService;

namespace content {
class BrowserContext;
}

namespace ash {

class AccountAppsAvailability;

class AccountAppsAvailabilityFactory : public ProfileKeyedServiceFactory {
 public:
  static AccountAppsAvailabilityFactory* GetInstance();
  static AccountAppsAvailability* GetForProfile(Profile* profile);

  AccountAppsAvailabilityFactory(const AccountAppsAvailabilityFactory&) =
      delete;
  AccountAppsAvailabilityFactory& operator=(
      const AccountAppsAvailabilityFactory&) = delete;

 private:
  friend class base::NoDestructor<AccountAppsAvailabilityFactory>;

  AccountAppsAvailabilityFactory();
  ~AccountAppsAvailabilityFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_APPS_AVAILABILITY_FACTORY_H_
