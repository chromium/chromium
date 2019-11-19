// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"

#include <utility>

#include "base/guid.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/test/fake_gaia_mixin.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/cryptohome/fake_cryptohome_client.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/policy_switches.h"

namespace chromeos {

namespace {

base::Value GetDefaultConfig() {
  base::Value config(base::Value::Type::DICTIONARY);

  base::Value managed_users(base::Value::Type::LIST);
  managed_users.Append("*");
  config.SetKey("managed_users", std::move(managed_users));

  config.SetKey("robot_api_auth_code",
                base::Value(FakeGaiaMixin::kFakeAuthCode));

  return config;
}

}  // namespace

LocalPolicyTestServerMixin::LocalPolicyTestServerMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {
  server_config_ = GetDefaultConfig();
}

void LocalPolicyTestServerMixin::SetUp() {
  policy_test_server_ = std::make_unique<policy::LocalPolicyTestServer>();
  policy_test_server_->SetConfig(server_config_);
  policy_test_server_->RegisterClient(policy::PolicyBuilder::kFakeToken,
                                      policy::PolicyBuilder::kFakeDeviceId,
                                      {} /* state_keys */);

  if (!canned_signing_keys_enabled_) {
    CHECK(policy_test_server_->SetSigningKeyAndSignature(
        policy::PolicyBuilder::CreateTestSigningKey().get(),
        policy::PolicyBuilder::GetTestSigningKeySignature()));
  }

  if (automatic_rotation_of_signing_keys_enabled_)
    policy_test_server_->EnableAutomaticRotationOfSigningKeys();

  CHECK(policy_test_server_->Start());
}

void LocalPolicyTestServerMixin::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Specify device management server URL.
  command_line->AppendSwitchASCII(policy::switches::kDeviceManagementUrl,
                                  policy_test_server_->GetServiceURL().spec());
}

void LocalPolicyTestServerMixin::ExpectAvailableLicenseCount(int perpetual,
                                                             int annual,
                                                             int kiosk) {
  base::Value licenses(base::Value::Type::DICTIONARY);
  if (perpetual >= 0) {
    licenses.SetKey("perpetual", base::Value(perpetual));
  }
  if (annual >= 0) {
    licenses.SetKey("annual", base::Value(annual));
  }
  if (kiosk >= 0) {
    licenses.SetKey("kiosk", base::Value(kiosk));
  }
  DCHECK(licenses.DictSize() > 0);

  server_config_.SetKey("available_licenses", std::move(licenses));
  policy_test_server_->SetConfig(server_config_);
}

void LocalPolicyTestServerMixin::ExpectTokenEnrollment(
    const std::string& enrollment_token,
    const std::string& token_creator) {
  base::Value token_enrollment(base::Value::Type::DICTIONARY);
  token_enrollment.SetKey("token", base::Value(enrollment_token));
  token_enrollment.SetKey("username", base::Value(token_creator));
  server_config_.SetKey("token_enrollment", std::move(token_enrollment));
  policy_test_server_->SetConfig(server_config_);
}

void LocalPolicyTestServerMixin::SetUpdateDeviceAttributesPermission(
    bool allowed) {
  server_config_.SetKey("allow_set_device_attributes", base::Value(allowed));
  policy_test_server_->SetConfig(server_config_);
}

void LocalPolicyTestServerMixin::SetExpectedDeviceEnrollmentError(
    int net_error_code) {
  server_config_.SetPath({"request_errors", "register"},
                         base::Value(net_error_code));
  policy_test_server_->SetConfig(server_config_);
}

void LocalPolicyTestServerMixin::SetExpectedDeviceAttributeUpdateError(
    int net_error_code) {
  server_config_.SetPath({"request_errors", "device_attribute_update"},
                         base::Value(net_error_code));
  policy_test_server_->SetConfig(server_config_);
}

void LocalPolicyTestServerMixin::SetExpectedPolicyFetchError(
    int net_error_code) {
  server_config_.SetPath({"request_errors", "policy"},
                         base::Value(net_error_code));
  policy_test_server_->SetConfig(server_config_);
}

bool LocalPolicyTestServerMixin::UpdateDevicePolicy(
    const enterprise_management::ChromeDeviceSettingsProto& policy) {
  DCHECK(policy_test_server_);
  return policy_test_server_->UpdatePolicy(
      policy::dm_protocol::kChromeDevicePolicyType,
      std::string() /* entity_id */, policy.SerializeAsString());
}

bool LocalPolicyTestServerMixin::UpdateUserPolicy(
    const enterprise_management::CloudPolicySettings& policy,
    const std::string& policy_user) {
  // Configure the test server's policy user. This will ensure the desired
  // username is set in policy responses, even if the request does not contain
  // username field.
  base::Value managed_users_list(base::Value::Type::LIST);
  managed_users_list.Append("*");
  server_config_.SetKey("managed_users", std::move(managed_users_list));
  server_config_.SetKey("policy_user", base::Value(policy_user));
  server_config_.SetKey("current_key_index", base::Value(0));
  if (!policy_test_server_->SetConfig(server_config_))
    return false;

  // Update the policy that should be served for the user.
  return policy_test_server_->UpdatePolicy(
      policy::dm_protocol::kChromeUserPolicyType, std::string() /* entity_id */,
      policy.SerializeAsString());
}

bool LocalPolicyTestServerMixin::UpdateUserPolicy(
    const base::Value& mandatory_policy,
    const base::Value& recommended_policy,
    const std::string& policy_user) {
  DCHECK(policy_test_server_);
  base::Value policy_type_dict(base::Value::Type::DICTIONARY);
  policy_type_dict.SetKey("mandatory", mandatory_policy.Clone());
  policy_type_dict.SetKey("recommended", recommended_policy.Clone());

  base::Value managed_users_list(base::Value::Type::LIST);
  managed_users_list.Append("*");

  server_config_.SetKey(policy::dm_protocol::kChromeUserPolicyType,
                        std::move(policy_type_dict));
  server_config_.SetKey("managed_users", std::move(managed_users_list));
  server_config_.SetKey("policy_user", base::Value(policy_user));
  server_config_.SetKey("current_key_index", base::Value(0));
  return policy_test_server_->SetConfig(server_config_);
}

void LocalPolicyTestServerMixin::SetFakeAttestationFlow() {
  g_browser_process->platform_part()
      ->browser_policy_connector_chromeos()
      ->GetDeviceCloudPolicyInitializer()
      ->SetAttestationFlowForTesting(
          std::make_unique<chromeos::attestation::AttestationFlow>(
              cryptohome::AsyncMethodCaller::GetInstance(),
              chromeos::FakeCryptohomeClient::Get(),
              std::make_unique<chromeos::attestation::FakeServerProxy>()));
}

bool LocalPolicyTestServerMixin::SetDeviceStateRetrievalResponse(
    policy::ServerBackedStateKeysBroker* keys_broker,
    enterprise_management::DeviceStateRetrievalResponse::RestoreMode
        restore_mode,
    const std::string& managemement_domain) {
  std::vector<std::string> keys;
  base::RunLoop loop;
  keys_broker->RequestStateKeys(base::BindOnce(
      [](std::vector<std::string>* keys, base::OnceClosure quit,
         const std::vector<std::string>& state_keys) {
        *keys = state_keys;
        std::move(quit).Run();
      },
      &keys, loop.QuitClosure()));
  loop.Run();
  if (keys.empty())
    return false;
  if (!policy_test_server_->RegisterClient("dm_token", base::GenerateGUID(),
                                           keys)) {
    return false;
  }

  base::Value device_state(base::Value::Type::DICTIONARY);
  device_state.SetKey("management_domain", base::Value(managemement_domain));
  device_state.SetKey("restore_mode",
                      base::Value(static_cast<int>(restore_mode)));
  server_config_.SetKey("device_state", std::move(device_state));
  return policy_test_server_->SetConfig(server_config_);
}

bool LocalPolicyTestServerMixin::SetDeviceInitialEnrollmentResponse(
    const std::string& device_brand_code,
    const std::string& device_serial_number,
    enterprise_management::DeviceInitialEnrollmentStateResponse::
        InitialEnrollmentMode initial_mode,
    const base::Optional<std::string>& management_domain,
    const base::Optional<bool> is_license_packaged_with_device) {
  base::Value serial_entry(base::Value::Type::DICTIONARY);
  serial_entry.SetKey("initial_enrollment_mode", base::Value(initial_mode));

  if (management_domain.has_value())
    serial_entry.SetKey("management_domain",
                        base::Value(management_domain.value()));

  if (is_license_packaged_with_device.has_value())
    serial_entry.SetKey("is_license_packaged_with_device",
                        base::Value(is_license_packaged_with_device.value()));

  const std::string brand_serial_id =
      device_brand_code + "_" + device_serial_number;
  server_config_.SetPath("initial_enrollment_state." + brand_serial_id,
                         std::move(serial_entry));
  policy_test_server_->SetConfig(server_config_);
  return true;
}

void LocalPolicyTestServerMixin::SetupZeroTouchForcedEnrollment() {
  SetFakeAttestationFlow();
  auto initial_enrollment =
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED;
  SetUpdateDeviceAttributesPermission(false);
  SetDeviceInitialEnrollmentResponse(
      test::kTestRlzBrandCodeKey, test::kTestSerialNumber, initial_enrollment,
      test::kTestDomain, base::nullopt /* is_license_packaged_with_device */);
}

void LocalPolicyTestServerMixin::ConfigureFakeStatisticsForZeroTouch(
    system::ScopedFakeStatisticsProvider* provider) {
  provider->SetMachineStatistic(system::kRlzBrandCodeKey,
                                test::kTestRlzBrandCodeKey);
  provider->SetMachineStatistic(system::kSerialNumberKeyForTest,
                                test::kTestSerialNumber);
  provider->SetMachineStatistic(system::kHardwareClassKey,
                                test::kTestHardwareClass);
}

void LocalPolicyTestServerMixin::EnableCannedSigningKeys() {
  DCHECK(!policy_test_server_);
  canned_signing_keys_enabled_ = true;
}

void LocalPolicyTestServerMixin::EnableAutomaticRotationOfSigningKeys() {
  DCHECK(!policy_test_server_);
  automatic_rotation_of_signing_keys_enabled_ = true;
}

LocalPolicyTestServerMixin::~LocalPolicyTestServerMixin() = default;

}  // namespace chromeos
