// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_EMBEDDED_POLICY_TEST_SERVER_MIXIN_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_EMBEDDED_POLICY_TEST_SERVER_MIXIN_H_

#include <memory>
#include <string>

#include "base/command_line.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_state_keys_broker.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "net/http/http_status_code.h"

namespace policy {
class EmbeddedPolicyTestServer;
}

namespace ash {

// This test mixin covers setting up EmbeddedPolicyTestServer and adding a
// command-line flag to use it. Please see SetUp function for default settings.
// Server is started after SetUp execution.
class EmbeddedPolicyTestServerMixin : public InProcessBrowserTestMixin {
 public:
  explicit EmbeddedPolicyTestServerMixin(InProcessBrowserTestMixinHost* host);

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

  // Updates the device policy blob served by the local policy test server.
  void UpdateDevicePolicy(
      const enterprise_management::ChromeDeviceSettingsProto& policy);

  // Updates user policy blob served by the embedded policy test server.
  // `policy_user` - the policy user's email.
  void UpdateUserPolicy(
      const enterprise_management::CloudPolicySettings& policy,
      const std::string& policy_user);

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

  // Configures fake statistics provider with values that can be used with
  // zero-touch enrollment.
  void ConfigureFakeStatisticsForZeroTouch(
      system::ScopedFakeStatisticsProvider* provider);

 private:
  std::unique_ptr<policy::EmbeddedPolicyTestServer> policy_test_server_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_EMBEDDED_POLICY_TEST_SERVER_MIXIN_H_
