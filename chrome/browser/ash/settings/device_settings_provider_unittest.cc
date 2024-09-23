// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/device_settings_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/values.h"
#include "chrome/browser/ash/ownership/owner_key_loader.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace ash {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Mock;

namespace {

const char kDisabledMessage[] = "This device has been disabled.";

constexpr char kDeviceLocalAccountKioskAccountId[] = "kiosk_account_id";

}  // namespace

class DeviceSettingsProviderTest : public DeviceSettingsTestBase {
 public:
  DeviceSettingsProviderTest(const DeviceSettingsProviderTest&) = delete;
  DeviceSettingsProviderTest& operator=(const DeviceSettingsProviderTest&) =
      delete;

  MOCK_METHOD1(SettingChanged, void(const std::string&));
  MOCK_METHOD0(GetTrustedCallback, void(void));

 protected:
  DeviceSettingsProviderTest()
      : local_state_(TestingBrowserProcess::GetGlobal()),
        user_data_dir_override_(chrome::DIR_USER_DATA) {}

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();

    // Disable owner key migration.
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{kStoreOwnerKeyInPrivateSlot},
        /*disabled_features=*/{kMigrateOwnerKeyToPrivateSlot});

    EXPECT_CALL(*this, SettingChanged(_)).Times(AnyNumber());
    provider_ = std::make_unique<DeviceSettingsProvider>(
        base::BindRepeating(&DeviceSettingsProviderTest::SettingChanged,
                            base::Unretained(this)),
        device_settings_service_.get(), local_state_.Get());
    Mock::VerifyAndClearExpectations(this);
  }

  void TearDown() override {
    provider_.reset();
    DeviceSettingsTestBase::TearDown();
  }

  void BuildAndInstallDevicePolicy() {
    EXPECT_CALL(*this, SettingChanged(_)).Times(AtLeast(1));
    device_policy_->Build();
    session_manager_client_.set_device_policy(device_policy_->GetBlob());
    ReloadDeviceSettings();
    Mock::VerifyAndClearExpectations(this);
  }

  // Helper routine to enable/disable all reporting settings in policy.
  void SetReportingSettings(bool enable_reporting, int frequency) {
    em::DeviceReportingProto* proto =
        device_policy_->payload().mutable_device_reporting();
    proto->set_report_version_info(enable_reporting);
    proto->set_report_activity_times(enable_reporting);
    proto->set_report_audio_status(enable_reporting);
    proto->set_report_boot_mode(enable_reporting);
    proto->set_report_location(enable_reporting);
    proto->set_report_network_configuration(enable_reporting);
    proto->set_report_network_status(enable_reporting);
    proto->set_report_users(enable_reporting);
    proto->set_report_session_status(enable_reporting);
    proto->set_report_graphics_status(enable_reporting);
    proto->set_report_crash_report_info(enable_reporting);
    proto->set_report_os_update_status(enable_reporting);
    proto->set_report_running_kiosk_app(enable_reporting);
    proto->set_report_peripherals(enable_reporting);
    proto->set_report_power_status(enable_reporting);
    proto->set_report_security_status(enable_reporting);
    proto->set_report_storage_status(enable_reporting);
    proto->set_report_board_status(enable_reporting);
    proto->set_report_app_info(enable_reporting);
    proto->set_report_print_jobs(enable_reporting);
    proto->set_report_login_logout(enable_reporting);
    proto->set_report_crd_sessions(enable_reporting);
    proto->set_report_runtime_counters(enable_reporting);
    proto->set_device_activity_heartbeat_enabled(enable_reporting);
    proto->set_report_network_events(enable_reporting);
    proto->set_report_network_telemetry_collection_rate_ms(frequency);
    proto->set_report_network_telemetry_event_checking_rate_ms(frequency);
    proto->set_device_status_frequency(frequency);
    proto->set_report_device_audio_status_checking_rate_ms(frequency);
    proto->set_device_report_runtime_counters_checking_rate_ms(frequency);
    proto->set_device_activity_heartbeat_collection_rate_ms(frequency);
    BuildAndInstallDevicePolicy();
  }

  // Helper routine to enable/disable all reporting settings in policy.
  void SetHeartbeatSettings(bool enable_heartbeat, int frequency) {
    em::DeviceHeartbeatSettingsProto* proto =
        device_policy_->payload().mutable_device_heartbeat_settings();
    proto->set_heartbeat_enabled(enable_heartbeat);
    proto->set_heartbeat_frequency(frequency);
    BuildAndInstallDevicePolicy();
  }

  // Helper routine to enable/disable log upload settings in policy.
  void SetLogUploadSettings(bool enable_system_log_upload) {
    em::DeviceLogUploadSettingsProto* proto =
        device_policy_->payload().mutable_device_log_upload_settings();
    proto->set_system_log_upload_enabled(enable_system_log_upload);
    BuildAndInstallDevicePolicy();
  }

  // Helper routine to set device wallpaper setting in policy.
  void SetWallpaperSettings(const std::string& wallpaper_settings) {
    em::DeviceWallpaperImageProto* proto =
        device_policy_->payload().mutable_device_wallpaper_image();
    proto->set_device_wallpaper_image(wallpaper_settings);
    BuildAndInstallDevicePolicy();
  }

  enum MetricsOption { DISABLE_METRICS, ENABLE_METRICS, REMOVE_METRICS_POLICY };

  // Helper routine to enable/disable metrics report upload settings in policy.
  void SetMetricsReportingSettings(MetricsOption option) {
    if (option == REMOVE_METRICS_POLICY) {
      // Remove policy altogether
      device_policy_->payload().clear_metrics_enabled();
    } else {
      // Enable or disable policy
      em::MetricsEnabledProto* proto =
          device_policy_->payload().mutable_metrics_enabled();
      proto->set_metrics_enabled(option == ENABLE_METRICS);
    }
    BuildAndInstallDevicePolicy();
  }

  // Helper routine to ensure all heartbeat policies have been correctly
  // decoded.
  void VerifyHeartbeatSettings(bool expected_enable_state,
                               int expected_frequency) {
    const base::Value expected_enabled_value(expected_enable_state);
    EXPECT_EQ(expected_enabled_value, *provider_->Get(kHeartbeatEnabled));

    const base::Value expected_frequency_value(expected_frequency);
    EXPECT_EQ(expected_frequency_value, *provider_->Get(kHeartbeatFrequency));
  }

  // Helper routine to ensure all reporting policies have been correctly
  // decoded.
  void VerifyReportingSettings(bool expected_enable_state,
                               int expected_frequency) {
    const char* reporting_settings[] = {
        kDeviceReportRuntimeCounters, kReportDeviceVersionInfo,
        kReportDeviceActivityTimes, kReportDeviceAudioStatus,
        kReportDeviceBoardStatus, kReportDeviceBootMode,
        // Device location reporting is not currently supported.
        // kReportDeviceLocation,
        kReportDeviceNetworkConfiguration, kReportDeviceNetworkStatus,
        kReportDeviceUsers, kReportDevicePeripherals, kReportDevicePowerStatus,
        kReportDeviceStorageStatus, kReportDeviceSessionStatus,
        kReportDeviceSecurityStatus, kReportDeviceGraphicsStatus,
        kReportDeviceCrashReportInfo, kReportDeviceAppInfo,
        kReportDevicePrintJobs, kReportDeviceLoginLogout, kReportOsUpdateStatus,
        kReportRunningKioskApp, kDeviceActivityHeartbeatEnabled,
        kDeviceReportNetworkEvents};

    const base::Value expected_enable_value(expected_enable_state);
    for (auto* setting : reporting_settings) {
      EXPECT_EQ(expected_enable_value, *provider_->Get(setting))
          << "Value for " << setting << " does not match expected";
    }

    const char* const reporting_frequency_settings[] = {
        kReportUploadFrequency,
        kReportDeviceNetworkTelemetryCollectionRateMs,
        kReportDeviceNetworkTelemetryEventCheckingRateMs,
        kReportDeviceAudioStatusCheckingRateMs,
        kDeviceReportRuntimeCountersCheckingRateMs,
        kDeviceActivityHeartbeatCollectionRateMs};
    const base::Value expected_frequency_value(expected_frequency);
    for (auto* frequency_setting : reporting_frequency_settings) {
      EXPECT_EQ(expected_frequency_value, *provider_->Get(frequency_setting))
          << "Value for " << frequency_setting << " does not match expected";
    }
  }

  // Helper routine to ensure log upload policy has been correctly
  // decoded.
  void VerifyLogUploadSettings(bool expected_enable_state) {
    const base::Value expected_enabled_value(expected_enable_state);
    EXPECT_EQ(expected_enabled_value, *provider_->Get(kSystemLogUploadEnabled));
  }

  void VerifyPolicyValue(const char* policy_key,
                         const base::Value* ptr_to_expected_value) {
    // The pointer might be null, so check before dereferencing.
    const base::Value* value = provider_->Get(policy_key);
    if (ptr_to_expected_value) {
      // This prevents tests from crashing if provider returns nullptr.
      ASSERT_TRUE(value);
      EXPECT_EQ(*ptr_to_expected_value, *value);
    } else {
      EXPECT_EQ(nullptr, value);
    }
  }

  void VerifyPolicyList(const char* policy_key,
                        const base::Value::List& expected_value) {
    const base::Value* value = provider_->Get(policy_key);
    // This prevents tests from crashing if provider returns nullptr.
    ASSERT_TRUE(value);
    EXPECT_TRUE(value->is_list());
    EXPECT_EQ(expected_value, provider_->Get(policy_key)->GetList());
  }

  // Helper routine to set LoginScreenDomainAutoComplete policy.
  void SetDomainAutoComplete(const std::string& domain) {
    em::LoginScreenDomainAutoCompleteProto* proto =
        device_policy_->payload().mutable_login_screen_domain_auto_complete();
    proto->set_login_screen_domain_auto_complete(domain);
    BuildAndInstallDevicePolicy();
  }

  // Helper routine to check value of the LoginScreenDomainAutoComplete policy.
  void VerifyDomainAutoComplete(const base::Value* ptr_to_expected_value) {
    VerifyPolicyValue(kAccountsPrefLoginScreenDomainAutoComplete,
                      ptr_to_expected_value);
  }

  // Helper routine to set AutoUpdates connection types policy.
  void SetAutoUpdateConnectionTypes(
      const std::vector<em::AutoUpdateSettingsProto::ConnectionType>& values) {
    em::AutoUpdateSettingsProto* proto =
        device_policy_->payload().mutable_auto_update_settings();
    proto->set_update_disabled(false);
    for (auto const& value : values) {
      proto->add_allowed_connection_types(value);
    }
    BuildAndInstallDevicePolicy();
  }

  // Helper to set the content protection policy
  void SetContentProtection(bool content_protection) {
    em::AttestationSettingsProto* proto =
        device_policy_->payload().mutable_attestation_settings();
    proto->set_content_protection_enabled(content_protection);
    BuildAndInstallDevicePolicy();
  }

  // Helper routine to set HostnameTemplate policy.
  void SetHostnameTemplate(const std::string& hostname_template) {
    em::NetworkHostnameProto* proto =
        device_policy_->payload().mutable_network_hostname();
    proto->set_device_hostname_template(hostname_template);
    BuildAndInstallDevicePolicy();
  }

  // Helper routine that sets the device DeviceAutoUpdateTimeRestrictions policy
  void SetDeviceAutoUpdateTimeRestrictions(const std::string& json_string) {
    em::AutoUpdateSettingsProto* proto =
        device_policy_->payload().mutable_auto_update_settings();
    proto->set_disallowed_time_intervals(json_string);
    BuildAndInstallDevicePolicy();
  }

  // Helper routine that sets the device DeviceScheduledUpdateCheck policy
  void SetDeviceScheduledUpdateCheck(const std::string& json_string) {
    em::DeviceScheduledUpdateCheckProto* proto =
        device_policy_->payload().mutable_device_scheduled_update_check();
    proto->set_device_scheduled_update_check_settings(json_string);
    BuildAndInstallDevicePolicy();
  }

  void SetPluginVmAllowedSetting(bool plugin_vm_allowed) {
    em::PluginVmAllowedProto* proto =
        device_policy_->payload().mutable_plugin_vm_allowed();
    proto->set_plugin_vm_allowed(plugin_vm_allowed);
    BuildAndInstallDevicePolicy();
  }

  void SetDeviceRebootOnUserSignout(
      em::DeviceRebootOnUserSignoutProto::RebootOnSignoutMode value) {
    EXPECT_CALL(*this, SettingChanged(_)).Times(AtLeast(1));
    em::DeviceRebootOnUserSignoutProto* proto =
        device_policy_->payload().mutable_device_reboot_on_user_signout();
    proto->set_reboot_on_signout_mode(value);
    device_policy_->Build();
    session_manager_client_.set_device_policy(device_policy_->GetBlob());
    ReloadDeviceSettings();
    Mock::VerifyAndClearExpectations(this);
  }

  void SetDeviceDockMacAddressSourceSetting(
      em::DeviceDockMacAddressSourceProto::Source
          device_dock_mac_address_source) {
    em::DeviceDockMacAddressSourceProto* proto =
        device_policy_->payload().mutable_device_dock_mac_address_source();
    proto->set_source(device_dock_mac_address_source);
    BuildAndInstallDevicePolicy();
  }

  void SetDeviceSecondFactorAuthenticationModeSetting(
      em::DeviceSecondFactorAuthenticationProto::U2fMode mode) {
    em::DeviceSecondFactorAuthenticationProto* proto =
        device_policy_->payload().mutable_device_second_factor_authentication();
    proto->set_mode(mode);
    BuildAndInstallDevicePolicy();
  }

  void SetDevicePowerwashAllowed(bool device_powerwash_allowed) {
    em::DevicePowerwashAllowedProto* proto =
        device_policy_->payload().mutable_device_powerwash_allowed();
    proto->set_device_powerwash_allowed(device_powerwash_allowed);
    BuildAndInstallDevicePolicy();
  }

  // Helper routine to set DeviceLoginScreenSystemInfoEnforced policy.
  void SetSystemInfoEnforced(bool enabled) {
    em::BooleanPolicyProto* proto =
        device_policy_->payload()
            .mutable_device_login_screen_system_info_enforced();
    proto->set_value(enabled);
    BuildAndInstallDevicePolicy();
  }

  void SetShowNumericKeyboardForPassword(bool show_numeric_keyboard) {
    em::BooleanPolicyProto* proto =
        device_policy_->payload()
            .mutable_device_show_numeric_keyboard_for_password();
    proto->set_value(show_numeric_keyboard);
    BuildAndInstallDevicePolicy();
  }

  void SetDevicePrinterAccessMode(
      em::DevicePrintersAccessModeProto::AccessMode access_mode) {
    em::DevicePrintersAccessModeProto* proto =
        device_policy_->payload().mutable_device_printers_access_mode();
    proto->set_access_mode(access_mode);
  }

  void SetDevicePrintersBlocklist(std::vector<std::string>& values) {
    em::DevicePrintersBlocklistProto* proto =
        device_policy_->payload().mutable_device_printers_blocklist();
    for (auto const& value : values) {
      proto->add_blocklist(value);
    }
  }

  void SetDevicePrintersAllowlist(std::vector<std::string>& values) {
    em::DevicePrintersAllowlistProto* proto =
        device_policy_->payload().mutable_device_printers_allowlist();
    for (auto const& value : values) {
      proto->add_allowlist(value);
    }
  }

  // Helper routine that sets the DeviceScheduledReboot policy.
  void SetDeviceScheduledReboot(const std::string& json_string) {
    em::DeviceScheduledRebootProto* proto =
        device_policy_->payload().mutable_device_scheduled_reboot();
    proto->set_device_scheduled_reboot_settings(json_string);
    BuildAndInstallDevicePolicy();
  }

  void VerifyDevicePrinterList(const char* policy_key,
                               std::vector<std::string>& values) {
    base::Value::List list;
    for (auto const& value : values) {
      list.Append(value);
    }

    base::Value expected_value(std::move(list));
    VerifyPolicyValue(policy_key, &expected_value);
  }

  // Helper routine clear the ShowLowDiskSpaceNotification policy.
  void ClearDeviceShowLowDiskSpaceNotification() {
    device_policy_->payload().clear_device_show_low_disk_space_notification();
    BuildAndInstallDevicePolicy();
  }

  // Helper routine set the ShowLowDiskSpaceNotification policy.
  void SetDeviceShowLowDiskSpaceNotification(bool show) {
    em::DeviceShowLowDiskSpaceNotificationProto* proto =
        device_policy_->payload()
            .mutable_device_show_low_disk_space_notification();
    proto->set_device_show_low_disk_space_notification(show);
    BuildAndInstallDevicePolicy();
  }

  void SetDeviceFamilyLinkAccountsAllowed(bool allow) {
    em::DeviceFamilyLinkAccountsAllowedProto* proto =
        device_policy_->payload().mutable_family_link_accounts_allowed();
    proto->set_family_link_accounts_allowed(allow);
    BuildAndInstallDevicePolicy();
  }

  void AddUserToAllowlist(const std::string& user_id) {
    auto& proto = device_policy_->payload();
    proto.mutable_user_allowlist()->add_user_allowlist(user_id);
    proto.mutable_allow_new_users()->set_allow_new_users(true);
    BuildAndInstallDevicePolicy();
  }

  void VerifyDeviceShowLowDiskSpaceNotification(bool expected) {
    const base::Value expected_value(expected);
    EXPECT_EQ(expected_value,
              *provider_->Get(kDeviceShowLowDiskSpaceNotification));
  }

  base::test::ScopedFeatureList feature_list_;

  ScopedTestingLocalState local_state_;

  std::unique_ptr<DeviceSettingsProvider> provider_;

  base::ScopedPathOverride user_data_dir_override_;
};

// Same as above, but enrolled into an enterprise.
class DeviceSettingsProviderTestEnterprise : public DeviceSettingsProviderTest {
 protected:
  void SetUp() override {
    DeviceSettingsProviderTest::SetUp();
    profile_->ScopedCrosSettingsTestHelper()
        ->InstallAttributes()
        ->SetCloudManaged(policy::PolicyBuilder::kFakeDomain,
                          policy::PolicyBuilder::kFakeDeviceId);
  }
};

TEST_F(DeviceSettingsProviderTest, InitializationTest) {
  // Have the service load a settings blob.
  EXPECT_CALL(*this, SettingChanged(_)).Times(AnyNumber());
  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(this);

  // Verify that the policy blob has been correctly parsed and trusted.
  // The trusted flag should be set before the call to PrepareTrustedValues.
  base::OnceClosure closure = base::DoNothing();
  EXPECT_EQ(CrosSettingsProvider::TRUSTED,
            provider_->PrepareTrustedValues(&closure));
  EXPECT_TRUE(closure);  // Ownership of |closure| was not taken.
  const base::Value* value = provider_->Get(kStatsReportingPref);
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_bool());
  EXPECT_FALSE(value->GetBool());
}

TEST_F(DeviceSettingsProviderTest, InitializationTestUnowned) {
  // Have the service check the key.
  owner_key_util_->Clear();
  ReloadDeviceSettings();

  // The trusted flag should be set before the call to PrepareTrustedValues.
  base::OnceClosure closure = base::DoNothing();
  EXPECT_EQ(CrosSettingsProvider::TRUSTED,
            provider_->PrepareTrustedValues(&closure));
  EXPECT_TRUE(closure);  // Ownership of |closure| was not taken.
  const base::Value* value = provider_->Get(kReleaseChannel);
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_string());
  EXPECT_TRUE(value->GetString().empty());

  // Sets should succeed though and be readable from the cache.
  EXPECT_CALL(*this, SettingChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*this, SettingChanged(kReleaseChannel)).Times(1);
  base::Value new_value("stable-channel");
  provider_->DoSet(kReleaseChannel, new_value);
  Mock::VerifyAndClearExpectations(this);

  // This shouldn't trigger a write.
  session_manager_client_.set_device_policy(std::string());
  FlushDeviceSettings();
  EXPECT_EQ(std::string(), session_manager_client_.device_policy());

  // Verify the change has been applied.
  const base::Value* saved_value = provider_->Get(kReleaseChannel);
  ASSERT_TRUE(saved_value);
  ASSERT_TRUE(saved_value->is_string());
  ASSERT_EQ("stable-channel", saved_value->GetString());
}

TEST_F(DeviceSettingsProviderTestEnterprise, NoPolicyDefaultsOn) {
  // Missing policy should default to reporting enabled for enterprise-enrolled
  // devices, see crbug/456186.
  SetMetricsReportingSettings(REMOVE_METRICS_POLICY);
  const base::Value* saved_value = provider_->Get(kStatsReportingPref);
  ASSERT_TRUE(saved_value);
  ASSERT_TRUE(saved_value->is_bool());
  EXPECT_TRUE(saved_value->GetBool());
}

TEST_F(DeviceSettingsProviderTest, NoPolicyDefaultsOff) {
  // Missing policy should default to reporting enabled for non-enterprise-
  // enrolled devices, see crbug/456186.
  SetMetricsReportingSettings(REMOVE_METRICS_POLICY);
  const base::Value* saved_value = provider_->Get(kStatsReportingPref);
  ASSERT_TRUE(saved_value);
  ASSERT_TRUE(saved_value->is_bool());
  EXPECT_FALSE(saved_value->GetBool());
}

TEST_F(DeviceSettingsProviderTest, SetPrefFailed) {
  SetMetricsReportingSettings(DISABLE_METRICS);

  // If we are not the owner no sets should work.
  base::Value value(true);
  EXPECT_CALL(*this, SettingChanged(kStatsReportingPref)).Times(1);
  provider_->DoSet(kStatsReportingPref, value);
  Mock::VerifyAndClearExpectations(this);

  // This shouldn't trigger a write.
  session_manager_client_.set_device_policy(std::string());
  FlushDeviceSettings();
  EXPECT_EQ(std::string(), session_manager_client_.device_policy());

  // Verify the change has not been applied.
  const base::Value* saved_value = provider_->Get(kStatsReportingPref);
  ASSERT_TRUE(saved_value);
  ASSERT_TRUE(saved_value->is_bool());
  EXPECT_FALSE(saved_value->GetBool());
}

TEST_F(DeviceSettingsProviderTest, SetPrefSucceed) {
  owner_key_util_->ImportPrivateKeyAndSetPublicKey(
      device_policy_->GetSigningKey());
  InitOwner(AccountId::FromUserEmail(device_policy_->policy_data().username()),
            true);
  FlushDeviceSettings();

  base::Value value(true);
  EXPECT_CALL(*this, SettingChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*this, SettingChanged(kStatsReportingPref)).Times(1);
  provider_->DoSet(kStatsReportingPref, value);
  Mock::VerifyAndClearExpectations(this);

  // Process the store.
  session_manager_client_.set_device_policy(std::string());
  FlushDeviceSettings();

  // Verify that the device policy has been adjusted.
  ASSERT_TRUE(device_settings_service_->device_settings());
  EXPECT_TRUE(device_settings_service_->device_settings()
                  ->metrics_enabled()
                  .metrics_enabled());

  // Verify the change has been applied.
  const base::Value* saved_value = provider_->Get(kStatsReportingPref);
  ASSERT_TRUE(saved_value);
  ASSERT_TRUE(saved_value->is_bool());
  EXPECT_TRUE(saved_value->GetBool());
}

TEST_F(DeviceSettingsProviderTest, SetPrefTwice) {
  owner_key_util_->ImportPrivateKeyAndSetPublicKey(
      device_policy_->GetSigningKey());
  InitOwner(AccountId::FromUserEmail(device_policy_->policy_data().username()),
            true);
  FlushDeviceSettings();

  EXPECT_CALL(*this, SettingChanged(_)).Times(AnyNumber());

  base::Value value1("beta");
  provider_->DoSet(kReleaseChannel, value1);
  base::Value value2("dev");
  provider_->DoSet(kReleaseChannel, value2);

  // Let the changes propagate through the system.
  session_manager_client_.set_device_policy(std::string());
  FlushDeviceSettings();

  // Verify the second change has been applied.
  const base::Value* saved_value = provider_->Get(kReleaseChannel);
  EXPECT_EQ(value2, *saved_value);

  Mock::VerifyAndClearExpectations(this);
}

TEST_F(DeviceSettingsProviderTest, PolicyRetrievalFailedBadSignature) {
  base::HistogramTester histogram_tester;
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  device_policy_->policy().set_policy_data_signature("bad signature");
  session_manager_client_.set_device_policy(device_policy_->GetBlob());
  ReloadDeviceSettings();

  // Verify that the cached settings blob is not "trusted".
  EXPECT_EQ(DeviceSettingsService::STORE_VALIDATION_ERROR,
            device_settings_service_->status());
  base::OnceClosure closure = base::DoNothing();
  EXPECT_EQ(CrosSettingsProvider::PERMANENTLY_UNTRUSTED,
            provider_->PrepareTrustedValues(&closure));
  EXPECT_TRUE(closure);  // Ownership of |closure| was not taken.
  histogram_tester.ExpectUniqueSample(
      "Enterprise.DeviceSettings.UpdatedStatus",
      DeviceSettingsService::STORE_VALIDATION_ERROR, /*amount=*/1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.DeviceSettings.MissingPolicyMitigated", 0);
}

TEST_F(DeviceSettingsProviderTest, PolicyRetrievalNoPolicy) {
  base::HistogramTester histogram_tester;
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  session_manager_client_.set_device_policy(std::string());
  ReloadDeviceSettings();

  // Verify that the cached settings blob is not "trusted".
  EXPECT_EQ(DeviceSettingsService::STORE_NO_POLICY,
            device_settings_service_->status());
  base::OnceClosure closure = base::DoNothing();
  EXPECT_EQ(CrosSettingsProvider::PERMANENTLY_UNTRUSTED,
            provider_->PrepareTrustedValues(&closure));
  EXPECT_TRUE(closure);  // Ownership of |closure| was not taken.
  histogram_tester.ExpectUniqueSample("Enterprise.DeviceSettings.UpdatedStatus",
                                      DeviceSettingsService::STORE_NO_POLICY,
                                      /*amount=*/1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.DeviceSettings.MissingPolicyMitigated", 0);
}

TEST_F(DeviceSettingsProviderTest, PolicyRetrievalNoPolicyMitigated) {
  base::HistogramTester histogram_tester;
  profile_->ScopedCrosSettingsTestHelper()
      ->InstallAttributes()
      ->SetConsumerOwned();
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_->GetSigningKey());
  session_manager_client_.set_device_policy(std::string());
  ReloadDeviceSettings();

  // Verify that the cached settings blob is not "trusted".
  EXPECT_EQ(DeviceSettingsService::STORE_NO_POLICY,
            device_settings_service_->status());
  base::OnceClosure closure = base::DoNothing();
  EXPECT_EQ(CrosSettingsProvider::TRUSTED,
            provider_->PrepareTrustedValues(&closure));
  EXPECT_TRUE(closure);  // Ownership of |closure| was not taken.
  histogram_tester.ExpectUniqueSample("Enterprise.DeviceSettings.UpdatedStatus",
                                      DeviceSettingsService::STORE_NO_POLICY,
                                      /*amount=*/1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.DeviceSettings.MissingPolicyMitigated", 1);
}

TEST_F(DeviceSettingsProviderTest, PolicyFailedPermanentlyNotification) {
  base::HistogramTester histogram_tester;
  session_manager_client_.set_device_policy(std::string());

  base::OnceClosure closure = base::BindOnce(
      &DeviceSettingsProviderTest::GetTrustedCallback, base::Unretained(this));

  EXPECT_CALL(*this, GetTrustedCallback());
  EXPECT_EQ(CrosSettingsProvider::TEMPORARILY_UNTRUSTED,
            provider_->PrepareTrustedValues(&closure));
  EXPECT_FALSE(closure);  // Ownership of |closure| was taken.

  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(this);

  closure = base::DoNothing();
  EXPECT_EQ(CrosSettingsProvider::PERMANENTLY_UNTRUSTED,
            provider_->PrepareTrustedValues(&closure));
  EXPECT_TRUE(closure);  // Ownership of |closure| was not taken.
  histogram_tester.ExpectUniqueSample("Enterprise.DeviceSettings.UpdatedStatus",
                                      DeviceSettingsService::STORE_NO_POLICY,
                                      /*amount=*/1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.DeviceSettings.MissingPolicyMitigated", 0);
}

TEST_F(DeviceSettingsProviderTest, PolicyLoadNotification) {
  base::HistogramTester histogram_tester;
  EXPECT_CALL(*this, GetTrustedCallback());

  base::OnceClosure closure = base::BindOnce(
      &DeviceSettingsProviderTest::GetTrustedCallback, base::Unretained(this));
  EXPECT_EQ(CrosSettingsProvider::TEMPORARILY_UNTRUSTED,
            provider_->PrepareTrustedValues(&closure));
  EXPECT_FALSE(closure);  // Ownership of |closure| was taken.

  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(this);
  histogram_tester.ExpectUniqueSample("Enterprise.DeviceSettings.UpdatedStatus",
                                      DeviceSettingsService::STORE_SUCCESS,
                                      /*amount=*/1);
  histogram_tester.ExpectTotalCount(
      "Enterprise.DeviceSettings.MissingPolicyMitigated", 0);
}

TEST_F(DeviceSettingsProviderTest, LegacyDeviceLocalAccounts) {
  em::DeviceLocalAccountInfoProto* account =
      device_policy_->payload().mutable_device_local_accounts()->add_account();
  account->set_deprecated_public_session_id(
      policy::PolicyBuilder::kFakeUsername);
  BuildAndInstallDevicePolicy();

  // On load, the deprecated spec should have been converted to the new format.
  base::Value::List expected_accounts;
  base::Value::Dict entry_dict;
  entry_dict.Set(kAccountsPrefDeviceLocalAccountsKeyId,
                 policy::PolicyBuilder::kFakeUsername);
  entry_dict.Set(
      kAccountsPrefDeviceLocalAccountsKeyType,
      static_cast<int>(policy::DeviceLocalAccountType::kPublicSession));
  expected_accounts.Append(std::move(entry_dict));
  const base::Value* actual_accounts =
      provider_->Get(kAccountsPrefDeviceLocalAccounts);
  EXPECT_EQ(expected_accounts, actual_accounts->GetList());
}

TEST_F(DeviceSettingsProviderTest,
       DeviceLocalAccountsWithoutEphemeralModeField) {
  em::DeviceLocalAccountInfoProto* account =
      device_policy_->payload().mutable_device_local_accounts()->add_account();
  account->set_account_id(kDeviceLocalAccountKioskAccountId);
  account->set_type(em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_KIOSK_APP);

  BuildAndInstallDevicePolicy();

  base::Value::List expected_accounts = base::Value::List().Append(
      base::Value::Dict()
          .Set(kAccountsPrefDeviceLocalAccountsKeyId,
               kDeviceLocalAccountKioskAccountId)
          .Set(kAccountsPrefDeviceLocalAccountsKeyType,
               static_cast<int>(policy::DeviceLocalAccountType::kKioskApp))
          .Set(kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
               static_cast<int>(
                   policy::DeviceLocalAccount::EphemeralMode::kUnset)));

  const base::Value* actual_accounts =
      provider_->Get(kAccountsPrefDeviceLocalAccounts);
  EXPECT_EQ(expected_accounts, actual_accounts->GetList());
}

TEST_F(DeviceSettingsProviderTest, DeviceLocalAccountsWithEphemeralModeField) {
  em::DeviceLocalAccountInfoProto* account =
      device_policy_->payload().mutable_device_local_accounts()->add_account();
  account->set_account_id(kDeviceLocalAccountKioskAccountId);
  account->set_type(
      em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_WEB_KIOSK_APP);
  account->set_ephemeral_mode(
      em::DeviceLocalAccountInfoProto::EPHEMERAL_MODE_ENABLE);

  BuildAndInstallDevicePolicy();

  base::Value::List expected_accounts = base::Value::List().Append(
      base::Value::Dict()
          .Set(kAccountsPrefDeviceLocalAccountsKeyId,
               kDeviceLocalAccountKioskAccountId)
          .Set(kAccountsPrefDeviceLocalAccountsKeyType,
               static_cast<int>(policy::DeviceLocalAccountType::kWebKioskApp))
          .Set(kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
               static_cast<int>(
                   policy::DeviceLocalAccount::EphemeralMode::kEnable)));

  const base::Value* actual_accounts =
      provider_->Get(kAccountsPrefDeviceLocalAccounts);
  EXPECT_EQ(expected_accounts, actual_accounts->GetList());
}

TEST_F(DeviceSettingsProviderTest, DecodeDeviceState) {
  device_policy_->policy_data().mutable_device_state()->set_device_mode(
      em::DeviceState::DEVICE_MODE_DISABLED);
  device_policy_->policy_data()
      .mutable_device_state()
      ->mutable_disabled_state()
      ->set_message(kDisabledMessage);
  BuildAndInstallDevicePolicy();

  // Verify that the device state has been decoded correctly.
  EXPECT_TRUE(provider_->Get(kDeviceDisabled));
  const base::Value expected_disabled_message_value(kDisabledMessage);
  EXPECT_EQ(expected_disabled_message_value,
            *provider_->Get(kDeviceDisabledMessage));

  // Verify that a change to the device state triggers a notification.
  device_policy_->policy_data().mutable_device_state()->clear_device_mode();
  BuildAndInstallDevicePolicy();

  // Verify that the updated state has been decoded correctly.
  EXPECT_FALSE(provider_->Get(kDeviceDisabled));
}

TEST_F(DeviceSettingsProviderTest, DecodeReportingSettings) {
  // Turn on all reporting and verify that the reporting settings have been
  // decoded correctly.
  constexpr int status_frequency = 500000000;
  SetReportingSettings(true, status_frequency);
  VerifyReportingSettings(true, status_frequency);

  // Turn off all reporting and verify that the settings are decoded
  // correctly.
  SetReportingSettings(false, status_frequency);
  VerifyReportingSettings(false, status_frequency);
}

TEST_F(DeviceSettingsProviderTest,
       DecodeReportingSignalStrengthEventDrivenTelemetrySetting) {
  em::DeviceReportingProto* proto =
      device_policy_->payload().mutable_device_reporting();
  proto->mutable_report_signal_strength_event_driven_telemetry()->add_entries(
      "https_latency");
  proto->mutable_report_signal_strength_event_driven_telemetry()->add_entries(
      "network_telemetry");

  BuildAndInstallDevicePolicy();

  base::Value::List signal_strength_telemetry_list;
  signal_strength_telemetry_list.Append("https_latency");
  signal_strength_telemetry_list.Append("network_telemetry");
  base::Value signal_strength_telemetry_list_value =
      base::Value(std::move(signal_strength_telemetry_list));

  VerifyPolicyValue(kReportDeviceSignalStrengthEventDrivenTelemetry,
                    &signal_strength_telemetry_list_value);
}

TEST_F(DeviceSettingsProviderTest, DecodeHeartbeatSettings) {
  // Turn on heartbeats and verify that the heartbeat settings have been
  // decoded correctly.
  const int heartbeat_frequency = 50000;
  SetHeartbeatSettings(true, heartbeat_frequency);
  VerifyHeartbeatSettings(true, heartbeat_frequency);

  // Turn off all reporting and verify that the settings are decoded
  // correctly.
  SetHeartbeatSettings(false, heartbeat_frequency);
  VerifyHeartbeatSettings(false, heartbeat_frequency);
}

TEST_F(DeviceSettingsProviderTest, DecodeDomainAutoComplete) {
  // By default LoginScreenDomainAutoComplete policy should not be set.
  VerifyDomainAutoComplete(nullptr);

  // Empty string means that the policy is not set.
  SetDomainAutoComplete("");
  VerifyDomainAutoComplete(nullptr);

  // Check some meaningful value. Policy should be set.
  const std::string domain = "domain.test";
  const base::Value domain_value(domain);
  SetDomainAutoComplete(domain);
  VerifyDomainAutoComplete(&domain_value);
}

TEST_F(DeviceSettingsProviderTest, EmptyAllowedConnectionTypesForUpdate) {
  // By default AllowedConnectionTypesForUpdate policy should not be set.
  VerifyPolicyValue(kAllowedConnectionTypesForUpdate, nullptr);

  // In case of empty list policy should not be set.
  const std::vector<em::AutoUpdateSettingsProto::ConnectionType> no_values = {};
  SetAutoUpdateConnectionTypes(no_values);
  VerifyPolicyValue(kAllowedConnectionTypesForUpdate, nullptr);

  const std::vector<em::AutoUpdateSettingsProto::ConnectionType> single_value =
      {em::AutoUpdateSettingsProto::CONNECTION_TYPE_ETHERNET};
  // Check some meaningful value. Policy should be set.
  SetAutoUpdateConnectionTypes(single_value);
  base::Value::List allowed_connections;
  allowed_connections.Append(0);
  VerifyPolicyList(kAllowedConnectionTypesForUpdate, allowed_connections);
}

TEST_F(DeviceSettingsProviderTest, DecodeHostnameTemplate) {
  // By default DeviceHostnameTemplate policy should not be set.
  VerifyPolicyValue(kDeviceHostnameTemplate, nullptr);

  // Empty string means that the policy is not set.
  SetHostnameTemplate("");
  VerifyPolicyValue(kDeviceHostnameTemplate, nullptr);

  // Check some meaningful value. Policy should be set.
  const std::string hostname_template = "chromebook-${ASSET_ID}";
  const base::Value template_value(hostname_template);
  SetHostnameTemplate(hostname_template);
  VerifyPolicyValue(kDeviceHostnameTemplate, &template_value);
}

TEST_F(DeviceSettingsProviderTest, DecodeLogUploadSettings) {
  SetLogUploadSettings(true);
  VerifyLogUploadSettings(true);

  SetLogUploadSettings(false);
  VerifyLogUploadSettings(false);
}

// Test invalid cases.
TEST_F(DeviceSettingsProviderTest, DeviceAutoUpdateTimeRestrictionsEmpty) {
  // Policy should not be set by default
  VerifyPolicyValue(kDeviceAutoUpdateTimeRestrictions, nullptr);

  // Empty string should not be considered valid
  SetDeviceAutoUpdateTimeRestrictions("");
  VerifyPolicyValue(kDeviceAutoUpdateTimeRestrictions, nullptr);
}

// JSON with required fields that have values out of bounds should be dropped.
TEST_F(DeviceSettingsProviderTest,
       DeviceAutoUpdateTimeRestrictionsInvalidField) {
  // JSON with an invalid field should be considered invalid.
  const std::string invalid_field =
      "[{\"start\": {\"day_of_week\": \"Monday\", \"hours\": 10, \"minutes\": "
      "50}, \"end\": {\"day_of_week\": \"Wednesday\", \"hours\": 1, "
      "\"minutes\": -20}}]";
  SetDeviceAutoUpdateTimeRestrictions(invalid_field);
  VerifyPolicyValue(kDeviceAutoUpdateTimeRestrictions, nullptr);
}

// Valid JSON with extra fields should be considered valid and saved with
// dropped extra fields.
TEST_F(DeviceSettingsProviderTest, DeviceAutoUpdateTimeRestrictionsExtra) {
  const std::string extra_field =
      "[{\"start\": {\"day_of_week\": \"Monday\", \"hours\": 10, \"minutes\": "
      "50}, \"end\": {\"day_of_week\": \"Wednesday\", \"hours\": 1, "
      "\"minutes\": 20, \"extra\": 50}}]";
  base::Value::List test_list;
  base::Value::Dict interval;
  interval.SetByDottedPath("start.day_of_week", "Monday");
  interval.SetByDottedPath("start.hours", 10);
  interval.SetByDottedPath("start.minutes", 50);
  interval.SetByDottedPath("end.day_of_week", "Wednesday");
  interval.SetByDottedPath("end.hours", 1);
  interval.SetByDottedPath("end.minutes", 20);
  test_list.Append(std::move(interval));
  SetDeviceAutoUpdateTimeRestrictions(extra_field);
  VerifyPolicyList(kDeviceAutoUpdateTimeRestrictions, test_list);
}

// Check valid JSON for DeviceScheduledUpdateCheck.
TEST_F(DeviceSettingsProviderTest, DeviceScheduledUpdateCheckTests) {
  const std::string json_string =
      "{\"update_check_time\": {\"hour\": 23, \"minute\": 35}, "
      "\"frequency\": \"DAILY\", \"day_of_week\": \"MONDAY\",  "
      "\"day_of_month\": 15}";
  base::Value::Dict expected_dict;
  expected_dict.SetByDottedPath("update_check_time.hour", 23);
  expected_dict.SetByDottedPath("update_check_time.minute", 35);
  expected_dict.Set("frequency", "DAILY");
  expected_dict.Set("day_of_week", "MONDAY");
  expected_dict.Set("day_of_month", 15);
  base::Value expected_value(std::move(expected_dict));
  SetDeviceScheduledUpdateCheck(json_string);
  VerifyPolicyValue(kDeviceScheduledUpdateCheck, &expected_value);
}

TEST_F(DeviceSettingsProviderTest, DecodePluginVmAllowedSetting) {
  SetPluginVmAllowedSetting(true);
  EXPECT_EQ(base::Value(true), *provider_->Get(kPluginVmAllowed));

  SetPluginVmAllowedSetting(false);
  EXPECT_EQ(base::Value(false), *provider_->Get(kPluginVmAllowed));
}

TEST_F(DeviceSettingsProviderTest, DeviceRebootAfterUserSignout) {
  using PolicyProto = em::DeviceRebootOnUserSignoutProto;

  VerifyPolicyValue(kDeviceRebootOnUserSignout, nullptr);

  {
    SetDeviceRebootOnUserSignout(PolicyProto::NEVER);
    base::Value expected_value(PolicyProto::NEVER);
    VerifyPolicyValue(kDeviceRebootOnUserSignout, &expected_value);
  }

  {
    SetDeviceRebootOnUserSignout(PolicyProto::ARC_SESSION);
    base::Value expected_value(PolicyProto::ARC_SESSION);
    VerifyPolicyValue(kDeviceRebootOnUserSignout, &expected_value);
  }

  {
    SetDeviceRebootOnUserSignout(PolicyProto::ALWAYS);
    base::Value expected_value(PolicyProto::ALWAYS);
    VerifyPolicyValue(kDeviceRebootOnUserSignout, &expected_value);
  }

  {
    SetDeviceRebootOnUserSignout(PolicyProto::VM_STARTED_OR_ARC_SESSION);
    base::Value expected_value(PolicyProto::VM_STARTED_OR_ARC_SESSION);
    VerifyPolicyValue(kDeviceRebootOnUserSignout, &expected_value);
  }
}

TEST_F(DeviceSettingsProviderTest, DeviceDockMacAddressSourceSetting) {
  const base::Value default_value(3);
  VerifyPolicyValue(kDeviceDockMacAddressSource, &default_value);

  SetDeviceDockMacAddressSourceSetting(
      em::DeviceDockMacAddressSourceProto::DEVICE_DOCK_MAC_ADDRESS);
  EXPECT_EQ(base::Value(1), *provider_->Get(kDeviceDockMacAddressSource));

  SetDeviceDockMacAddressSourceSetting(
      em::DeviceDockMacAddressSourceProto::DEVICE_NIC_MAC_ADDRESS);
  EXPECT_EQ(base::Value(2), *provider_->Get(kDeviceDockMacAddressSource));

  SetDeviceDockMacAddressSourceSetting(
      em::DeviceDockMacAddressSourceProto::DOCK_NIC_MAC_ADDRESS);
  EXPECT_EQ(base::Value(3), *provider_->Get(kDeviceDockMacAddressSource));
}

TEST_F(DeviceSettingsProviderTest,
       DeviceSecondFactorAuthenticationModeSetting) {
  VerifyPolicyValue(kDeviceSecondFactorAuthenticationMode, nullptr);

  SetDeviceSecondFactorAuthenticationModeSetting(
      em::DeviceSecondFactorAuthenticationProto::UNSET);
  EXPECT_EQ(base::Value(0),
            *provider_->Get(kDeviceSecondFactorAuthenticationMode));

  SetDeviceSecondFactorAuthenticationModeSetting(
      em::DeviceSecondFactorAuthenticationProto::DISABLED);
  EXPECT_EQ(base::Value(1),
            *provider_->Get(kDeviceSecondFactorAuthenticationMode));

  SetDeviceSecondFactorAuthenticationModeSetting(
      em::DeviceSecondFactorAuthenticationProto::U2F);
  EXPECT_EQ(base::Value(2),
            *provider_->Get(kDeviceSecondFactorAuthenticationMode));

  SetDeviceSecondFactorAuthenticationModeSetting(
      em::DeviceSecondFactorAuthenticationProto::U2F_EXTENDED);
  EXPECT_EQ(base::Value(3),
            *provider_->Get(kDeviceSecondFactorAuthenticationMode));
}

TEST_F(DeviceSettingsProviderTest, DevicePowerwashAllowed) {
  // Policy should be set to true by default.
  base::Value default_value(true);
  VerifyPolicyValue(kDevicePowerwashAllowed, &default_value);

  SetDevicePowerwashAllowed(true);
  EXPECT_EQ(base::Value(true), *provider_->Get(kDevicePowerwashAllowed));

  SetDevicePowerwashAllowed(false);
  EXPECT_EQ(base::Value(false), *provider_->Get(kDevicePowerwashAllowed));
}

TEST_F(DeviceSettingsProviderTest, DeviceLoginScreenSystemInfoEnforced) {
  // Policy should not be set by default.
  VerifyPolicyValue(kDeviceLoginScreenSystemInfoEnforced, nullptr);

  SetSystemInfoEnforced(true);
  EXPECT_EQ(base::Value(true),
            *provider_->Get(kDeviceLoginScreenSystemInfoEnforced));

  SetSystemInfoEnforced(false);
  EXPECT_EQ(base::Value(false),
            *provider_->Get(kDeviceLoginScreenSystemInfoEnforced));
}

TEST_F(DeviceSettingsProviderTest, DeviceShowNumericKeyboardForPassword) {
  // Policy should not be set by default.
  VerifyPolicyValue(kDeviceShowNumericKeyboardForPassword, nullptr);

  SetShowNumericKeyboardForPassword(true);
  EXPECT_EQ(base::Value(true),
            *provider_->Get(kDeviceShowNumericKeyboardForPassword));

  SetShowNumericKeyboardForPassword(false);
  EXPECT_EQ(base::Value(false),
            *provider_->Get(kDeviceShowNumericKeyboardForPassword));
}

TEST_F(DeviceSettingsProviderTest, DevicePrintersAccessMode_empty) {
  // Policy should be ACCESS_MODE_ALL by default.
  base::Value default_value(em::DevicePrintersAccessModeProto::ACCESS_MODE_ALL);
  VerifyPolicyValue(kDevicePrintersAccessMode, &default_value);
}

TEST_F(DeviceSettingsProviderTest, DevicePrintersAccessMode_allowlist) {
  SetDevicePrinterAccessMode(
      em::DevicePrintersAccessModeProto::ACCESS_MODE_ALLOWLIST);
  BuildAndInstallDevicePolicy();
  base::Value expected_value(
      em::DevicePrintersAccessModeProto::ACCESS_MODE_ALLOWLIST);
  VerifyPolicyValue(kDevicePrintersAccessMode, &expected_value);
}

TEST_F(DeviceSettingsProviderTest, DevicePrintersBlocklist_empty) {
  // Policy should not be set by default
  VerifyPolicyValue(kDevicePrintersBlocklist, nullptr);
}

TEST_F(DeviceSettingsProviderTest, DevicePrintersBlocklist_blocklist) {
  std::vector<std::string> values = {"foo", "bar"};

  // If the blocklist only is set, use that.
  SetDevicePrintersBlocklist(values);
  BuildAndInstallDevicePolicy();
  VerifyDevicePrinterList(kDevicePrintersBlocklist, values);
}

TEST_F(DeviceSettingsProviderTest, DevicePrintersAllowlist_empty) {
  // Policy should not be set by default.
  VerifyPolicyValue(kDevicePrintersAllowlist, nullptr);
}

TEST_F(DeviceSettingsProviderTest, DevicePrintersAllowlist_allowlist) {
  std::vector<std::string> values = {"foo", "bar"};

  // If the allowlist only is set, use that.
  SetDevicePrintersAllowlist(values);
  BuildAndInstallDevicePolicy();
  VerifyDevicePrinterList(kDevicePrintersAllowlist, values);
}

TEST_F(DeviceSettingsProviderTest,
       DeviceShowLowDiskSpaceNotificationDefaultTrue) {
  ClearDeviceShowLowDiskSpaceNotification();
  // Missing policy should default to showing the low disk space
  // notification for consumer devices.
  VerifyDeviceShowLowDiskSpaceNotification(true);
}

TEST_F(DeviceSettingsProviderTestEnterprise,
       DeviceShowLowDiskSpaceNotificationDefaultFalse) {
  ClearDeviceShowLowDiskSpaceNotification();
  // Missing policy should default to suppressing the low disk space
  // notification for enrolled devices by default.
  VerifyDeviceShowLowDiskSpaceNotification(false);
}

TEST_F(DeviceSettingsProviderTestEnterprise,
       DeviceShowLowDiskSpaceNotification) {
  // Showing the low disk space notification can be controlled by policy.
  SetDeviceShowLowDiskSpaceNotification(true);
  VerifyDeviceShowLowDiskSpaceNotification(true);

  SetDeviceShowLowDiskSpaceNotification(false);
  VerifyDeviceShowLowDiskSpaceNotification(false);
}

// Tests DeviceFamilyLinkAccountsAllowed policy with the feature disabled.
// The policy should have no effect.
TEST_F(DeviceSettingsProviderTest, DeviceFamilyLinkAccountsAllowedDisabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kFamilyLinkOnSchoolDevice);

  base::Value default_value(false);
  VerifyPolicyValue(kAccountsPrefFamilyLinkAccountsAllowed, &default_value);

  // Family Link allowed with allowlist set, but the feature is disabled.
  SetDeviceFamilyLinkAccountsAllowed(true);
  AddUserToAllowlist("*@managedchrome.com");
  EXPECT_EQ(base::Value(false),
            *provider_->Get(kAccountsPrefFamilyLinkAccountsAllowed));
}

// Tests DeviceFamilyLinkAccountsAllowed policy with the feature enabled.
TEST_F(DeviceSettingsProviderTest, DeviceFamilyLinkAccountsAllowedEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kFamilyLinkOnSchoolDevice);

  base::Value default_value(false);
  VerifyPolicyValue(kAccountsPrefFamilyLinkAccountsAllowed, &default_value);

  // Family Link allowed, but no allowlist set.
  SetDeviceFamilyLinkAccountsAllowed(true);
  EXPECT_EQ(base::Value(false),
            *provider_->Get(kAccountsPrefFamilyLinkAccountsAllowed));

  // Family Link allowed with allowlist set.
  AddUserToAllowlist("*@managedchrome.com");
  EXPECT_EQ(base::Value(true),
            *provider_->Get(kAccountsPrefFamilyLinkAccountsAllowed));

  // Family Link disallowed with allowlist set.
  SetDeviceFamilyLinkAccountsAllowed(false);
  EXPECT_EQ(base::Value(false),
            *provider_->Get(kAccountsPrefFamilyLinkAccountsAllowed));
}

TEST_F(DeviceSettingsProviderTest, FeatureFlags) {
  EXPECT_EQ(nullptr, provider_->Get(kFeatureFlags));

  device_policy_->payload().mutable_feature_flags()->add_feature_flags("foo");
  BuildAndInstallDevicePolicy();

  base::Value::List expected_feature_flags;
  expected_feature_flags.Append("foo");
  EXPECT_EQ(expected_feature_flags, provider_->Get(kFeatureFlags)->GetList());
}

TEST_F(DeviceSettingsProviderTest, DeviceAllowedBluetoothServices) {
  em::DeviceAllowedBluetoothServicesProto* proto =
      device_policy_->payload().mutable_device_allowed_bluetooth_services();
  proto->add_allowlist("0x1124");
  BuildAndInstallDevicePolicy();
  base::Value::List allowlist;
  allowlist.Append("0x1124");
  EXPECT_EQ(allowlist,
            provider_->Get(kDeviceAllowedBluetoothServices)->GetList());
}

// Check valid JSON for DeviceScheduledReboot.
TEST_F(DeviceSettingsProviderTest, DeviceScheduledReboot) {
  const std::string json_string =
      "{\"reboot_time\": {\"hour\": 22, \"minute\": 30}, "
      "\"frequency\": \"MONTHLY\", \"day_of_week\": \"MONDAY\", "
      "\"day_of_month\": 15}";
  base::Value::Dict expected_dict;
  expected_dict.SetByDottedPath("reboot_time.hour", 22);
  expected_dict.SetByDottedPath("reboot_time.minute", 30);
  expected_dict.Set("frequency", "MONTHLY");
  expected_dict.Set("day_of_week", "MONDAY");
  expected_dict.Set("day_of_month", 15);
  base::Value expected_value(std::move(expected_dict));
  SetDeviceScheduledReboot(json_string);
  VerifyPolicyValue(kDeviceScheduledReboot, &expected_value);
}

// Checks that content_protection decodes correctly.
TEST_F(DeviceSettingsProviderTest, DecodeContentProtectionDefault) {
  BuildAndInstallDevicePolicy();
  const base::Value* value =
      provider_->Get(kAttestationForContentProtectionEnabled);
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_bool());
  EXPECT_TRUE(value->GetBool());
}

// Checks that content_protection decodes correctly.
TEST_F(DeviceSettingsProviderTest, DecodeContentProtectionEnable) {
  SetContentProtection(true);
  const base::Value* value =
      provider_->Get(kAttestationForContentProtectionEnabled);
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_bool());
  EXPECT_TRUE(value->GetBool());
}

// Checks that content_protection decodes correctly.
TEST_F(DeviceSettingsProviderTest, DecodeContentProtectionDisable) {
  SetContentProtection(false);
  const base::Value* value =
      provider_->Get(kAttestationForContentProtectionEnabled);
  ASSERT_TRUE(value);
  ASSERT_TRUE(value->is_bool());
  EXPECT_FALSE(value->GetBool());
}

TEST_F(DeviceSettingsProviderTest, DeviceRestrictedManagedGuestSessionEnabled) {
  device_policy_->payload()
      .mutable_device_restricted_managed_guest_session_enabled()
      ->set_enabled(true);
  BuildAndInstallDevicePolicy();
  EXPECT_EQ(base::Value(true),
            *provider_->Get(kDeviceRestrictedManagedGuestSessionEnabled));
}

TEST_F(DeviceSettingsProviderTest,
       DeviceRestrictedManagedGuestSessionDisabled) {
  device_policy_->payload()
      .mutable_device_restricted_managed_guest_session_enabled()
      ->set_enabled(false);
  BuildAndInstallDevicePolicy();
  EXPECT_EQ(base::Value(false),
            *provider_->Get(kDeviceRestrictedManagedGuestSessionEnabled));
}

TEST_F(DeviceSettingsProviderTest, KioskCRXManifestUpdateURLIgnoredEnabled) {
  device_policy_->payload()
      .mutable_kiosk_crx_manifest_update_url_ignored()
      ->set_value(true);
  BuildAndInstallDevicePolicy();
  EXPECT_EQ(base::Value(true),
            *provider_->Get(kKioskCRXManifestUpdateURLIgnored));
}

TEST_F(DeviceSettingsProviderTest, KioskCRXManifestUpdateURLIgnoredDisabled) {
  device_policy_->payload()
      .mutable_kiosk_crx_manifest_update_url_ignored()
      ->set_value(false);
  BuildAndInstallDevicePolicy();
  EXPECT_EQ(base::Value(false),
            *provider_->Get(kKioskCRXManifestUpdateURLIgnored));
}

TEST_F(DeviceSettingsProviderTest, DeviceEncryptedReportingPipelineEnabled) {
  device_policy_->payload()
      .mutable_device_encrypted_reporting_pipeline_enabled()
      ->set_enabled(true);
  BuildAndInstallDevicePolicy();
  EXPECT_EQ(base::Value(true),
            *provider_->Get(kDeviceEncryptedReportingPipelineEnabled));
}

TEST_F(DeviceSettingsProviderTest, DeviceEncryptedReportingPipelineDisabled) {
  device_policy_->payload()
      .mutable_device_encrypted_reporting_pipeline_enabled()
      ->set_enabled(false);
  BuildAndInstallDevicePolicy();
  EXPECT_EQ(base::Value(false),
            *provider_->Get(kDeviceEncryptedReportingPipelineEnabled));
}

TEST_F(DeviceSettingsProviderTest, DevicePrintingClientNameTemplateUnset) {
  device_policy_->payload().clear_device_printing_client_name_template();
  BuildAndInstallDevicePolicy();
  EXPECT_FALSE(provider_->Get(kDevicePrintingClientNameTemplate));
}

TEST_F(DeviceSettingsProviderTest, DevicePrintingClientNameTemplate) {
  device_policy_->payload()
      .mutable_device_printing_client_name_template()
      ->set_value("chromebook-${DEVICE_ASSET_ID}");
  BuildAndInstallDevicePolicy();
  EXPECT_EQ(base::Value("chromebook-${DEVICE_ASSET_ID}"),
            *provider_->Get(kDevicePrintingClientNameTemplate));
}

TEST_F(DeviceSettingsProviderTest, DeviceSystemAecEnabled) {
  device_policy_->payload()
      .mutable_device_system_aec_enabled()
      ->set_device_system_aec_enabled(true);
  BuildAndInstallDevicePolicy();
  EXPECT_EQ(base::Value(true), *provider_->Get(kDeviceSystemAecEnabled));
}

TEST_F(DeviceSettingsProviderTest, DeviceSystemAecDisabled) {
  device_policy_->payload()
      .mutable_device_system_aec_enabled()
      ->set_device_system_aec_enabled(false);
  BuildAndInstallDevicePolicy();
  EXPECT_EQ(base::Value(false), *provider_->Get(kDeviceSystemAecEnabled));
}

TEST_F(DeviceSettingsProviderTest, DeviceHindiInscriptLayoutEnabled) {
  device_policy_->payload()
      .mutable_device_hindi_inscript_layout_enabled()
      ->set_enabled(true);
  BuildAndInstallDevicePolicy();
  EXPECT_EQ(base::Value(true),
            *provider_->Get(kDeviceHindiInscriptLayoutEnabled));
}

TEST_F(DeviceSettingsProviderTest, DeviceDlcPredownloadListUnset) {
  // Device setting must be unset if the policy is not set.
  VerifyPolicyValue(kDeviceDlcPredownloadList, nullptr);
}

TEST_F(DeviceSettingsProviderTest, DeviceDlcPredownloadListEmpty) {
  // Device setting must be unset if there are no DLCs to pre download.
  device_policy_->payload().clear_device_dlc_predownload_list();
  BuildAndInstallDevicePolicy();
  VerifyPolicyValue(kDeviceDlcPredownloadList, nullptr);
}

TEST_F(DeviceSettingsProviderTest, DeviceDlcPredownloadListNonempty) {
  device_policy_->payload()
      .mutable_device_dlc_predownload_list()
      ->mutable_value()
      ->add_entries("scanner_drivers");

  BuildAndInstallDevicePolicy();

  VerifyPolicyList(kDeviceDlcPredownloadList,
                   base::Value::List().Append("sane-backends-pfu"));
}

TEST_F(DeviceSettingsProviderTest, DeviceDlcPredownloadListInvalidDlc) {
  device_policy_->payload()
      .mutable_device_dlc_predownload_list()
      ->mutable_value()
      ->add_entries("scanner_drivers");
  device_policy_->payload()
      .mutable_device_dlc_predownload_list()
      ->mutable_value()
      ->add_entries("invalid_dlc_name");

  BuildAndInstallDevicePolicy();

  // Device setting must contain only the valid DLCs that can be pre downloaded.
  VerifyPolicyList(kDeviceDlcPredownloadList,
                   base::Value::List().Append("sane-backends-pfu"));
}

TEST_F(DeviceSettingsProviderTest, DeviceDlcPredownloadListDuplicateDlc) {
  device_policy_->payload()
      .mutable_device_dlc_predownload_list()
      ->mutable_value()
      ->add_entries("scanner_drivers");
  device_policy_->payload()
      .mutable_device_dlc_predownload_list()
      ->mutable_value()
      ->add_entries("scanner_drivers");

  BuildAndInstallDevicePolicy();

  // Device setting must not contain any duplicate values.
  VerifyPolicyList(kDeviceDlcPredownloadList,
                   base::Value::List().Append("sane-backends-pfu"));
}

TEST_F(DeviceSettingsProviderTest, DeviceExtendedAutoUpdateEnabledValueSet) {
  device_policy_->payload()
      .mutable_deviceextendedautoupdateenabled()
      ->set_value(true);
  BuildAndInstallDevicePolicy();

  const base::Value* actual_value =
      provider_->Get(kDeviceExtendedAutoUpdateEnabled);

  EXPECT_TRUE(actual_value->GetBool());
}

TEST_F(DeviceSettingsProviderTest, DeviceExtendedAutoUpdateEnabledValueUnset) {
  device_policy_->payload().clear_deviceextendedautoupdateenabled();
  BuildAndInstallDevicePolicy();

  const base::Value* actual_value =
      provider_->Get(kDeviceExtendedAutoUpdateEnabled);

  EXPECT_FALSE(actual_value);
}

TEST_F(DeviceSettingsProviderTest, DeviceExtensionsSystemLogEnabled) {
  device_policy_->payload()
      .mutable_deviceextensionssystemlogenabled()
      ->set_value(true);
  BuildAndInstallDevicePolicy();

  const base::Value* actual_value =
      provider_->Get(kDeviceExtensionsSystemLogEnabled);

  EXPECT_TRUE(actual_value->GetBool());
}

}  // namespace ash
