// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ACCOUNT_RECONCILOR_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_ACCOUNT_RECONCILOR_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace signin {
class AccountReconcilorDelegate;
}

class AccountReconcilor;

// Singleton that owns all AccountReconcilors and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up.
class AccountReconcilorFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of AccountReconcilor associated with this profile
  // (creating one if none exists). Returns NULL if this profile cannot have an
  // AccountReconcilor (for example, if |profile| is incognito).
  static AccountReconcilor* GetForProfile(Profile* profile);

  // Returns an instance of the factory singleton.
  static AccountReconcilorFactory* GetInstance();

  // BrowserContextKeyedServiceFactory:
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;

 private:
  friend base::NoDestructor<AccountReconcilorFactory>;
  friend class DummyAccountReconcilorWithDelegate;  // For testing.

  AccountReconcilorFactory();
  ~AccountReconcilorFactory() override;

  // Creates the AccountReconcilorDelegate.
  static std::unique_ptr<signin::AccountReconcilorDelegate>
  CreateAccountReconcilorDelegate(Profile* profile);

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SIGNIN_ACCOUNT_RECONCILOR_FACTORY_H_
