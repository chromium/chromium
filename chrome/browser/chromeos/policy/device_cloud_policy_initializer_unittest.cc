// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_cloud_policy_initializer.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/chromeos/policy/enrollment_config.h"
#include "chrome/browser/chromeos/policy/server_backed_device_state.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/common/pref_names.h"
#include "chromeos/attestation/attestation_flow_utils.h"
#include "chromeos/attestation/mock_attestation_flow.h"
#include "chromeos/dbus/attestation/attestation_client.h"
#include "chromeos/dbus/attestation/interface.pb.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kFakeChallenge[] = "fake challenge";

}  // namespace

namespace policy {

struct ZeroTouchParam {
  const char* enable_zero_touch_flag;
  EnrollmentConfig::AuthMechanism auth_mechanism;
  EnrollmentConfig::AuthMechanism auth_mechanism_after_oobe;

  ZeroTouchParam(const char* flag,
                 EnrollmentConfig::AuthMechanism auth,
                 EnrollmentConfig::AuthMechanism auth_after_oobe)
      : enable_zero_touch_flag(flag),
        auth_mechanism(auth),
        auth_mechanism_after_oobe(auth_after_oobe) {}
};

class DeviceCloudPolicyInitializerTest
    : public testing::TestWithParam<ZeroTouchParam> {
 protected:
  DeviceCloudPolicyInitializerTest()
      : device_cloud_policy_initializer_(
            &local_state_,
            nullptr,
            nullptr,
            &install_attributes_,
            nullptr,
            nullptr,
            nullptr,
            std::make_unique<chromeos::attestation::MockAttestationFlow>(),
            &statistics_provider_) {
    RegisterLocalState(local_state_.registry());
    statistics_provider_.SetMachineStatistic(
        chromeos::system::kSerialNumberKeyForTest, "fake-serial");
    statistics_provider_.SetMachineStatistic(
        chromeos::system::kHardwareClassKey, "fake-hardware");
  }

  void SetupZeroTouchFlag();

  chromeos::system::ScopedFakeStatisticsProvider statistics_provider_;
  TestingPrefServiceSimple local_state_;
  chromeos::StubInstallAttributes install_attributes_;
  DeviceCloudPolicyInitializer device_cloud_policy_initializer_;
};

void DeviceCloudPolicyInitializerTest::SetupZeroTouchFlag() {
  const ZeroTouchParam& param = GetParam();
  if (param.enable_zero_touch_flag != nullptr) {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        chromeos::switches::kEnterpriseEnableZeroTouchEnrollment,
        param.enable_zero_touch_flag);
  }
}

TEST_P(DeviceCloudPolicyInitializerTest,
       GetPrescribedEnrollmentConfigDuringOOBE) {
  SetupZeroTouchFlag();

  // Default configuration is empty.
  EnrollmentConfig config =
      device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
  EXPECT_TRUE(config.management_domain.empty());
  EXPECT_EQ(GetParam().auth_mechanism, config.auth_mechanism);

  // Set signals in increasing order of precedence, check results.

  // OEM manifest: advertised enrollment.
  statistics_provider_.SetMachineFlag(
      chromeos::system::kOemIsEnterpriseManagedKey, true);
  config = device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_LOCAL_ADVERTISED, config.mode);
  EXPECT_TRUE(config.management_domain.empty());
  EXPECT_EQ(GetParam().auth_mechanism, config.auth_mechanism);

  // Pref: advertised enrollment. The resulting |config| is indistinguishable
  // from the OEM manifest configuration, so clear the latter to at least
  // verify the pref configuration results in the expect behavior on its own.
  statistics_provider_.ClearMachineFlag(
      chromeos::system::kOemIsEnterpriseManagedKey);
  local_state_.SetBoolean(prefs::kDeviceEnrollmentAutoStart, true);
  config = device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_LOCAL_ADVERTISED, config.mode);
  EXPECT_TRUE(config.management_domain.empty());
  EXPECT_EQ(GetParam().auth_mechanism, config.auth_mechanism);

  // Server-backed state: advertised enrollment.
  base::DictionaryValue state_dict;
  state_dict.SetString(kDeviceStateMode,
                       kDeviceStateRestoreModeReEnrollmentRequested);
  state_dict.SetString(kDeviceStateManagementDomain, "example.com");
  local_state_.Set(prefs::kServerBackedDeviceState, state_dict);
  config = device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_SERVER_ADVERTISED, config.mode);
  EXPECT_EQ("example.com", config.management_domain);
  EXPECT_EQ(GetParam().auth_mechanism, config.auth_mechanism);

  // OEM manifest: forced enrollment.
  statistics_provider_.SetMachineFlag(
      chromeos::system::kOemIsEnterpriseManagedKey, true);
  statistics_provider_.SetMachineFlag(
      chromeos::system::kOemCanExitEnterpriseEnrollmentKey, false);
  config = device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_LOCAL_FORCED, config.mode);
  EXPECT_TRUE(config.management_domain.empty());
  EXPECT_EQ(GetParam().auth_mechanism, config.auth_mechanism);

  // Pref: forced enrollment. The resulting |config| is indistinguishable from
  // the OEM manifest configuration, so clear the latter to at least verify the
  // pref configuration results in the expect behavior on its own.
  statistics_provider_.ClearMachineFlag(
      chromeos::system::kOemIsEnterpriseManagedKey);
  local_state_.SetBoolean(prefs::kDeviceEnrollmentCanExit, false);
  config = device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_LOCAL_FORCED, config.mode);
  EXPECT_TRUE(config.management_domain.empty());
  EXPECT_EQ(GetParam().auth_mechanism, config.auth_mechanism);

  // Server-backed state: forced enrollment.
  state_dict.SetString(kDeviceStateMode,
                       kDeviceStateRestoreModeReEnrollmentEnforced);
  local_state_.Set(prefs::kServerBackedDeviceState, state_dict);
  config = device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_SERVER_FORCED, config.mode);
  EXPECT_EQ("example.com", config.management_domain);
  EXPECT_EQ(GetParam().auth_mechanism, config.auth_mechanism);
}

TEST_P(DeviceCloudPolicyInitializerTest,
       GetPrescribedEnrollmentConfigAfterOOBE) {
  SetupZeroTouchFlag();

  // If OOBE is complete, we may re-enroll to the domain configured in install
  // attributes. This is only enforced after detecting enrollment loss.
  local_state_.SetBoolean(chromeos::prefs::kOobeComplete, true);
  EnrollmentConfig config =
      device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
  EXPECT_TRUE(config.management_domain.empty());
  EXPECT_EQ(GetParam().auth_mechanism_after_oobe, config.auth_mechanism);

  // Advertised enrollment gets ignored.
  local_state_.SetBoolean(prefs::kDeviceEnrollmentAutoStart, true);
  statistics_provider_.SetMachineFlag(
      chromeos::system::kOemIsEnterpriseManagedKey, true);
  config = device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
  EXPECT_TRUE(config.management_domain.empty());
  EXPECT_EQ(GetParam().auth_mechanism_after_oobe, config.auth_mechanism);

  // If the device is enterprise-managed, the management domain gets pulled from
  // install attributes.
  install_attributes_.SetCloudManaged("example.com", "fake-id");
  config = device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
  EXPECT_EQ("example.com", config.management_domain);
  EXPECT_EQ(GetParam().auth_mechanism_after_oobe, config.auth_mechanism);

  // If enrollment recovery is on, this is signaled in |config.mode|.
  local_state_.SetBoolean(prefs::kEnrollmentRecoveryRequired, true);
  config = device_cloud_policy_initializer_.GetPrescribedEnrollmentConfig();
  EXPECT_EQ(EnrollmentConfig::MODE_RECOVERY, config.mode);
  EXPECT_EQ("example.com", config.management_domain);
  EXPECT_EQ(GetParam().auth_mechanism_after_oobe, config.auth_mechanism);
}

INSTANTIATE_TEST_SUITE_P(
    ZeroTouchFlag,
    DeviceCloudPolicyInitializerTest,
    ::testing::Values(
        ZeroTouchParam(nullptr,  // No flag set.
                       EnrollmentConfig::AUTH_MECHANISM_INTERACTIVE,
                       EnrollmentConfig::AUTH_MECHANISM_INTERACTIVE),
        ZeroTouchParam("",  // Flag set without a set value.
                       EnrollmentConfig::AUTH_MECHANISM_BEST_AVAILABLE,
                       EnrollmentConfig::AUTH_MECHANISM_INTERACTIVE),
        ZeroTouchParam("forced",
                       EnrollmentConfig::AUTH_MECHANISM_ATTESTATION,
                       EnrollmentConfig::AUTH_MECHANISM_ATTESTATION)));

class DeviceCloudPolicyInitializerTpmEnrollmentKeySigningServiceTest
    : public testing::Test {
 public:
  DeviceCloudPolicyInitializerTpmEnrollmentKeySigningServiceTest() {
    chromeos::AttestationClient::InitializeFake();
  }
  ~DeviceCloudPolicyInitializerTpmEnrollmentKeySigningServiceTest() override {
    chromeos::AttestationClient::Shutdown();
  }

  static void SaveChallengeResponseAndRunCallback(
      base::OnceClosure callback,
      bool* out_success,
      enterprise_management::SignedData* out_signed_data,
      bool in_success,
      enterprise_management::SignedData in_signed_data) {
    *out_success = in_success;
    *out_signed_data = in_signed_data;
    std::move(callback).Run();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(DeviceCloudPolicyInitializerTpmEnrollmentKeySigningServiceTest,
       SigningSuccess) {
  chromeos::AttestationClient::Get()
      ->GetTestInterface()
      ->AllowlistSignSimpleChallengeKey(
          /*username=*/"",
          chromeos::attestation::GetKeyNameForProfile(
              chromeos::attestation::PROFILE_ENTERPRISE_ENROLLMENT_CERTIFICATE,
              ""));

  base::RunLoop run_loop;
  bool returned_success = false;
  enterprise_management::SignedData returned_signed_data;
  DeviceCloudPolicyInitializer::TpmEnrollmentKeySigningService signing_service;
  signing_service.SignData(
      kFakeChallenge,
      BindOnce(DeviceCloudPolicyInitializerTpmEnrollmentKeySigningServiceTest::
                   SaveChallengeResponseAndRunCallback,
               run_loop.QuitClosure(), &returned_success,
               &returned_signed_data));
  run_loop.Run();
  EXPECT_TRUE(returned_success);
  ::attestation::SignedData result_challenge_response;
  result_challenge_response.set_data(returned_signed_data.data());
  result_challenge_response.set_signature(returned_signed_data.signature());
  EXPECT_TRUE(chromeos::AttestationClient::Get()
                  ->GetTestInterface()
                  ->VerifySimpleChallengeResponse(kFakeChallenge,
                                                  result_challenge_response));
}

TEST_F(DeviceCloudPolicyInitializerTpmEnrollmentKeySigningServiceTest,
       SigningFailure) {
  // This is expected to be failed because we don't allowslit any key in
  // `FakeAttestationClient`.
  base::RunLoop run_loop;
  bool returned_success = true;
  enterprise_management::SignedData returned_signed_data;
  DeviceCloudPolicyInitializer::TpmEnrollmentKeySigningService signing_service;
  signing_service.SignData(
      kFakeChallenge,
      BindOnce(DeviceCloudPolicyInitializerTpmEnrollmentKeySigningServiceTest::
                   SaveChallengeResponseAndRunCallback,
               run_loop.QuitClosure(), &returned_success,
               &returned_signed_data));
  run_loop.Run();
  EXPECT_FALSE(returned_success);
}

}  // namespace policy
