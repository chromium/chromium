// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_TEST_SUPPORT_EMBEDDED_POLICY_TEST_SERVER_MIXIN_H_
#define CHROME_BROWSER_ASH_POLICY_TEST_SUPPORT_EMBEDDED_POLICY_TEST_SERVER_MIXIN_H_

#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>

#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"

class AccountId;

namespace policy {
class EmbeddedPolicyTestServer;
}

namespace ash {

class ScopedAttestationFlowFactoryForEnrollmentOverrideForTesting;

// This test mixin covers setting up EmbeddedPolicyTestServer and adding a
// command-line flag to use it. Please see SetUp function for default settings.
// Server is started after SetUp execution.
class EmbeddedPolicyTestServerMixin : public InProcessBrowserTestMixin {
 public:
  enum Capabilities {
    // Enables the usage of keys canned into the policy test server, instead of
    // the default key returned by PolicyBuilder::CreateTestSigningKey().
    ENABLE_CANNED_SIGNING_KEYS,
    // Enables the automatic rotation of the policy signing keys with each
    // policy fetch request.
    ENABLE_AUTOMATIC_ROTATION_OF_SIGNINGKEYS,
    // By default all users are considered managed by the server. If tests need
    // to handle both managed and unmanaged users, this capability would prevent
    // marking all users as managed, tests would need to indicate management
    // status separately.
    PER_USER_MANAGEMENT_STATUS
  };

  explicit EmbeddedPolicyTestServerMixin(
      InProcessBrowserTestMixinHost* host,
      std::initializer_list<Capabilities> capabilities = {});

  EmbeddedPolicyTestServerMixin(const EmbeddedPolicyTestServerMixin&) = delete;
  EmbeddedPolicyTestServerMixin& operator=(
      const EmbeddedPolicyTestServerMixin&) = delete;

  ~EmbeddedPolicyTestServerMixin() override;

  policy::EmbeddedPolicyTestServer* server() {
    return policy_test_server_.get();
  }

  // InProcessBrowserTestMixin:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // Updates the device policy blob served by the embedded policy test server.
  // This does not trigger policy invalidation, hence test authors must manually
  // trigger a policy fetch.
  void UpdateDevicePolicy(
      const enterprise_management::ChromeDeviceSettingsProto& policy);

  // Updates user policy blob served by the embedded policy test server.
  // `policy_user` - the policy user's email. This does not trigger policy
  // invalidation, hence test authors must manually trigger a policy fetch.
  void UpdateUserPolicy(
      const enterprise_management::CloudPolicySettings& policy,
      const std::string& policy_user);

  // Updates the timestamp returned by the server for policies. By default,
  // server returns current time as timestamp.
  void UpdatePolicyTimestamp(const base::Time& timestamp);

  // Updates policy selected by |type| and optional |entity_id|. The policy is
  // set to the proto serialized in |serialized_policy|. This does not trigger
  // policy invalidation, hence test authors must manually trigger a policy
  // fetch.
  void UpdatePolicy(const std::string& type,
                    const std::string& serialized_policy);
  void UpdatePolicy(const std::string& type,
                    const std::string& entity_id,
                    const std::string& serialized_policy);

  // Updates policy selected by |type| and optional |entity_id|. The
  // |raw_policy| is served via an external endpoint. This does not trigger
  // policy invalidation, hence test authors must manually trigger a policy
  // fetch.
  void UpdateExternalPolicy(const std::string& type,
                            const std::string& entity_id,
                            const std::string& raw_policy);

  // Configures whether the server should indicate that the client is
  // allowed to update device attributes in response to
  // DeviceAttributeUpdatePermissionRequest.
  void SetUpdateDeviceAttributesPermission(bool allowed);

  // Configures server to respond with particular error code during requests.
  // `net_error_code` - error code from device_management_service.cc.
  void SetDeviceEnrollmentError(int net_error_code);
  void SetDeviceAttributeUpdateError(int net_error_code);
  void SetPolicyFetchError(int net_error_code);

  // Configures fake attestation flow so that we can test attestation-based
  // enrollment flows.
  void SetFakeAttestationFlow();

  // Sets which types of licenses are possible to use for enrollment.
  void SetAvailableLicenses(bool has_enterpise_license, bool has_kiosk_license);

  // Sets market segment for the device, this information is provided via policy
  // metadata.
  void SetMarketSegment(
      enterprise_management::PolicyData::MarketSegment segment);
  // Sets metric segment for the user, this information is provided via policy
  // metadata.
  void SetMetricsLogSegment(
      enterprise_management::PolicyData::MetricsLogSegment segment);

  // Configures server to expect these PSM (private set membership) execution
  // values (i.e. `psm_execution_result` and `psm_determination_timestamp`) as
  // part of DeviceRegisterRequest. Note: `device_brand_code` and
  // `device_serial_number` values will be used on the server as a key to
  // retrieve the PSM execution values.
  void SetExpectedPsmParamsInDeviceRegisterRequest(
      const std::string& device_brand_code,
      const std::string& device_serial_number,
      int psm_execution_result,
      int64_t psm_determination_timestamp);

  // Set response for DeviceStateRetrievalRequest. Returns that if finds state
  // key passed in the request. State keys could be set by RegisterClient call
  // on policy test server.
  bool SetDeviceStateRetrievalResponse(
      policy::ServerBackedStateKeysBroker* keys_broker,
      enterprise_management::DeviceStateRetrievalResponse::RestoreMode
          restore_mode,
      const std::string& managemement_domain);

  // Set response for DeviceInitialEnrollmentStateRequest.
  void SetDeviceInitialEnrollmentResponse(
      const std::string& device_brand_code,
      const std::string& device_serial_number,
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          InitialEnrollmentMode initial_mode,
      const std::string& management_domain);

  // Utility function that configures server parameters for zero-touch
  // enrollment. Should be used in conjunction with enabling zero-touch
  // via command line and calling `ConfigureFakeStatisticsForZeroTouch`.
  void SetupZeroTouchForcedEnrollment();

  void SetEnrollmentNudgePolicy(bool enrollment_required);

  // Configures fake statistics provider with values that can be used with
  // zero-touch enrollment.
  void ConfigureFakeStatisticsForZeroTouch(
      system::ScopedFakeStatisticsProvider* provider);

  // Methods used in conjunction with PER_USER_MANAGEMENT_STATUS capability.
  // Allow to mark individual/all users as managed.
  void MarkUserAsManaged(const AccountId& account_id);
  void MarkAllUsersAsManaged();

 private:
  std::unique_ptr<policy::EmbeddedPolicyTestServer> policy_test_server_;
  base::flat_set<Capabilities> capabilities_;

  std::unique_ptr<ScopedAttestationFlowFactoryForEnrollmentOverrideForTesting>
      attestation_flow_override_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POLICY_TEST_SUPPORT_EMBEDDED_POLICY_TEST_SERVER_MIXIN_H_
