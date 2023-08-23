// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/profile_token_policy_web_signin_service_factory.h"

#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/cloud/profile_token_policy_web_signin_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace policy {

ProfileTokenPolicyWebSigninServiceFactory::
    ProfileTokenPolicyWebSigninServiceFactory()
    : ProfileKeyedServiceFactory("ProfileTokenPolicyWebSigninService") {}

ProfileTokenPolicyWebSigninServiceFactory::
    ~ProfileTokenPolicyWebSigninServiceFactory() = default;

// static
ProfileTokenPolicyWebSigninService*
ProfileTokenPolicyWebSigninServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<ProfileTokenPolicyWebSigninService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
ProfileTokenPolicyWebSigninServiceFactory*
ProfileTokenPolicyWebSigninServiceFactory::GetInstance() {
  static base::NoDestructor<ProfileTokenPolicyWebSigninServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<KeyedService> ProfileTokenPolicyWebSigninServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  Profile* profile = static_cast<Profile*>(context);
  BrowserPolicyConnector* connector =
      g_browser_process->browser_policy_connector();
  return std::make_unique<ProfileTokenPolicyWebSigninService>(
      profile, g_browser_process->local_state(),
      connector->device_management_service(),
      profile->GetProfileCloudPolicyManager(),
      g_browser_process->shared_url_loader_factory());
}

bool ProfileTokenPolicyWebSigninServiceFactory::
    ServiceIsCreatedWithBrowserContext() const {
  return true;
}

}  // namespace policy
