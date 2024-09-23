// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/alwayson_vpn_pre_connect_url_allowlist_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/net/alwayson_vpn_pre_connect_url_allowlist_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/user_manager/user_manager.h"
#include "components/variations/service/variations_service.h"
#include "content/public/browser/browser_context.h"

namespace ash {

// static
AlwaysOnVpnPreConnectUrlAllowlistServiceFactory*
AlwaysOnVpnPreConnectUrlAllowlistServiceFactory::GetInstance() {
  static base::NoDestructor<AlwaysOnVpnPreConnectUrlAllowlistServiceFactory>
      instance;
  return instance.get();
}

// static
AlwaysOnVpnPreConnectUrlAllowlistService*
AlwaysOnVpnPreConnectUrlAllowlistServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<AlwaysOnVpnPreConnectUrlAllowlistService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

void AlwaysOnVpnPreConnectUrlAllowlistServiceFactory::
    RecreateServiceInstanceForTesting(content::BrowserContext* context) {
  Disassociate(context);
}

AlwaysOnVpnPreConnectUrlAllowlistServiceFactory::
    AlwaysOnVpnPreConnectUrlAllowlistServiceFactory()
    : ProfileKeyedServiceFactory(
          "AlwaysOnVpnPreConnectUrlAllowlistService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

AlwaysOnVpnPreConnectUrlAllowlistServiceFactory::
    ~AlwaysOnVpnPreConnectUrlAllowlistServiceFactory() = default;

std::unique_ptr<KeyedService> AlwaysOnVpnPreConnectUrlAllowlistServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (!user_manager::UserManager::Get()->IsPrimaryUser(
          ash::BrowserContextHelper::Get()->GetUserByBrowserContext(context)) ||
      !profile->GetProfilePolicyConnector()->IsManaged()) {
    return nullptr;
  }
  return std::make_unique<AlwaysOnVpnPreConnectUrlAllowlistService>(profile);
}

bool AlwaysOnVpnPreConnectUrlAllowlistServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

void AlwaysOnVpnPreConnectUrlAllowlistServiceFactory::
    SetServiceIsNULLWhileTestingForTesting(bool service_is_null_while_testing) {
  service_is_null_while_testing_ = service_is_null_while_testing;
}

bool AlwaysOnVpnPreConnectUrlAllowlistServiceFactory::
    ServiceIsNULLWhileTesting() const {
  return service_is_null_while_testing_;
}

}  // namespace ash
