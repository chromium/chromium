// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/embedded_policy_test_server_mixin.h"

#include <string>
#include <utility>

#include "base/guid.h"
#include "base/json/values_util.h"
#include "base/values.h"
#include "chrome/browser/ash/login/test/fake_gaia_mixin.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/enrollment/device_cloud_policy_initializer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/signature_provider.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

EmbeddedPolicyTestServerMixin::EmbeddedPolicyTestServerMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {}

EmbeddedPolicyTestServerMixin::~EmbeddedPolicyTestServerMixin() = default;

void EmbeddedPolicyTestServerMixin::SetUp() {
  InProcessBrowserTestMixin::SetUp();
  policy_test_server_ = std::make_unique<policy::EmbeddedPolicyTestServer>();
  policy_test_server_->policy_storage()->set_robot_api_auth_code(
      FakeGaiaMixin::kFakeAuthCode);
  policy_test_server_->policy_storage()->add_managed_user("*");

  // Create universal signing keys that can sign any domain.
  std::vector<policy::SignatureProvider::SigningKey> universal_signing_keys;
  universal_signing_keys.push_back(policy::SignatureProvider::SigningKey(
      policy::PolicyBuilder::CreateTestSigningKey(),
      {{"*", policy::PolicyBuilder::GetTestSigningKeySignature()}}));
  policy_test_server_->policy_storage()->signature_provider()->set_signing_keys(
      std::move(universal_signing_keys));

  // Register default user used in many tests.
  policy::ClientStorage::ClientInfo client_info;
  client_info.device_id = policy::PolicyBuilder::kFakeDeviceId;
  client_info.device_token = policy::PolicyBuilder::kFakeToken;
  client_info.allowed_policy_types = {
      policy::dm_protocol::kChromeDevicePolicyType,
      policy::dm_protocol::kChromeUserPolicyType,
      policy::dm_protocol::kChromePublicAccountPolicyType,
      policy::dm_protocol::kChromeExtensionPolicyType,
      policy::dm_protocol::kChromeSigninExtensionPolicyType,
      policy::dm_protocol::kChromeMachineLevelUserCloudPolicyType,
      policy::dm_protocol::kChromeMachineLevelExtensionCloudPolicyType};
  policy_test_server_->client_storage()->RegisterClient(client_info);

  CHECK(policy_test_server_->Start());
}

void EmbeddedPolicyTestServerMixin::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Specify device management server URL.
  command_line->AppendSwitchASCII(policy::switches::kDeviceManagementUrl,
                                  policy_test_server_->GetServiceURL().spec());
}

void EmbeddedPolicyTestServerMixin::UpdateUserPolicy(
    const enterprise_management::CloudPolicySettings& policy,
    const std::string& policy_user) {
  policy_test_server_->policy_storage()->set_policy_user(policy_user);
  policy_test_server_->policy_storage()->SetPolicyPayload(
      policy::dm_protocol::kChromeUserPolicyType, policy.SerializeAsString());
}

}  // namespace ash
