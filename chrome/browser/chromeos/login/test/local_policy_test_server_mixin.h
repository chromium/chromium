// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOCAL_POLICY_TEST_SERVER_MIXIN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOCAL_POLICY_TEST_SERVER_MIXIN_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/local_policy_test_server.h"

namespace chromeos {

namespace system {
class ScopedFakeStatisticsProvider;
}  // namespace system

namespace test {

// Test constants used during enrollment wherever appropriate.
constexpr char kTestDomain[] = "test-domain.com";
constexpr char kTestRlzBrandCodeKey[] = "TEST";
constexpr char kTestSerialNumber[] = "111111";
constexpr char kTestHardwareClass[] = "hw";

}  // namespace test

// This test mixin covers setting up LocalPolicyTestServer and adding a
// command-line flag to use it. Please see SetUp function for default settings.
// Server is started after SetUp execution.
class LocalPolicyTestServerMixin : public InProcessBrowserTestMixin {
 public:
  explicit LocalPolicyTestServerMixin(InProcessBrowserTestMixinHost* host);
  ~LocalPolicyTestServerMixin() override;

  policy::LocalPolicyTestServer* server() { return policy_test_server_.get(); }

  // InProcessBrowserTestMixin:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // Updates device policy blob served by the local policy test server.
  bool UpdateDevicePolicy(
      const enterprise_management::ChromeDeviceSettingsProto& policy);

  // Updates user policy blob served by the local policy test server.
  // |policy_user| - the policy user's email.
  bool UpdateUserPolicy(
      const enterprise_management::CloudPolicySettings& policy,
      const std::string& policy_user);

  // Updates user policies served by the local policy test server, by
  // configuring ser of mandatory and recommended policies that should be
  // returned for the policy user.
  // |policy_user| - the policy user's email.
  bool UpdateUserPolicy(const base::Value& mandatory_policy,
                        const base::Value& recommended_policy,
                        const std::string& policy_user);

  // Configures and sets expectations for enrollment flow with license
  // selection. Non-negative values indicate number of available licenses.
  // There should be at least one license type.
  void ExpectAvailableLicenseCount(int perpetual, int annual, int kiosk);

  void ExpectTokenEnrollment(const std::string& enrollment_token,
                             const std::string& token_creator);

  void SetUpdateDeviceAttributesPermission(bool allowed);

  // Configures fake attestation flow so that we can test attestation-based
  // enrollment flows.
  void SetFakeAttestationFlow();

  // Configures server to respond with particular error code during requests.
  // |net_error_code| - error code from device_management_service.cc.
  void SetExpectedDeviceEnrollmentError(int net_error_code);
  void SetExpectedDeviceAttributeUpdateError(int net_error_code);
  void SetExpectedPolicyFetchError(int net_error_code);

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
      const base::Optional<std::string>& management_domain,
      const base::Optional<bool> is_license_packaged_with_device);

  // Utility function that configures server parameters for zero-touch
  // enrollment. Should be used in conjunction with enabling zero-touch
  // via command line and calling |ConfigureFakeStatisticsForZeroTouch|.
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

  DISALLOW_COPY_AND_ASSIGN(LocalPolicyTestServerMixin);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOCAL_POLICY_TEST_SERVER_MIXIN_H_
