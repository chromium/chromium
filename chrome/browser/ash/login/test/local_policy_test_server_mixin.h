// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_LOCAL_POLICY_TEST_SERVER_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_LOCAL_POLICY_TEST_SERVER_MIXIN_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
// TODO(https://crbug.com/1164001): move to forward declaration.
#include "chromeos/system/fake_statistics_provider.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/local_policy_test_server.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// This test mixin covers setting up LocalPolicyTestServer and adding a
// command-line flag to use it. Please see SetUp function for default settings.
// Server is started after SetUp execution.
class LocalPolicyTestServerMixin : public InProcessBrowserTestMixin {
 public:
  explicit LocalPolicyTestServerMixin(InProcessBrowserTestMixinHost* host);

  LocalPolicyTestServerMixin(const LocalPolicyTestServerMixin&) = delete;
  LocalPolicyTestServerMixin& operator=(const LocalPolicyTestServerMixin&) =
      delete;

  ~LocalPolicyTestServerMixin() override;

  policy::LocalPolicyTestServer* server() { return policy_test_server_.get(); }

  // InProcessBrowserTestMixin:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // Updates device policy blob served by the local policy test server.
  bool UpdateDevicePolicy(
      const enterprise_management::ChromeDeviceSettingsProto& policy);

  // Updates user policy blob served by the local policy test server.
  // `policy_user` - the policy user's email.
  bool UpdateUserPolicy(
      const enterprise_management::CloudPolicySettings& policy,
      const std::string& policy_user);

  // Updates user policies served by the local policy test server, by
  // configuring ser of mandatory and recommended policies that should be
  // returned for the policy user.
  // `policy_user` - the policy user's email.
  bool UpdateUserPolicy(const base::Value& mandatory_policy,
                        const base::Value& recommended_policy,
                        const std::string& policy_user);

  // Sets a fixed timestamp for the user policy served by the local policy
  // server. By default the timestamp is set to current time on each request.
  bool UpdateUserPolicyTimestamp(const base::Time& timestamp);

  void SetUpdateDeviceAttributesPermission(bool allowed);

  // Configures fake attestation flow so that we can test attestation-based
  // enrollment flows.
  void SetFakeAttestationFlow();

  // Configures server to respond with particular error code during requests.
  // `net_error_code` - error code from device_management_service.cc.
  void SetExpectedDeviceEnrollmentError(int net_error_code);
  void SetExpectedDeviceAttributeUpdateError(int net_error_code);
  void SetExpectedPolicyFetchError(int net_error_code);

  // Configures server to expect these optional PSM (private set membership)
  // execution values (i.e. `psm_execution_result` and
  // `psm_determination_timestamp`) as part of DeviceRegisterRequest.
  // Note: `device_brand_code` and `device_serial_number` values will be used on
  // the server as a key to retrieve the PSM execution values.
  void SetExpectedPsmParamsInDeviceRegisterRequest(
      const std::string& device_brand_code,
      const std::string& device_serial_number,
      int psm_execution_result,
      const absl::optional<int64_t> psm_determination_timestamp);

  // Set response for DeviceStateRetrievalRequest. Returns that if finds state
  // key passed in the request. State keys could be set by RegisterClient call
  // on policy test server.
  bool SetDeviceStateRetrievalResponse(
      policy::ServerBackedStateKeysBroker* keys_broker,
      enterprise_management::DeviceStateRetrievalResponse::RestoreMode
          restore_mode,
      const std::string& managemement_domain);

  bool SetDeviceInitialEnrollmentResponse(
      const std::string& device_brand_code,
      const std::string& device_serial_number,
      enterprise_management::DeviceInitialEnrollmentStateResponse::
          InitialEnrollmentMode initial_mode,
      const absl::optional<std::string>& management_domain);

  // Utility function that configures server parameters for zero-touch
  // enrollment. Should be used in conjunction with enabling zero-touch
  // via command line and calling `ConfigureFakeStatisticsForZeroTouch`.
  void SetupZeroTouchForcedEnrollment();

  // Configures fake statistics provider with values that can be used with
  // zero-touch enrollment.
  void ConfigureFakeStatisticsForZeroTouch(
      system::ScopedFakeStatisticsProvider* provider);

  // Enables the usage of keys canned into the policy test server, instead of
  // specifying to it the test key to be used.  This must be called before
  // starting the server.
  void EnableCannedSigningKeys();

  // Enables the automatic rotation of the policy signing keys with each policy
  // fetch request. This must be called before starting the server, and only
  // works when the server serves from a temporary directory.
  void EnableAutomaticRotationOfSigningKeys();

 private:
  std::unique_ptr<policy::LocalPolicyTestServer> policy_test_server_;
  base::Value server_config_;
  bool canned_signing_keys_enabled_ = false;
  bool automatic_rotation_of_signing_keys_enabled_ = false;
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove after the //chrome/browser/chromeos
// source migration is finished.
namespace chromeos {
using ::ash::LocalPolicyTestServerMixin;
}

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_LOCAL_POLICY_TEST_SERVER_MIXIN_H_
