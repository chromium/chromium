// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_FACTORY_H_

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "components/keyed_service/content/refcounted_browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_store_interface.h"

class Profile;

// Singleton that owns all PasswordStores and associates them with
// Profiles.
class PasswordStoreFactory
    : public RefcountedBrowserContextKeyedServiceFactory {
 public:
  static scoped_refptr<password_manager::PasswordStoreInterface> GetForProfile(
      Profile* profile,
      ServiceAccessType set);

  static PasswordStoreFactory* GetInstance();

  PasswordStoreFactory(const PasswordStoreFactory&) = delete;
  PasswordStoreFactory& operator=(const PasswordStoreFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<PasswordStoreFactory>;

  PasswordStoreFactory();
  ~PasswordStoreFactory() override;

  // RefcountedBrowserContextKeyedServiceFactory:
  scoped_refptr<RefcountedKeyedService> BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_FACTORY_H_
