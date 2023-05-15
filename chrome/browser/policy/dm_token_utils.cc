// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/dm_token_utils.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "components/user_manager/user.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_loader_lacros.h"
#else
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/browser/controller/chrome_browser_cloud_management_controller.h"
#endif

namespace policy {

namespace {

DMToken* GetTestingDMTokenStorage() {
  static base::NoDestructor<DMToken> dm_token(DMToken::CreateEmptyToken());
  return dm_token.get();
}

}  // namespace

DMToken GetDMToken(Profile* const profile) {
  DMToken dm_token = *GetTestingDMTokenStorage();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (!profile)
    return dm_token;

  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user)
    return dm_token;

  CloudPolicyManager* policy_manager;
  if (user->IsDeviceLocalAccount()) {
    // Policy Manager for Device DM Token (Kiosk and Managed Guest Session).
    policy::BrowserPolicyConnectorAsh* connector =
        g_browser_process->platform_part()->browser_policy_connector_ash();
    DCHECK(connector);
    policy_manager = connector->GetDeviceCloudPolicyManager();
  } else {
    // Policy Manager for User DM Token.
    policy_manager = profile->GetUserCloudPolicyManagerAsh();
  }

  if (dm_token.is_empty() && policy_manager &&
      policy_manager->IsClientRegistered()) {
    dm_token =
        DMToken::CreateValidToken(policy_manager->core()->client()->dm_token());
  }
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!profile)
    return dm_token;

  if (profile->IsMainProfile()) {
    const enterprise_management::PolicyData* policy =
        policy::PolicyLoaderLacros::main_user_policy_data();
    if (dm_token.is_empty() && policy && policy->has_request_token() &&
        !policy->request_token().empty()) {
      dm_token = DMToken::CreateValidToken(policy->request_token());
    }
  } else {
    UserCloudPolicyManager* policy_manager =
        profile->GetUserCloudPolicyManager();
    if (dm_token.is_empty() && policy_manager &&
        policy_manager->IsClientRegistered()) {
      dm_token = DMToken::CreateValidToken(
          policy_manager->core()->client()->dm_token());
    }
  }
#elif !BUILDFLAG(IS_ANDROID)
  if (dm_token.is_empty() &&
      ChromeBrowserCloudManagementController::IsEnabled()) {
    dm_token = BrowserDMTokenStorage::Get()->RetrieveDMToken();
  }
#endif

  return dm_token;
}

void SetDMTokenForTesting(const DMToken& dm_token) {
  *GetTestingDMTokenStorage() = dm_token;
}

}  // namespace policy
