// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_ACCOUNTS_POLICY_MANAGER_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_ACCOUNTS_POLICY_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/signin/accounts_policy_manager.h"

class Profile;

class AccountsPolicyManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns an instance of the factory singleton.
  static AccountsPolicyManagerFactory* GetInstance();
  static AccountsPolicyManager* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<AccountsPolicyManagerFactory>;

  AccountsPolicyManagerFactory();
  ~AccountsPolicyManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SIGNIN_ACCOUNTS_POLICY_MANAGER_FACTORY_H_
