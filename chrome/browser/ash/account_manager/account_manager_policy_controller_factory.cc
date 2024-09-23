// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/account_manager/account_manager_policy_controller_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/account_manager/account_manager_policy_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/account_manager/account_manager_factory.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_manager_core/account_manager_facade.h"
#include "components/account_manager_core/chromeos/account_manager_facade_factory.h"
#include "components/user_manager/user.h"

namespace ash {

// static
AccountManagerPolicyController*
AccountManagerPolicyControllerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AccountManagerPolicyController*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AccountManagerPolicyControllerFactory*
AccountManagerPolicyControllerFactory::GetInstance() {
  static base::NoDestructor<AccountManagerPolicyControllerFactory> instance;
  return instance.get();
}

AccountManagerPolicyControllerFactory::AccountManagerPolicyControllerFactory()
    : ProfileKeyedServiceFactory(
          "AccountManagerPolicyController",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

AccountManagerPolicyControllerFactory::
    ~AccountManagerPolicyControllerFactory() = default;

std::unique_ptr<KeyedService>
AccountManagerPolicyControllerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* const profile = Profile::FromBrowserContext(context);
  auto* factory =
      g_browser_process->platform_part()->GetAccountManagerFactory();
  auto* account_manager =
      factory->GetAccountManager(profile->GetPath().value());

  if (!account_manager)
    return nullptr;

  auto* account_manager_facade =
      ::GetAccountManagerFacade(profile->GetPath().value());

  if (!account_manager_facade)
    return nullptr;

  const user_manager::User* const user =
      BrowserContextHelper::Get()->GetUserByBrowserContext(profile);
  if (!user)
    return nullptr;

  std::unique_ptr<AccountManagerPolicyController> service =
      std::make_unique<AccountManagerPolicyController>(profile, account_manager,
                                                       account_manager_facade,
                                                       user->GetAccountId());
  // Auto-start the Service.
  service->Start();

  return service;
}

}  // namespace ash
