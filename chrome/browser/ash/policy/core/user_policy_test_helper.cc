// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/user_policy_test_helper.h"

#include <utility>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/cloud/cloud_policy_client.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/cloud_policy_core.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace policy {

UserPolicyTestHelper::UserPolicyTestHelper(
    const std::string& account_id,
    ash::EmbeddedPolicyTestServerMixin* embedded_policy_server)
    : account_id_(account_id),
      embedded_policy_server_(embedded_policy_server) {}

UserPolicyTestHelper::~UserPolicyTestHelper() {}

void UserPolicyTestHelper::SetPolicy(
    const enterprise_management::CloudPolicySettings& policy) {
  DCHECK(embedded_policy_server_);
  embedded_policy_server_->UpdateUserPolicy(policy, account_id_);
}

void UserPolicyTestHelper::WaitForInitialPolicy(Profile* profile) {
  BrowserPolicyConnector* const connector =
      g_browser_process->browser_policy_connector();
  connector->ScheduleServiceInitialization(0);

  UserCloudPolicyManagerAsh* const policy_manager =
      profile->GetUserCloudPolicyManagerAsh();
  DCHECK(!policy_manager->IsInitializationComplete(POLICY_DOMAIN_CHROME));

  // Give a bogus OAuth token to the |policy_manager|. This should make its
  // CloudPolicyClient fetch the DMToken.
  ASSERT_FALSE(policy_manager->core()->client()->is_registered());
  CloudPolicyClient::RegistrationParameters user_registration(
      enterprise_management::DeviceRegisterRequest::USER,
      enterprise_management::DeviceRegisterRequest::FLAVOR_USER_REGISTRATION);
  policy_manager->core()->client()->Register(
      user_registration, std::string() /* client_id */,
      "oauth_token_unused" /* oauth_token */);

  ProfilePolicyConnector* const profile_connector =
      profile->GetProfilePolicyConnector();
  PolicyService* const policy_service = profile_connector->policy_service();

  base::RunLoop run_loop;
  policy_service->RefreshPolicies(run_loop.QuitClosure(),
                                  PolicyFetchReason::kTest);
  run_loop.Run();
}

void UserPolicyTestHelper::SetPolicyAndWait(
    const enterprise_management::CloudPolicySettings& policy,
    Profile* profile) {
  SetPolicy(policy);
  RefreshPolicyAndWait(profile);
}

void UserPolicyTestHelper::RefreshPolicyAndWait(Profile* profile) {
  ProfilePolicyConnector* const profile_connector =
      profile->GetProfilePolicyConnector();
  PolicyService* const policy_service = profile_connector->policy_service();

  base::RunLoop run_loop;
  policy_service->RefreshPolicies(run_loop.QuitClosure(),
                                  PolicyFetchReason::kTest);
  run_loop.Run();
}

}  // namespace policy
