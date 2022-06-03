// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_POLICY_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_POLICY_CONTROLLER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace ash {
class AccountManagerPolicyController;

class AccountManagerPolicyControllerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  AccountManagerPolicyControllerFactory(
      const AccountManagerPolicyControllerFactory&) = delete;
  AccountManagerPolicyControllerFactory& operator=(
      const AccountManagerPolicyControllerFactory&) = delete;

  // Gets the instance of the service associated with |context|.
  static AccountManagerPolicyController* GetForBrowserContext(
      content::BrowserContext* context);

  // Gets the singleton instance of the factory
  // (|AccountManagerPolicyControllerFactory|).
  static AccountManagerPolicyControllerFactory* GetInstance();

 private:
  friend class base::NoDestructor<AccountManagerPolicyControllerFactory>;

  AccountManagerPolicyControllerFactory();
  ~AccountManagerPolicyControllerFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_POLICY_CONTROLLER_FACTORY_H_
