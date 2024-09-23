// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"

#include <cstdint>
#include <initializer_list>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "chrome/browser/ash/login/enrollment/enrollment_launcher.h"
#include "chrome/browser/ash/policy/enrollment/device_cloud_policy_initializer.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chrome/browser/ash/policy/test_support/policy_test_server_constants.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/attestation/fake_attestation_flow.h"
#include "chromeos/ash/components/attestation/fake_certificate.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/account_id/account_id.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/policy_switches.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/client_storage.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/policy_storage.h"
#include "components/policy/test_support/signature_provider.h"
#include "net/http/http_status_code.h"

namespace ash {

namespace {

std::string GetBrandSerialId(const std::string& device_brand_code,
                             const std::string& device_serial_number) {
  return device_brand_code + "_" + device_serial_number;
}

std::unique_ptr<ash::attestation::AttestationFlow> CreateFakeAttestationFlow() {
  std::string valid_certificate;
  attestation::GetFakeCertificatePEM(/*expiry=*/base::Days(10),
                                     &valid_certificate);

  return std::make_unique<attestation::FakeAttestationFlow>(
      std::move(valid_certificate));
}

}  // namespace

EmbeddedPolicyTestServerMixin::EmbeddedPolicyTestServerMixin(
    InProcessBrowserTestMixinHost* host,
    std::initializer_list<Capabilities> capabilities)
    : InProcessBrowserTestMixin(host), capabilities_(capabilities) {}

EmbeddedPolicyTestServerMixin::~EmbeddedPolicyTestServerMixin() = default;

void EmbeddedPolicyTestServerMixin::SetUp() {
  InProcessBrowserTestMixin::SetUp();
  policy_test_server_ = std::make_unique<policy::EmbeddedPolicyTestServer>();
  policy_test_server_->policy_storage()->set_robot_api_auth_code(
      FakeGaiaMixin::kFakeAuthCode);
  if (!capabilities_.contains(PER_USER_MANAGEMENT_STATUS)) {
    policy_test_server_->policy_storage()->add_managed_user("*");
  }

  if (!capabilities_.contains(ENABLE_CANNED_SIGNING_KEYS)) {
    // Create universal signing keys that can sign any domain.
    policy_test_server_->policy_storage()
        ->signature_provider()
        ->SetUniversalSigningKeys();
  }

  if (capabilities_.contains(ENABLE_AUTOMATIC_ROTATION_OF_SIGNINGKEYS)) {
    policy_test_server_->policy_storage()
        ->signature_provider()
        ->set_rotate_keys(true);
  }

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

  // This will change the verification key to be used by the
  // CloudPolicyValidator. It will allow for the policy provided by the
  // PolicyBuilder to pass the signature validation.
  command_line->AppendSwitchASCII(
      policy::switches::kPolicyVerificationKey,
      policy::PolicyBuilder::GetEncodedPolicyVerificationKey());
}

void EmbeddedPolicyTestServerMixin::UpdateDevicePolicy(
    const enterprise_management::ChromeDeviceSettingsProto& policy) {
  UpdatePolicy(policy::dm_protocol::kChromeDevicePolicyType,
               policy.SerializeAsString());
}

void EmbeddedPolicyTestServerMixin::UpdateUserPolicy(
    const enterprise_management::CloudPolicySettings& policy,
    const std::string& policy_user) {
  policy_test_server_->policy_storage()->set_policy_user(policy_user);
  UpdatePolicy(policy::dm_protocol::kChromeUserPolicyType,
               policy.SerializeAsString());
}

void EmbeddedPolicyTestServerMixin::UpdatePolicyTimestamp(
    const base::Time& timestamp) {
  policy_test_server_->policy_storage()->set_timestamp(timestamp);
}

void EmbeddedPolicyTestServerMixin::UpdatePolicy(
    const std::string& type,
    const std::string& serialized_policy) {
  UpdatePolicy(type, std::string(), serialized_policy);
}

void EmbeddedPolicyTestServerMixin::UpdatePolicy(
    const std::string& type,
    const std::string& entity_id,
    const std::string& serialized_policy) {
  policy_test_server_->policy_storage()->SetPolicyPayload(type, entity_id,
                                                          serialized_policy);
}

void EmbeddedPolicyTestServerMixin::UpdateExternalPolicy(
    const std::string& type,
    const std::string& entity_id,
    const std::string& raw_policy) {
  policy_test_server_->UpdateExternalPolicy(type, entity_id, raw_policy);
}

void EmbeddedPolicyTestServerMixin::SetUpdateDeviceAttributesPermission(
    bool allowed) {
  policy_test_server_->policy_storage()->set_allow_set_device_attributes(
      allowed);
}

void EmbeddedPolicyTestServerMixin::SetDeviceEnrollmentError(
    int net_error_code) {
  policy_test_server_->ConfigureRequestError(
      policy::dm_protocol::kValueRequestRegister,
      static_cast<net::HttpStatusCode>(net_error_code));
}

void EmbeddedPolicyTestServerMixin::SetDeviceAttributeUpdateError(
    int net_error_code) {
  policy_test_server_->ConfigureRequestError(
      policy::dm_protocol::kValueRequestDeviceAttributeUpdate,
      static_cast<net::HttpStatusCode>(net_error_code));
}

void EmbeddedPolicyTestServerMixin::SetPolicyFetchError(int net_error_code) {
  policy_test_server_->ConfigureRequestError(
      policy::dm_protocol::kValueRequestPolicy,
      static_cast<net::HttpStatusCode>(net_error_code));
}

void EmbeddedPolicyTestServerMixin::SetFakeAttestationFlow() {
  auto factory = base::BindRepeating(CreateFakeAttestationFlow);
  attestation_flow_override_ = std::make_unique<
      ScopedAttestationFlowFactoryForEnrollmentOverrideForTesting>(
      std::move(factory));
}

void EmbeddedPolicyTestServerMixin::SetAvailableLicenses(
    bool has_enterpise_license,
    bool has_kiosk_license) {
  policy_test_server_->policy_storage()->set_has_enterprise_license(
      has_enterpise_license);
  policy_test_server_->policy_storage()->set_has_kiosk_license(
      has_kiosk_license);
}

void EmbeddedPolicyTestServerMixin::SetMarketSegment(
    enterprise_management::PolicyData::MarketSegment segment) {
  policy_test_server_->policy_storage()->set_market_segment(segment);
}

void EmbeddedPolicyTestServerMixin::SetMetricsLogSegment(
    enterprise_management::PolicyData::MetricsLogSegment segment) {
  policy_test_server_->policy_storage()->set_metrics_log_segment(segment);
}

void EmbeddedPolicyTestServerMixin::SetExpectedPsmParamsInDeviceRegisterRequest(
    const std::string& device_brand_code,
    const std::string& device_serial_number,
    int psm_execution_result,
    int64_t psm_determination_timestamp) {
  policy::PolicyStorage::PsmEntry psm_entry;
  psm_entry.psm_execution_result = psm_execution_result;
  psm_entry.psm_determination_timestamp = psm_determination_timestamp;
  policy_test_server_->policy_storage()->SetPsmEntry(
      GetBrandSerialId(device_brand_code, device_serial_number), psm_entry);
}

bool EmbeddedPolicyTestServerMixin::SetDeviceStateRetrievalResponse(
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
  if (keys.empty()) {
    return false;
  }

  policy::ClientStorage::ClientInfo client_info;
  client_info.device_token = "dm_token";
  client_info.device_id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  client_info.state_keys = keys;
  policy_test_server_->client_storage()->RegisterClient(client_info);
  policy_test_server_->policy_storage()->set_device_state(
      policy::PolicyStorage::DeviceState{managemement_domain, restore_mode});
  return true;
}

void EmbeddedPolicyTestServerMixin::SetDeviceInitialEnrollmentResponse(
    const std::string& device_brand_code,
    const std::string& device_serial_number,
    enterprise_management::DeviceInitialEnrollmentStateResponse::
        InitialEnrollmentMode initial_mode,
    const std::string& management_domain) {
  policy::PolicyStorage::InitialEnrollmentState initial_enrollment_state;
  initial_enrollment_state.initial_enrollment_mode = initial_mode;
  initial_enrollment_state.management_domain = management_domain;
  policy_test_server_->policy_storage()->SetInitialEnrollmentState(
      GetBrandSerialId(device_brand_code, device_serial_number),
      initial_enrollment_state);
}

void EmbeddedPolicyTestServerMixin::SetupZeroTouchForcedEnrollment() {
  SetFakeAttestationFlow();
  auto initial_enrollment =
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          INITIAL_ENROLLMENT_MODE_ZERO_TOUCH_ENFORCED;
  SetUpdateDeviceAttributesPermission(false);
  SetDeviceInitialEnrollmentResponse(test::kTestRlzBrandCodeKey,
                                     test::kTestSerialNumber,
                                     initial_enrollment, test::kTestDomain);
}

void EmbeddedPolicyTestServerMixin::SetEnrollmentNudgePolicy(
    bool enrollment_required) {
  policy_test_server_->policy_storage()->set_enrollment_required(
      enrollment_required);
}

void EmbeddedPolicyTestServerMixin::ConfigureFakeStatisticsForZeroTouch(
    system::ScopedFakeStatisticsProvider* provider) {
  provider->SetMachineStatistic(system::kRlzBrandCodeKey,
                                test::kTestRlzBrandCodeKey);
  provider->SetMachineStatistic(system::kSerialNumberKey,
                                test::kTestSerialNumber);
  provider->SetMachineStatistic(system::kHardwareClassKey,
                                test::kTestHardwareClass);
  provider->SetVpdStatus(system::StatisticsProvider::VpdStatus::kValid);
}

void EmbeddedPolicyTestServerMixin::MarkUserAsManaged(
    const AccountId& account_id) {
  CHECK(capabilities_.contains(PER_USER_MANAGEMENT_STATUS))
      << "Without PER_USER_MANAGEMENT_STATUS capability all users are "
         "considered managed";
  policy_test_server_->policy_storage()->add_managed_user(
      account_id.GetUserEmail());
}

void EmbeddedPolicyTestServerMixin::MarkAllUsersAsManaged() {
  CHECK(capabilities_.contains(PER_USER_MANAGEMENT_STATUS))
      << "Without PER_USER_MANAGEMENT_STATUS capability all users are "
         "considered managed";
  policy_test_server_->policy_storage()->add_managed_user("*");
}

}  // namespace ash
