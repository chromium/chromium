// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_PRIMARY_ACCOUNT_POLICY_MANAGER_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_PRIMARY_ACCOUNT_POLICY_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/signin/primary_account_policy_manager.h"

class Profile;

class PrimaryAccountPolicyManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns an instance of the factory singleton.
  static PrimaryAccountPolicyManagerFactory* GetInstance();
  static PrimaryAccountPolicyManager* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<PrimaryAccountPolicyManagerFactory>;

  PrimaryAccountPolicyManagerFactory();
  ~PrimaryAccountPolicyManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SIGNIN_PRIMARY_ACCOUNT_POLICY_MANAGER_FACTORY_H_
