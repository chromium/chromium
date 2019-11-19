// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_STORAGE_ACCOUNT_PASSWORD_STORE_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_STORAGE_ACCOUNT_PASSWORD_STORE_FACTORY_H_

#include "base/macros.h"
#include "base/memory/singleton.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/service_access_type.h"

class Profile;

namespace password_manager {
class PasswordStore;
}

// Singleton that owns all Gaia-account-scoped PasswordStores and associates
// them with Profiles.
class AccountPasswordStoreFactory
    : public RefcountedBrowserContextKeyedServiceFactory {
 public:
  static scoped_refptr<password_manager::PasswordStore> GetForProfile(
      Profile* profile,
      ServiceAccessType set);

  static AccountPasswordStoreFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<AccountPasswordStoreFactory>;

  AccountPasswordStoreFactory();
  ~AccountPasswordStoreFactory() override;

  // RefcountedBrowserContextKeyedServiceFactory:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(AccountPasswordStoreFactory);
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ACCOUNT_STORAGE_ACCOUNT_PASSWORD_STORE_FACTORY_H_
