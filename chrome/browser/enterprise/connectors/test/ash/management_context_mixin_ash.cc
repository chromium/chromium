// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/test/ash/management_context_mixin_ash.h"

#include <array>
#include <utility>

#include "base/check.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/enterprise/connectors/test/test_constants.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/policy/core/common/cloud/dm_token.h"

namespace enterprise_connectors::test {

ManagementContextMixinAsh::ManagementContextMixinAsh(
    InProcessBrowserTestMixinHost* host,
    InProcessBrowserTest* test_base,
    ManagementContext management_context)
    : ManagementContextMixin(host, test_base, std::move(management_context)),
      device_state_mixin_(
          host,
          management_context.is_cloud_machine_managed
              ? ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED
              : ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED) {}

ManagementContextMixinAsh::~ManagementContextMixinAsh() = default;

std::unique_ptr<ash::ScopedDevicePolicyUpdate>
ManagementContextMixinAsh::RequestDevicePolicyUpdate() {
  return device_state_mixin_.RequestDevicePolicyUpdate();
}

void ManagementContextMixinAsh::ManageCloudUser() {
  ManagementContextMixin::ManageCloudUser();
  auto* profile_policy_manager =
      browser()->profile()->GetUserCloudPolicyManagerAsh();
  profile_policy_manager->core()->client()->SetupRegistration(
      kProfileDmToken, kProfileClientId, {});
  profile_policy_manager->core()->store()->set_policy_data_for_testing(
      GetBaseUserPolicyData());
}

void ManagementContextMixinAsh::SetUpOnMainThread() {
  ManagementContextMixin::SetUpOnMainThread();
  if (management_context_.is_cloud_user_managed) {
    ManageCloudUser();
  }
}

void ManagementContextMixinAsh::ManageCloudMachine() {
  ManagementContextMixin::ManageCloudMachine();
  policy::SetDMTokenForTesting(
      policy::DMToken::CreateValidToken(kDeviceDmToken));

  auto device_policy_update = RequestDevicePolicyUpdate();
  device_policy_update->policy_data()->add_device_affiliation_ids(
      kFakeCustomerId);
  device_policy_update->policy_data()->set_obfuscated_customer_id(
      kFakeCustomerId);
}

}  // namespace enterprise_connectors::test
