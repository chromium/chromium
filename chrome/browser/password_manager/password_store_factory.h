// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_FACTORY_H_

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service_factory.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store_interface.h"

class Profile;

// Singleton that owns all PasswordStores and associates them with
// Profiles.
class PasswordStoreFactory : public RefcountedProfileKeyedServiceFactory {
 public:
  static scoped_refptr<password_manager::PasswordStoreInterface> GetForProfile(
      Profile* profile,
      ServiceAccessType set);

  static PasswordStoreFactory* GetInstance();

  PasswordStoreFactory(const PasswordStoreFactory&) = delete;
  PasswordStoreFactory& operator=(const PasswordStoreFactory&) = delete;

 private:
  friend base::NoDestructor<PasswordStoreFactory>;

  PasswordStoreFactory();
  ~PasswordStoreFactory() override;

  // RefcountedBrowserContextKeyedServiceFactory:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_FACTORY_H_
