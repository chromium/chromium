// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"

#include "ash/constants/ash_login_pref_names.h"
#include "ash/constants/ash_pref_names.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "base/test/gtest_util.h"
#include "base/test/run_until.h"
#include "base/test/scoped_command_line.h"
#include "base/values.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/ash/login/configuration_keys.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_test_helper.h"
#include "chrome/browser/ash/policy/server_backed_state/server_backed_device_state.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/scoped_test_device_settings_service.h"
#include "chrome/browser/prefs/browser_prefs.h"
#include "chrome/browser/ui/ash/login/fake_login_display_host.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/policy/device_policy/device_policy_builder.h"
#include "chromeos/ash/components/system/fake_statistics_provider.h"
#include "chromeos/ash/components/system/statistics_provider.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

constexpr char kTestDomain[] = "example.com";

class EnrollmentConfigTest : public testing::Test {
 protected:
  EnrollmentConfigTest() = default;

  void SetUp() override {
    RegisterLocalState(local_state_.registry());
    statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey,
                                             "fake-serial");
    statistics_provider_.SetMachineStatistic(ash::system::kHardwareClassKey,
                                             "fake-hardware");
    ash::InstallAttributes::SetForTesting(&install_attributes_);
  }

  void TearDown() override { ash::InstallAttributes::ShutdownForTesting(); }

  EnrollmentConfig GetPrescribedConfig() {
    return EnrollmentConfig::GetPrescribedEnrollmentConfig(
        &local_state_, install_attributes_, &statistics_provider_,
        enrollment_test_helper_.oobe_configuration());
  }

  content::BrowserTaskEnvironment task_environment_;
  ash::system::ScopedFakeStatisticsProvider statistics_provider_;
  TestingPrefServiceSimple local_state_;
  ash::StubInstallAttributes install_attributes_;
  base::test::ScopedCommandLine command_line_;
  test::EnrollmentTestHelper enrollment_test_helper_{&command_line_,
                                                     &statistics_provider_};
  ash::FakeLoginDisplayHost fake_login_display_host_;
  ash::FakeSessionManagerClient fake_session_manager_client_;
  ash::ScopedTestDeviceSettingsService scoped_device_settings_;
  policy::DevicePolicyBuilder device_policy_;
};

TEST_F(EnrollmentConfigTest, TokenEnrollmentModeWithNoTokenYieldsModeNone) {
  enrollment_test_helper_.SetUpFlexDevice();
  auto state_dict = base::DictValue().Set(
      kDeviceStateMode, kDeviceStateInitialModeTokenEnrollment);
  local_state_.SetDict(prefs::kServerBackedDeviceState, state_dict.Clone());

  const auto config = GetPrescribedConfig();

  EXPECT_EQ(config.mode, EnrollmentConfig::MODE_NONE);
  EXPECT_FALSE(config.should_enroll());
}

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
TEST_F(
    EnrollmentConfigTest,
    TokenEnrollmentModeWithTokenPresentYieldsEnrollmentConfigModeTokenEnrollment) {
  enrollment_test_helper_.SetUpFlexDevice();
  enrollment_test_helper_.SetUpEnrollmentTokenConfig();
  auto state_dict = base::DictValue().Set(
      kDeviceStateMode, kDeviceStateInitialModeTokenEnrollment);
  local_state_.SetDict(prefs::kServerBackedDeviceState, state_dict.Clone());

  const EnrollmentConfig config = GetPrescribedConfig();

  EXPECT_EQ(config.mode,
            EnrollmentConfig::MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED);
  EXPECT_EQ(config.enrollment_token, test::kEnrollmentToken);
  EXPECT_TRUE(config.should_enroll());
  EXPECT_TRUE(config.is_forced());
  EXPECT_TRUE(config.is_mode_with_manual_fallback());
  EXPECT_TRUE(config.is_automatic_enrollment());
  EXPECT_FALSE(config.is_mode_oauth());
  EXPECT_EQ(CHECK_DEREF(config.GetManualFallbackConfig()).mode,
            EnrollmentConfig::MODE_ENROLLMENT_TOKEN_INITIAL_MANUAL_FALLBACK);
}

TEST_F(
    EnrollmentConfigTest,
    TokenEnrollmentModeWithRemoteDeploymentSourceYieldsRemoteDeploymentMode) {
  const char kRemoteDeploymentFlexOobeConfig[] = R"({
    "enrollmentToken": "test-enrollment-token",
    "source": "REMOTE_DEPLOYMENT"
  })";
  enrollment_test_helper_.SetUpFlexDevice();
  enrollment_test_helper_.SetUpEnrollmentTokenConfig(
      kRemoteDeploymentFlexOobeConfig);
  auto state_dict = base::DictValue().Set(
      kDeviceStateMode, kDeviceStateInitialModeTokenEnrollment);
  local_state_.SetDict(prefs::kServerBackedDeviceState, state_dict.Clone());

  const EnrollmentConfig config = GetPrescribedConfig();

  EXPECT_EQ(config.mode,
            EnrollmentConfig::MODE_REMOTE_DEPLOYMENT_SERVER_FORCED);
  EXPECT_TRUE(config.should_enroll());
  EXPECT_TRUE(config.is_forced());
  EXPECT_TRUE(config.is_mode_with_manual_fallback());
  EXPECT_TRUE(config.is_automatic_enrollment());
  EXPECT_EQ(CHECK_DEREF(config.GetManualFallbackConfig()).mode,
            EnrollmentConfig::MODE_REMOTE_DEPLOYMENT_MANUAL_FALLBACK);
}

struct EnrollmentConfigOOBEConfigSourceTestCase {
  const char* json_source;
  OOBEConfigSource expected_oobe_config_source;
  EnrollmentConfig::Mode expected_mode;
};

class EnrollmentConfigOOBEConfigSourceTest
    : public EnrollmentConfigTest,
      public testing::WithParamInterface<
          EnrollmentConfigOOBEConfigSourceTestCase> {};

const EnrollmentConfigOOBEConfigSourceTestCase test_cases[] = {
    {"", OOBEConfigSource::kNone,
     EnrollmentConfig::Mode::MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED},
    {"UNKNOWN_VALUE", OOBEConfigSource::kUnknown,
     EnrollmentConfig::Mode::MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED},
    {"REMOTE_DEPLOYMENT", OOBEConfigSource::kRemoteDeployment,
     EnrollmentConfig::Mode::MODE_REMOTE_DEPLOYMENT_SERVER_FORCED},
    {"PACKAGING_TOOL", OOBEConfigSource::kPackagingTool,
     EnrollmentConfig::Mode::MODE_ENROLLMENT_TOKEN_INITIAL_SERVER_FORCED},
};

TEST_P(EnrollmentConfigOOBEConfigSourceTest,
       TokenEnrollmentModeWithTokenAndOOBEConfigSource) {
  EnrollmentConfigOOBEConfigSourceTestCase test_case = GetParam();
  const char kOOBEConfigFormat[] = R"({
    "enrollmentToken": "test_enrollment_token",
    "source": "%s"
  })";
  std::string oobe_config =
      base::StringPrintf(kOOBEConfigFormat, test_case.json_source).c_str();
  enrollment_test_helper_.SetUpFlexDevice();
  enrollment_test_helper_.SetUpEnrollmentTokenConfig(oobe_config.c_str());
  auto state_dict = base::DictValue().Set(
      kDeviceStateMode, kDeviceStateInitialModeTokenEnrollment);
  local_state_.SetDict(prefs::kServerBackedDeviceState, state_dict.Clone());

  const EnrollmentConfig config = GetPrescribedConfig();

  EXPECT_EQ(config.enrollment_token, test::kEnrollmentToken);
  EXPECT_EQ(config.oobe_config_source, test_case.expected_oobe_config_source);
  EXPECT_EQ(config.mode, test_case.expected_mode);
}

INSTANTIATE_TEST_SUITE_P(TokenEnrollmentModeWithTokenAndOOBEConfigSource,
                         EnrollmentConfigOOBEConfigSourceTest,
                         testing::ValuesIn(test_cases));
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Test enrollment configuration based on device state with precedence.
TEST_F(EnrollmentConfigTest, GetPrescribedEnrollmentConfigDuringOOBE) {
  // Default configuration is empty.
  {
    const auto config = GetPrescribedConfig();
    EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
    EXPECT_TRUE(config.management_domain.empty());
    EXPECT_FALSE(config.is_automatic_enrollment());
    EXPECT_FALSE(config.is_mode_oauth());
  }

  // Set signals in increasing order of precedence, check results.

  // OEM manifest: advertised enrollment.
  statistics_provider_.SetMachineFlag(ash::system::kOemIsEnterpriseManagedKey,
                                      true);
  {
    const auto config = GetPrescribedConfig();
    EXPECT_EQ(EnrollmentConfig::MODE_LOCAL_ADVERTISED, config.mode);
    EXPECT_TRUE(config.management_domain.empty());
    EXPECT_TRUE(config.is_mode_oauth());
    EXPECT_FALSE(config.is_mode_with_manual_fallback());
    EXPECT_FALSE(config.GetManualFallbackConfig().has_value());
  }

  // Pref: advertised enrollment. The resulting |config| is indistinguishable
  // from the OEM manifest configuration, so clear the latter to at least
  // verify the pref configuration results in the expect behavior on its own.
  statistics_provider_.ClearMachineFlag(
      ash::system::kOemIsEnterpriseManagedKey);
  local_state_.SetBoolean(ash::prefs::kDeviceEnrollmentAutoStart, true);
  {
    const auto config = GetPrescribedConfig();
    EXPECT_EQ(EnrollmentConfig::MODE_LOCAL_ADVERTISED, config.mode);
    EXPECT_TRUE(config.management_domain.empty());
    EXPECT_TRUE(config.is_mode_oauth());
    EXPECT_FALSE(config.is_mode_with_manual_fallback());
    EXPECT_FALSE(config.GetManualFallbackConfig().has_value());
  }

  // Server-backed state: advertised enrollment.
  auto state_dict =
      base::DictValue()
          .Set(kDeviceStateMode, kDeviceStateRestoreModeReEnrollmentRequested)
          .Set(kDeviceStateManagementDomain, kTestDomain);
  local_state_.SetDict(prefs::kServerBackedDeviceState, state_dict.Clone());
  {
    const auto config = GetPrescribedConfig();
    EXPECT_EQ(EnrollmentConfig::MODE_SERVER_ADVERTISED, config.mode);
    EXPECT_EQ(kTestDomain, config.management_domain);
    EXPECT_TRUE(config.is_mode_oauth());
    EXPECT_FALSE(config.is_mode_with_manual_fallback());
    EXPECT_FALSE(config.GetManualFallbackConfig().has_value());
  }

  // OEM manifest: forced enrollment.
  statistics_provider_.SetMachineFlag(ash::system::kOemIsEnterpriseManagedKey,
                                      true);
  statistics_provider_.SetMachineFlag(
      ash::system::kOemCanExitEnterpriseEnrollmentKey, false);
  {
    const auto config = GetPrescribedConfig();
    EXPECT_EQ(EnrollmentConfig::MODE_LOCAL_FORCED, config.mode);
    EXPECT_TRUE(config.management_domain.empty());
    EXPECT_TRUE(config.is_mode_oauth());
    EXPECT_FALSE(config.is_mode_with_manual_fallback());
    EXPECT_FALSE(config.GetManualFallbackConfig().has_value());
  }

  // Pref: forced enrollment. The resulting |config| is indistinguishable from
  // the OEM manifest configuration, so clear the latter to at least verify the
  // pref configuration results in the expect behavior on its own.
  statistics_provider_.ClearMachineFlag(
      ash::system::kOemIsEnterpriseManagedKey);
  local_state_.SetBoolean(ash::prefs::kDeviceEnrollmentCanExit, false);
  {
    const auto config = GetPrescribedConfig();
    EXPECT_EQ(EnrollmentConfig::MODE_LOCAL_FORCED, config.mode);
    EXPECT_TRUE(config.management_domain.empty());
    EXPECT_TRUE(config.is_mode_oauth());
    EXPECT_FALSE(config.is_mode_with_manual_fallback());
    EXPECT_FALSE(config.GetManualFallbackConfig().has_value());
  }

  // Server-backed state: forced initial attestation-based enrollment.
  local_state_.SetDict(
      prefs::kServerBackedDeviceState,
      base::DictValue()
          .Set(kDeviceStateMode, kDeviceStateInitialModeEnrollmentZeroTouch)
          .Set(kDeviceStateManagementDomain, kTestDomain));
  {
    const auto config = GetPrescribedConfig();
    EXPECT_EQ(EnrollmentConfig::MODE_ATTESTATION_INITIAL_SERVER_FORCED,
              config.mode);
    EXPECT_EQ(kTestDomain, config.management_domain);
    EXPECT_TRUE(config.is_automatic_enrollment());
    EXPECT_TRUE(config.is_mode_attestation());

    const auto manual_fallback_config = config.GetManualFallbackConfig();
    ASSERT_TRUE(manual_fallback_config.has_value());
    EXPECT_TRUE(manual_fallback_config->is_manual_fallback());
    EXPECT_EQ(EnrollmentConfig::MODE_ATTESTATION_INITIAL_MANUAL_FALLBACK,
              manual_fallback_config->mode);
    EXPECT_TRUE(manual_fallback_config->is_mode_oauth());
  }

  // Server-backed state: forced attestation-based re-enrollment.
  local_state_.SetDict(
      prefs::kServerBackedDeviceState,
      base::DictValue()
          .Set(kDeviceStateMode, kDeviceStateRestoreModeReEnrollmentZeroTouch)
          .Set(kDeviceStateManagementDomain, kTestDomain));
  {
    const auto config = GetPrescribedConfig();
    EXPECT_EQ(EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED, config.mode);
    EXPECT_EQ(kTestDomain, config.management_domain);
    EXPECT_TRUE(config.is_automatic_enrollment());
    EXPECT_TRUE(config.is_mode_attestation());

    const auto manual_fallback_config = config.GetManualFallbackConfig();
    ASSERT_TRUE(manual_fallback_config);
    EXPECT_TRUE(manual_fallback_config->is_manual_fallback());
    EXPECT_EQ(EnrollmentConfig::MODE_ATTESTATION_MANUAL_FALLBACK,
              manual_fallback_config->mode);
    EXPECT_TRUE(manual_fallback_config->is_mode_oauth());
  }

  // Server-backed state: forced initial enrollment.
  local_state_.SetDict(
      prefs::kServerBackedDeviceState,
      base::DictValue()
          .Set(kDeviceStateMode, kDeviceStateInitialModeEnrollmentEnforced)
          .Set(kDeviceStateManagementDomain, kTestDomain));
  {
    const auto config = GetPrescribedConfig();
    EXPECT_EQ(EnrollmentConfig::MODE_INITIAL_SERVER_FORCED, config.mode);
    EXPECT_EQ(kTestDomain, config.management_domain);
    EXPECT_TRUE(config.is_mode_oauth());
    EXPECT_FALSE(config.is_mode_with_manual_fallback());
    EXPECT_FALSE(config.GetManualFallbackConfig().has_value());
  }

  // Server-backed state: forced re-enrollment.
  local_state_.SetDict(
      prefs::kServerBackedDeviceState,
      base::DictValue()
          .Set(kDeviceStateMode, kDeviceStateRestoreModeReEnrollmentEnforced)
          .Set(kDeviceStateManagementDomain, kTestDomain));
  {
    const auto config = GetPrescribedConfig();
    EXPECT_EQ(EnrollmentConfig::MODE_SERVER_FORCED, config.mode);
    EXPECT_EQ(kTestDomain, config.management_domain);
    EXPECT_TRUE(config.is_mode_oauth());
    EXPECT_FALSE(config.is_mode_with_manual_fallback());
    EXPECT_FALSE(config.GetManualFallbackConfig().has_value());
  }

  // OOBE config: rollback re-enrollment.
  CHECK_DEREF(fake_login_display_host_.GetWizardContext())
      .configuration.Set(ash::configuration::kRestoreAfterRollback, true);
  {
    const auto config = GetPrescribedConfig();
    EXPECT_EQ(EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_FORCED, config.mode);
    EXPECT_TRUE(config.management_domain.empty());
    EXPECT_TRUE(config.is_automatic_enrollment());
    EXPECT_TRUE(config.is_mode_attestation());

    const auto manual_fallback_config = config.GetManualFallbackConfig();
    ASSERT_TRUE(manual_fallback_config);
    EXPECT_TRUE(manual_fallback_config->is_manual_fallback());
    EXPECT_EQ(EnrollmentConfig::MODE_ATTESTATION_ROLLBACK_MANUAL_FALLBACK,
              manual_fallback_config->mode);
    EXPECT_TRUE(manual_fallback_config->is_mode_oauth());
  }
}

// Test enrollment configuration after OOBE completed.
TEST_F(EnrollmentConfigTest, GetPrescribedEnrollmentConfigAfterOOBE) {
  // If OOBE is complete, we may re-enroll to the domain configured in install
  // attributes. This is only enforced after detecting enrollment loss.
  local_state_.SetBoolean(ash::prefs::kOobeComplete, true);
  {
    const auto config = GetPrescribedConfig();
    EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
    EXPECT_TRUE(config.management_domain.empty());
    EXPECT_FALSE(config.should_enroll());
  }

  // Advertised enrollment gets ignored.
  local_state_.SetBoolean(ash::prefs::kDeviceEnrollmentAutoStart, true);
  statistics_provider_.SetMachineFlag(ash::system::kOemIsEnterpriseManagedKey,
                                      true);
  {
    const auto config = GetPrescribedConfig();
    EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
    EXPECT_TRUE(config.management_domain.empty());
    EXPECT_FALSE(config.should_enroll());
  }

  // If the device is enterprise-managed and OOBE is complete, no enrollment
  // required.
  install_attributes_.SetCloudManaged(kTestDomain, "fake-id");
  ASSERT_TRUE(install_attributes_.IsCloudManaged());
  {
    const auto config = GetPrescribedConfig();
    EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
    EXPECT_TRUE(config.management_domain.empty());
    EXPECT_FALSE(config.should_enroll());
  }

  // If enrollment recovery is on, this is signaled in |config.mode|.
  local_state_.SetBoolean(ash::prefs::kEnrollmentRecoveryRequired, true);
  {
    const auto config = GetPrescribedConfig();
    EXPECT_EQ(EnrollmentConfig::MODE_RECOVERY, config.mode);
    EXPECT_EQ(kTestDomain, config.management_domain);
    EXPECT_TRUE(config.is_mode_oauth());
    EXPECT_FALSE(config.is_mode_with_manual_fallback());
    EXPECT_FALSE(config.GetManualFallbackConfig().has_value());
  }
}

TEST_F(EnrollmentConfigTest, GetDemoModeEnrollmentConfig) {
  const auto config = EnrollmentConfig::GetDemoModeEnrollmentConfig();

  EXPECT_EQ(EnrollmentConfig::MODE_ATTESTATION, config.mode);
  EXPECT_EQ(policy::kDemoModeDomain, config.management_domain);
  EXPECT_TRUE(config.is_automatic_enrollment());
  EXPECT_TRUE(config.is_mode_attestation());
  EXPECT_FALSE(config.is_mode_with_manual_fallback());
  EXPECT_FALSE(config.GetManualFallbackConfig().has_value());
}

TEST_F(EnrollmentConfigTest, GetEffectivePrescribedEnrollmentConfig) {
  EnrollmentConfig config;
  config.mode = EnrollmentConfig::MODE_ATTESTATION_SERVER_FORCED;
  config.management_domain = kTestDomain;

  ASSERT_TRUE(config.should_enroll());
  EXPECT_EQ(config, config.GetEffectiveConfig());
}

// Test that partially filled prescribed config that does not prescribe
// enrollment produces correct manual enrollment config.
TEST_F(EnrollmentConfigTest, GetEffectiveManualEnrollmentConfig) {
  {
    const auto config = GetPrescribedConfig();
    ASSERT_FALSE(config.should_enroll());

    const auto manual_config = config.GetEffectiveConfig();

    EXPECT_EQ(EnrollmentConfig::MODE_MANUAL, manual_config.mode);
    EXPECT_TRUE(manual_config.management_domain.empty());
    EXPECT_TRUE(manual_config.is_mode_oauth());
    EXPECT_EQ(LicenseType::kNone, manual_config.license_type);
    EXPECT_FALSE(config.is_mode_with_manual_fallback());
    EXPECT_FALSE(manual_config.GetManualFallbackConfig().has_value());
  }

  local_state_.SetDict(
      prefs::kServerBackedDeviceState,
      base::DictValue()
          .Set(kDeviceStateManagementDomain, kTestDomain)
          .Set(kDeviceStateLicenseType, kDeviceStateLicenseTypeEducation));

  {
    const auto config = GetPrescribedConfig();
    ASSERT_FALSE(config.should_enroll());

    const auto manual_config = config.GetEffectiveConfig();

    EXPECT_EQ(EnrollmentConfig::MODE_MANUAL, manual_config.mode);
    EXPECT_TRUE(manual_config.management_domain.empty());
    EXPECT_TRUE(manual_config.is_mode_oauth());
    EXPECT_EQ(LicenseType::kEducation, manual_config.license_type);
    EXPECT_FALSE(config.is_mode_with_manual_fallback());
    EXPECT_FALSE(manual_config.GetManualFallbackConfig().has_value());
  }
}

TEST_F(EnrollmentConfigTest, FalseRecoveryFlagDetectedWhenDmTokenExists) {
  // Without loaded DMToken, the recovery flag is respected.
  {
    local_state_.SetBoolean(ash::prefs::kEnrollmentRecoveryRequired, true);
    const auto config = GetPrescribedConfig();
    EXPECT_TRUE(
        local_state_.GetBoolean(ash::prefs::kEnrollmentRecoveryRequired));
    EXPECT_EQ(EnrollmentConfig::MODE_RECOVERY, config.mode);
  }

  // Set a DMToken and load it into the DeviceSettingsService.
  device_policy_.policy_data().set_request_token("fake-dm-token");
  device_policy_.Build();
  fake_session_manager_client_.set_device_policy(device_policy_.GetBlob());

  scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util{
      base::MakeRefCounted<ownership::MockOwnerKeyUtil>()};
  owner_key_util->SetPublicKeyFromPrivateKey(*device_policy_.GetSigningKey());
  owner_key_util->ImportPrivateKeyAndSetPublicKey(
      *device_policy_.GetSigningKey());

  ash::DeviceSettingsService::Get()->StartProcessing(
      &local_state_, &fake_session_manager_client_, owner_key_util);
  ash::DeviceSettingsService::Get()->LoadImmediately();

  // With DMToken loaded, the recovery flag will be cleared and recovery will
  // not be considered.
  {
    local_state_.SetBoolean(ash::prefs::kEnrollmentRecoveryRequired, true);
    const auto config = GetPrescribedConfig();
    EXPECT_FALSE(
        local_state_.GetBoolean(ash::prefs::kEnrollmentRecoveryRequired));
    EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
  }

  local_state_.SetDict(
      prefs::kServerBackedDeviceState,
      base::DictValue()
          .Set(kDeviceStateMode, kDeviceStateRestoreModeReEnrollmentEnforced)
          .Set(kDeviceStateManagementDomain, kTestDomain));
  {
    local_state_.SetBoolean(ash::prefs::kEnrollmentRecoveryRequired, true);
    const auto config = GetPrescribedConfig();
    EXPECT_FALSE(
        local_state_.GetBoolean(ash::prefs::kEnrollmentRecoveryRequired));
    EXPECT_EQ(EnrollmentConfig::MODE_SERVER_FORCED, config.mode);
  }
}

TEST_F(EnrollmentConfigTest, FalseRecoveryFlagIgnoredWithoutSerialNumber) {
  local_state_.SetBoolean(ash::prefs::kEnrollmentRecoveryRequired, true);
  // With non-empty serial number, the recovery flag is respected.
  {
    const auto config = GetPrescribedConfig();
    EXPECT_TRUE(
        local_state_.GetBoolean(ash::prefs::kEnrollmentRecoveryRequired));
    EXPECT_EQ(EnrollmentConfig::MODE_RECOVERY, config.mode);
  }

  // With empty serial number, the recovery flag will be cleared and recovery
  // will not be considered.
  statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey, "");
  {
    const auto config = GetPrescribedConfig();
    EXPECT_TRUE(
        local_state_.GetBoolean(ash::prefs::kEnrollmentRecoveryRequired));
    EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
  }

  local_state_.SetDict(
      prefs::kServerBackedDeviceState,
      base::DictValue()
          .Set(kDeviceStateMode, kDeviceStateRestoreModeReEnrollmentEnforced)
          .Set(kDeviceStateManagementDomain, kTestDomain));
  {
    const auto config = GetPrescribedConfig();
    EXPECT_TRUE(
        local_state_.GetBoolean(ash::prefs::kEnrollmentRecoveryRequired));
    EXPECT_EQ(EnrollmentConfig::MODE_SERVER_FORCED, config.mode);
  }
}

TEST_F(EnrollmentConfigTest, EnrolledDevicesDoNotEnrollAgain) {
  local_state_.SetBoolean(ash::prefs::kOobeComplete, true);
  install_attributes_.SetCloudManaged(kTestDomain, "fake-id");

  // When OOBE is completed and the device is cloud managed, no additional
  // enrollment is required (unless recovery is requested).
  // not be considered.
  {
    const auto config = GetPrescribedConfig();
    EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
  }

  // Server backed state is irrelevant if the device is already managed.
  local_state_.SetDict(
      prefs::kServerBackedDeviceState,
      base::DictValue()
          .Set(kDeviceStateMode, kDeviceStateRestoreModeReEnrollmentEnforced)
          .Set(kDeviceStateManagementDomain, kTestDomain));
  {
    const auto config = GetPrescribedConfig();
    EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
  }

  // Recovery could be required for managed devices.
  local_state_.SetBoolean(ash::prefs::kEnrollmentRecoveryRequired, true);
  {
    const auto config = GetPrescribedConfig();
    EXPECT_TRUE(
        local_state_.GetBoolean(ash::prefs::kEnrollmentRecoveryRequired));
    EXPECT_EQ(EnrollmentConfig::MODE_RECOVERY, config.mode);
  }

  // But recovery would be skipped in case of a missing serial number.
  statistics_provider_.SetMachineStatistic(ash::system::kSerialNumberKey, "");
  local_state_.SetBoolean(ash::prefs::kEnrollmentRecoveryRequired, true);
  {
    const auto config = GetPrescribedConfig();
    EXPECT_TRUE(
        local_state_.GetBoolean(ash::prefs::kEnrollmentRecoveryRequired));
    EXPECT_EQ(EnrollmentConfig::MODE_NONE, config.mode);
  }
}

}  // namespace policy
