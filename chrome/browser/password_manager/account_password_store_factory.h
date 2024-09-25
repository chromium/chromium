// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_PASSWORD_STORE_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_PASSWORD_STORE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service_factory.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"

class Profile;

// Singleton that owns all Gaia-account-scoped PasswordStores and associates
// them with Profiles.
class AccountPasswordStoreFactory
    : public RefcountedProfileKeyedServiceFactory {
 public:
  static scoped_refptr<password_manager::PasswordStoreInterface> GetForProfile(
      Profile* profile,
      ServiceAccessType set);

  // Returns whether a PasswordStore was created for `profile`. GetForProfile()
  // can't be used because it creates the store if one doesn't exist yet.
  static bool HasStore(Profile* profile);

  static AccountPasswordStoreFactory* GetInstance();

  AccountPasswordStoreFactory(const AccountPasswordStoreFactory&) = delete;
  AccountPasswordStoreFactory& operator=(const AccountPasswordStoreFactory&) =
      delete;

  // Returns the default factory, useful in tests where the service is null by
  // default.
  static TestingFactory GetDefaultFactoryForTesting();

 private:
  friend base::NoDestructor<AccountPasswordStoreFactory>;

  AccountPasswordStoreFactory();
  ~AccountPasswordStoreFactory() override;

  // RefcountedBrowserContextKeyedServiceFactory:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_PASSWORD_STORE_FACTORY_H_
