// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/networking/user_network_configuration_updater_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/net/nss_service_factory.h"
#include "chrome/browser/policy/networking/user_network_configuration_updater.h"
#include "chrome/browser/policy/networking/user_network_configuration_updater_ash.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"

static_assert(BUILDFLAG(IS_CHROMEOS));

namespace policy {

// static
UserNetworkConfigurationUpdater*
UserNetworkConfigurationUpdaterFactory::GetForBrowserContext(
    content::BrowserContext* browser_context) {
  return static_cast<UserNetworkConfigurationUpdater*>(
      GetInstance()->GetServiceForBrowserContext(browser_context, true));
}

// static
UserNetworkConfigurationUpdaterFactory*
UserNetworkConfigurationUpdaterFactory::GetInstance() {
  static base::NoDestructor<UserNetworkConfigurationUpdaterFactory> instance;
  return instance.get();
}

UserNetworkConfigurationUpdaterFactory::UserNetworkConfigurationUpdaterFactory()
    : ProfileKeyedServiceFactory(
          "UserNetworkConfigurationUpdater",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // Guest Profile follows Regular Profile selection mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // On the login/lock screen only device network policies apply.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(NssServiceFactory::GetInstance());
}

UserNetworkConfigurationUpdaterFactory::
    ~UserNetworkConfigurationUpdaterFactory() = default;

bool UserNetworkConfigurationUpdaterFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

bool UserNetworkConfigurationUpdaterFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

std::unique_ptr<KeyedService>
UserNetworkConfigurationUpdaterFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  DCHECK(user);
  // Currently, only the network policy of the primary user is supported. See
  // also http://crbug.com/310685 .
  if (user != user_manager::UserManager::Get()->GetPrimaryUser())
    return nullptr;

  // Note that sessions which don't have policy (e.g. guest sessions) still
  // expect to have UserNetworkConfigurationUpdater, because
  // ManagedNetworkConfigurationHandler requires a (possibly empty) policy to be
  // set for all user sessions.
  // TODO(crbug.com/40097732): Evaluate if this is can be solved in a
  // more elegant way.
  return UserNetworkConfigurationUpdaterAsh::CreateForUserPolicy(
      profile, *user, profile->GetProfilePolicyConnector()->policy_service(),
      ash::NetworkHandler::Get()->managed_network_configuration_handler());
}

}  // namespace policy
