// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/user_cloud_policy_token_forwarder_factory.h"

#include "chrome/browser/chromeos/policy/user_cloud_policy_manager_chromeos.h"
#include "chrome/browser/chromeos/policy/user_cloud_policy_token_forwarder.h"
#include "chrome/browser/chromeos/policy/user_policy_manager_factory_chromeos.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "services/identity/public/cpp/identity_manager.h"

namespace policy {

// static
UserCloudPolicyTokenForwarderFactory*
    UserCloudPolicyTokenForwarderFactory::GetInstance() {
  return base::Singleton<UserCloudPolicyTokenForwarderFactory>::get();
}

UserCloudPolicyTokenForwarderFactory::UserCloudPolicyTokenForwarderFactory()
    : BrowserContextKeyedServiceFactory(
        "UserCloudPolicyTokenForwarder",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
  DependsOn(UserPolicyManagerFactoryChromeOS::GetInstance());
}

UserCloudPolicyTokenForwarderFactory::~UserCloudPolicyTokenForwarderFactory() {}

KeyedService* UserCloudPolicyTokenForwarderFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  UserCloudPolicyManagerChromeOS* manager =
      UserPolicyManagerFactoryChromeOS::GetCloudPolicyManagerForProfile(
          profile);
  identity::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  if (!manager || !identity_manager)
    return nullptr;
  return new UserCloudPolicyTokenForwarder(manager, identity_manager);
}

bool UserCloudPolicyTokenForwarderFactory::
ServiceIsCreatedWithBrowserContext() const {
  // Create this object when the profile is created so it fetches the token
  // during startup.
  return true;
}

bool UserCloudPolicyTokenForwarderFactory::ServiceIsNULLWhileTesting() const {
  // This is only needed in a handful of tests that manually create the object.
  return true;
}

}  // namespace policy
