// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PHONEHUB_PHONE_HUB_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_PHONEHUB_PHONE_HUB_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace ash {
namespace phonehub {

class PhoneHubManager;

class PhoneHubManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the PhoneHubManager instance associated with |profile|. Null is
  // returned if |profile| is not the primary Profile, if the kPhoneHub flag
  // is disabled, or if the feature is prohibited by policy.
  static PhoneHubManager* GetForProfile(Profile* profile);

  static PhoneHubManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<PhoneHubManagerFactory>;

  PhoneHubManagerFactory();
  PhoneHubManagerFactory(const PhoneHubManagerFactory&) = delete;
  PhoneHubManagerFactory& operator=(const PhoneHubManagerFactory&) = delete;
  ~PhoneHubManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  void BrowserContextShutdown(content::BrowserContext* context) override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace phonehub
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PHONEHUB_PHONE_HUB_MANAGER_FACTORY_H_
