// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/auth_factors_policy/local_auth_factors_policy_controller_factory.h"

#include "chrome/browser/ash/login/auth_factors_policy/local_auth_factors_policy_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/account_id/account_id.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/user_manager/user.h"
#include "content/public/browser/browser_context.h"

namespace ash {

LocalAuthFactorsPolicyController*
LocalAuthFactorsPolicyControllerFactory::GetForProfile(Profile* profile) {
  return static_cast<LocalAuthFactorsPolicyController*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

LocalAuthFactorsPolicyControllerFactory*
LocalAuthFactorsPolicyControllerFactory::GetInstance() {
  static base::NoDestructor<LocalAuthFactorsPolicyControllerFactory> instance;
  return instance.get();
}

LocalAuthFactorsPolicyControllerFactory::
    LocalAuthFactorsPolicyControllerFactory()
    : ProfileKeyedServiceFactory(
          "LocalAuthFactorsPolicyController",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {}

LocalAuthFactorsPolicyControllerFactory::
    ~LocalAuthFactorsPolicyControllerFactory() = default;

std::unique_ptr<KeyedService>
LocalAuthFactorsPolicyControllerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  CHECK(context);
  Profile* profile = Profile::FromBrowserContext(context);
  if (!profile) {
    LOG(WARNING) << "Profile was null not building service instance.";
    return nullptr;
  }
  const user_manager::User* user =
      BrowserContextHelper::Get()->GetUserByBrowserContext(profile);

  if (!user) {
    LOG(WARNING) << "User was null not building service instance.";
    return nullptr;
  }

  if (!ash::UserDataAuthClient::Get()) {
    LOG(WARNING)
        << "UserDataAuthClient was null not building service instance.";
    return nullptr;
  }

  const AccountId& account_id = user->GetAccountId();
  return std::make_unique<LocalAuthFactorsPolicyController>(
      *g_browser_process->local_state(), profile, account_id);
}

bool LocalAuthFactorsPolicyControllerFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace ash
