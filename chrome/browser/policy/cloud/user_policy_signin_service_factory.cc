// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_ANDROID)
#include "chrome/browser/policy/cloud/user_policy_signin_service_mobile.h"
#else
#include "chrome/browser/policy/cloud/user_policy_signin_service.h"
#endif

namespace policy {

namespace {

// Used only for testing.
DeviceManagementService* g_device_management_service = NULL;

}  // namespace

UserPolicySigninServiceFactory::UserPolicySigninServiceFactory()
    : BrowserContextKeyedServiceFactory(
        "UserPolicySigninService",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(IdentityManagerFactory::GetInstance());
}

UserPolicySigninServiceFactory::~UserPolicySigninServiceFactory() {}

// static
UserPolicySigninService* UserPolicySigninServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<UserPolicySigninService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
UserPolicySigninServiceFactory* UserPolicySigninServiceFactory::GetInstance() {
  return base::Singleton<UserPolicySigninServiceFactory>::get();
}

// static
void UserPolicySigninServiceFactory::SetDeviceManagementServiceForTesting(
    DeviceManagementService* device_management_service) {
  g_device_management_service = device_management_service;
}

KeyedService* UserPolicySigninServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  DeviceManagementService* device_management_service =
      g_device_management_service ? g_device_management_service
                                  : connector->device_management_service();

  UserPolicySigninService* service = new UserPolicySigninService(
      profile, g_browser_process->local_state(), device_management_service,
      profile->GetUserCloudPolicyManager(),
      IdentityManagerFactory::GetForProfile(profile),
      g_browser_process->shared_url_loader_factory());
  return service;
}

bool
UserPolicySigninServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // Create this object when the profile is created so it can track any
  // user signin activity.
  return true;
}

void UserPolicySigninServiceFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* user_prefs) {
#if defined(OS_ANDROID)
  user_prefs->RegisterInt64Pref(prefs::kLastPolicyCheckTime, 0);
#endif
}

}  // namespace policy
