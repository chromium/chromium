// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_policy_signin_service_factory.h"

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/policy/cloud/user_policy_signin_service_mobile.h"
#else
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#endif

namespace policy {

namespace {

// Used only for testing.
DeviceManagementService* g_device_management_service = nullptr;

}  // namespace

UserPolicySigninServiceFactory::UserPolicySigninServiceFactory()
    : ProfileKeyedServiceFactory(
          "UserPolicySigninService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

UserPolicySigninServiceFactory::~UserPolicySigninServiceFactory() = default;

// static
UserPolicySigninService* UserPolicySigninServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<UserPolicySigninService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
UserPolicySigninServiceFactory* UserPolicySigninServiceFactory::GetInstance() {
  static base::NoDestructor<UserPolicySigninServiceFactory> instance;
  return instance.get();
}

// static
void UserPolicySigninServiceFactory::SetDeviceManagementServiceForTesting(
    DeviceManagementService* device_management_service) {
  g_device_management_service = device_management_service;
}

std::unique_ptr<KeyedService>
UserPolicySigninServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  DeviceManagementService* device_management_service =
      g_device_management_service ? g_device_management_service
                                  : connector->device_management_service();

  return std::make_unique<UserPolicySigninService>(
      profile, g_browser_process->local_state(), device_management_service,
      profile->GetUserCloudPolicyManager(),
      IdentityManagerFactory::GetForProfile(profile),
      g_browser_process->shared_url_loader_factory());
}

bool
UserPolicySigninServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // Create this object when the profile is created so it can track any
  // user signin activity.
  return true;
}

void UserPolicySigninServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
#if BUILDFLAG(IS_ANDROID)
  user_prefs->RegisterInt64Pref(policy_prefs::kLastPolicyCheckTime, 0);
#endif
}

}  // namespace policy
