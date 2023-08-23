// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_MODE_MANAGER_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_MODE_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/signin/account_consistency_mode_manager.h"

class AccountConsistencyModeManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns an instance of the factory singleton.
  static AccountConsistencyModeManagerFactory* GetInstance();

  static AccountConsistencyModeManager* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<AccountConsistencyModeManagerFactory>;

  AccountConsistencyModeManagerFactory();
  ~AccountConsistencyModeManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_MODE_MANAGER_FACTORY_H_
