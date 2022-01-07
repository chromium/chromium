// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_PRIMARY_ACCOUNT_POLICY_MANAGER_FACTORY_H_
#define CHROME_BROWSER_SIGNIN_PRIMARY_ACCOUNT_POLICY_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/signin/primary_account_policy_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

class PrimaryAccountPolicyManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns an instance of the factory singleton.
  static PrimaryAccountPolicyManagerFactory* GetInstance();
  static PrimaryAccountPolicyManager* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<PrimaryAccountPolicyManagerFactory>;

  PrimaryAccountPolicyManagerFactory();
  ~PrimaryAccountPolicyManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SIGNIN_PRIMARY_ACCOUNT_POLICY_MANAGER_FACTORY_H_
