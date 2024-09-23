// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signin/user_policy_oidc_signin_service_factory.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/profile_management/profile_management_features.h"
#include "chrome/browser/enterprise/signin/user_policy_oidc_signin_service.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/profile_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

absl::variant<UserCloudPolicyManager*, ProfileCloudPolicyManager*>
GetCloudPolicyManager(Profile* profile) {
  auto* entry = g_browser_process->profile_manager()
                    ->GetProfileAttributesStorage()
                    .GetProfileAttributesWithPath(profile->GetPath());
  if (entry && entry->IsDasherlessManagement()) {
    return profile->GetProfileCloudPolicyManager();
  }
  return profile->GetUserCloudPolicyManager();
}

UserPolicyOidcSigninServiceFactory::UserPolicyOidcSigninServiceFactory()
    : ProfileKeyedServiceFactory(
          "UserPolicyOidcSigninService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

UserPolicyOidcSigninServiceFactory::~UserPolicyOidcSigninServiceFactory() =
    default;

// static
UserPolicyOidcSigninService* UserPolicyOidcSigninServiceFactory::GetForProfile(
    Profile* profile) {
  return (base::FeatureList::IsEnabled(
             profile_management::features::kOidcAuthProfileManagement))
             ? static_cast<UserPolicyOidcSigninService*>(
                   GetInstance()->GetServiceForBrowserContext(profile, true))
             : nullptr;
}

// static
UserPolicyOidcSigninServiceFactory*
UserPolicyOidcSigninServiceFactory::GetInstance() {
  static base::NoDestructor<UserPolicyOidcSigninServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService>
UserPolicyOidcSigninServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(
          profile_management::features::kOidcAuthProfileManagement)) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  DeviceManagementService* device_management_service =
      connector->device_management_service();

  // TODO(326188539): The attribute IsDasherlessManagement should be set
  // correctly before an OIDC-maanged profile is created. However, to support
  // the workflow where consent dialog and profile creation happens before DM
  // server registration, this policy signin service needs to be reactive when
  // the value of IsDasherlessManagement changes.
  return std::make_unique<UserPolicyOidcSigninService>(
      profile, g_browser_process->local_state(), device_management_service,
      GetCloudPolicyManager(profile),
      IdentityManagerFactory::GetForProfile(profile),
      g_browser_process->shared_url_loader_factory());
}

bool UserPolicyOidcSigninServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  // Create this object when the profile is created so it can track any
  // user signin activity.
  return true;
}

bool UserPolicyOidcSigninServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace policy
