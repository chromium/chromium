// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_POLICY_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_POLICY_CONTROLLER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace ash {
class AccountManagerPolicyController;

class AccountManagerPolicyControllerFactory
    : public ProfileKeyedServiceFactory {
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

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_ACCOUNT_MANAGER_ACCOUNT_MANAGER_POLICY_CONTROLLER_FACTORY_H_
