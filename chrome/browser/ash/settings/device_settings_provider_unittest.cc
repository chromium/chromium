// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/settings/device_settings_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_path_override.h"
#include "base/values.h"
#include "chrome/browser/ash/settings/device_settings_test_helper.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/user.h"
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

}  // namespace

class DeviceSettingsProviderTest : public DeviceSettingsTestBase {
 public:
  MOCK_METHOD1(SettingChanged, void(const std::string&));
  MOCK_METHOD0(GetTrustedCallback, void(void));

 protected:
  DeviceSettingsProviderTest()
      : local_state_(TestingBrowserProcess::GetGlobal()),
        user_data_dir_override_(chrome::DIR_USER_DATA) {}

  void SetUp() override {
    DeviceSettingsTestBase::SetUp();

    EXPECT_CALL(*this, SettingChanged(_)).Times(AnyNumber());
    provider_.reset(new DeviceSettingsProvider(
        base::BindRepeating(&DeviceSettingsProviderTest::SettingChanged,
                            base::Unretained(this)),
        device_settings_service_.get(), local_state_.Get()));
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
    proto->set_report_boot_mode(enable_reporting);
    proto->set_report_location(enable_reporting);
    proto->set_report_network_interfaces(enable_reporting);
    proto->set_report_users(enable_reporting);
    proto->set_report_hardware_status(enable_reporting);
    proto->set_report_session_status(enable_reporting);
    proto->set_report_graphics_status(enable_reporting);
    proto->set_report_crash_report_info(enable_reporting);
    proto->set_report_os_update_status(enable_reporting);
    proto->set_report_running_kiosk_app(enable_reporting);
    proto->set_report_power_status(enable_reporting);
    proto->set_report_storage_status(enable_reporting);
    proto->set_report_board_status(enable_reporting);
    proto->set_report_app_info(enable_reporting);
    proto->set_report_print_jobs(enable_reporting);
    proto->set_device_status_frequency(frequency);
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
        kReportDeviceVersionInfo,
        kReportDeviceActivityTimes,
        kReportDeviceBoardStatus,
        kReportDeviceBootMode,
        // Device location reporting is not currently supported.
        // kReportDeviceLocation,
        kReportDeviceNetworkInterfaces,
        kReportDeviceUsers,
        kReportDeviceHardwareStatus,
        kReportDevicePowerStatus,
        kReportDeviceStorageStatus,
        kReportDeviceSessionStatus,
        kReportDeviceGraphicsStatus,
        kReportDeviceCrashReportInfo,
        kReportDeviceAppInfo,
        kReportDevicePrintJobs,
        kReportOsUpdateStatus,
        kReportRunningKioskApp,
    };

    const base::Value expected_enable_value(expected_enable_state);
    for (auto* setting : reporting_settings) {
      EXPECT_EQ(expected_enable_value, *provider_->Get(setting))
          << "Value for " << setting << " does not match expected";
    }
    const base::Value expected_frequency_value(expected_frequency);
    EXPECT_EQ(expected_frequency_value,
              *provider_->Get(kReportUploadFrequency));
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
    if (ptr_to_expected_value)
      EXPECT_EQ(*ptr_to_expected_value, *provider_->Get(policy_key));
    else
      EXPECT_EQ(nullptr, provider_->Get(policy_key));
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

  // Helper routine to set HostnameTemplate policy.
  void SetHostnameTemplate(const std::string& hostname_template) {
    em::NetworkHostnameProto* proto =
        device_policy_->payload().mutable_network_hostname();
    proto->set_device_hostname_template(hostname_template);
    BuildAndInstallDevicePolicy();
  }

  // Helper routine to set the DeviceSamlLoginAuthenticationType policy.
  void SetSamlLoginAuthenticationType(
      em::SamlLoginAuthenticationTypeProto::Type value) {
    em::SamlLoginAuthenticationTypeProto* proto =
        device_policy_->payload().mutable_saml_login_authentication_type();
    proto->set_saml_login_authentication_type(value);
    BuildAndInstallDevicePolicy();
  }

  // Helper routine that sets the device DeviceAutoUpdateTimeRestricitons policy
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

  void SetPluginVmLicenseKeySetting(const std::string& plugin_vm_license_key) {
    em::PluginVmLicenseKeyProto* proto =
        device_policy_->payload().mutable_plugin_vm_license_key();
    proto->set_plugin_vm_license_key(plugin_vm_license_key);
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

  // Helper routine that sets the device DeviceWilcoDtcAllowed policy.
  void SetDeviceWilcoDtcAllowedSetting(bool device_wilco_dtc_allowed) {
    em::DeviceWilcoDtcAllowedProto* proto =
        device_policy_->payload().mutable_device_wilco_dtc_allowed();
    proto->set_device_wilco_dtc_allowed(device_wilco_dtc_allowed);
    BuildAndInstallDevicePolicy();
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

  void SetNativeDevicePrinterAccessMode(
      em::DeviceNativePrintersAccessModeProto::AccessMode access_mode) {
    em::DeviceNativePrintersAccessModeProto* proto =
        device_policy_->payload().mutable_native_device_printers_access_mode();
    proto->set_access_mode(access_mode);
  }

  void SetDevicePrinterAccessMode(
      em::DevicePrintersAccessModeProto::AccessMode access_mode) {
    em::DevicePrintersAccessModeProto* proto =
        device_policy_->payload().mutable_device_printers_access_mode();
    proto->set_access_mode(access_mode);
  }

  void SetNativeDevicePrintersBlacklist(std::vector<std::string>& values) {
    em::DeviceNativePrintersBlacklistProto* proto =
        device_policy_->payload().mutable_native_device_printers_blacklist();
    for (auto const& value : values) {
      proto->add_blacklist(value);
    }
  }

  void SetDevicePrintersBlocklist(std::vector<std::string>& values) {
    em::DevicePrintersBlocklistProto* proto =
        device_policy_->payload().mutable_device_printers_blocklist();
    for (auto const& value : values) {
      proto->add_blocklist(value);
    }
  }

  void SetNativeDevicePrintersWhitelist(std::vector<std::string>& values) {
    em::DeviceNativePrintersWhitelistProto* proto =
        device_policy_->payload().mutable_native_device_printers_whitelist();
    for (auto const& value : values) {
      proto->add_whitelist(value);
    }
  }

  void SetDevicePrintersAllowlist(std::vector<std::string>& values) {
    em::DevicePrintersAllowlistProto* proto =
        device_policy_->payload().mutable_device_printers_allowlist();
    for (auto const& value : values) {
      proto->add_allowlist(value);
    }
  }

  void VerifyDevicePrinterList(const char* policy_key,
                               std::vector<std::string>& values) {
    base::Value list(base::Value::Type::LIST);
    for (auto const& value : values) {
      list.Append(value);
    }

    VerifyPolicyValue(policy_key, &list);
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
    em::UserAllowlistProto* proto =
        device_policy_->payload().mutable_user_allowlist();
    proto->add_user_allowlist(user_id);
    BuildAndInstallDevicePolicy();
  }

  void VerifyDeviceShowLowDiskSpaceNotification(bool expected) {
    const base::Value expected_value(expected);
    EXPECT_EQ(expected_value,
              *provider_->Get(kDeviceShowLowDiskSpaceNotification));
  }

  ScopedTestingLocalState local_state_;

  std::unique_ptr<DeviceSettingsProvider> provider_;

  base::ScopedPathOverride user_data_dir_override_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceSettingsProviderTest);
};

// Same as above, but enrolled into an enterprise
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
  std::string string_value;
  EXPECT_TRUE(value->GetAsString(&string_value));
  EXPECT_TRUE(string_value.empty());

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
  EXPECT_TRUE(saved_value->GetAsString(&string_value));
  ASSERT_EQ("stable-channel", string_value);
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
  owner_key_util_->SetPrivateKey(device_policy_->GetSigningKey());
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
  owner_key_util_->SetPrivateKey(device_policy_->GetSigningKey());
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
  EXPECT_TRUE(value2.Equals(saved_value));

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

  closure = base::DoNothing::Once();
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
  base::ListValue expected_accounts;
  std::unique_ptr<base::DictionaryValue> entry_dict(
      new base::DictionaryValue());
  entry_dict->SetString(kAccountsPrefDeviceLocalAccountsKeyId,
                        policy::PolicyBuilder::kFakeUsername);
  entry_dict->SetInteger(kAccountsPrefDeviceLocalAccountsKeyType,
                         policy::DeviceLocalAccount::TYPE_PUBLIC_SESSION);
  expected_accounts.Append(std::move(entry_dict));
  const base::Value* actual_accounts =
      provider_->Get(kAccountsPrefDeviceLocalAccounts);
  EXPECT_EQ(expected_accounts, *actual_accounts);
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
  const base::Value expected_disabled_value(true);
  EXPECT_EQ(expected_disabled_value, *provider_->Get(kDeviceDisabled));
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
  const int status_frequency = 50000;
  SetReportingSettings(true, status_frequency);
  VerifyReportingSettings(true, status_frequency);

  // Turn off all reporting and verify that the settings are decoded
  // correctly.
  SetReportingSettings(false, status_frequency);
  VerifyReportingSettings(false, status_frequency);
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
  base::ListValue allowed_connections;
  allowed_connections.AppendInteger(0);
  VerifyPolicyValue(kAllowedConnectionTypesForUpdate, &allowed_connections);
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

TEST_F(DeviceSettingsProviderTest, SamlLoginAuthenticationType) {
  using PolicyProto = em::SamlLoginAuthenticationTypeProto;

  VerifyPolicyValue(kSamlLoginAuthenticationType, nullptr);

  {
    SetSamlLoginAuthenticationType(PolicyProto::TYPE_DEFAULT);
    base::Value expected_value(PolicyProto::TYPE_DEFAULT);
    VerifyPolicyValue(kSamlLoginAuthenticationType, &expected_value);
  }

  {
    SetSamlLoginAuthenticationType(PolicyProto::TYPE_CLIENT_CERTIFICATE);
    base::Value expected_value(PolicyProto::TYPE_CLIENT_CERTIFICATE);
    VerifyPolicyValue(kSamlLoginAuthenticationType, &expected_value);
  }
}

// Test invalid cases
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
  base::ListValue test_list;
  base::DictionaryValue interval;
  interval.SetPath({"start", "day_of_week"}, base::Value("Monday"));
  interval.SetPath({"start", "hours"}, base::Value(10));
  interval.SetPath({"start", "minutes"}, base::Value(50));
  interval.SetPath({"end", "day_of_week"}, base::Value("Wednesday"));
  interval.SetPath({"end", "hours"}, base::Value(1));
  interval.SetPath({"end", "minutes"}, base::Value(20));
  test_list.Append(std::move(interval));
  SetDeviceAutoUpdateTimeRestrictions(extra_field);
  VerifyPolicyValue(kDeviceAutoUpdateTimeRestrictions, &test_list);
}

// Check valid JSON for DeviceScheduledUpdateCheck.
TEST_F(DeviceSettingsProviderTest, DeviceScheduledUpdateCheckTests) {
  const std::string json_string =
      "{\"update_check_time\": {\"hour\": 23, \"minute\": 35}, "
      "\"frequency\": \"DAILY\", \"day_of_week\": \"MONDAY\",  "
      "\"day_of_month\": 15}";
  base::DictionaryValue expected_val;
  expected_val.SetPath({"update_check_time", "hour"}, base::Value(23));
  expected_val.SetPath({"update_check_time", "minute"}, base::Value(35));
  expected_val.Set("frequency", std::make_unique<base::Value>("DAILY"));
  expected_val.Set("day_of_week", std::make_unique<base::Value>("MONDAY"));
  expected_val.Set("day_of_month", std::make_unique<base::Value>(15));
  SetDeviceScheduledUpdateCheck(json_string);
  VerifyPolicyValue(kDeviceScheduledUpdateCheck, &expected_val);
}

TEST_F(DeviceSettingsProviderTest, DecodePluginVmAllowedSetting) {
  SetPluginVmAllowedSetting(true);
  EXPECT_EQ(base::Value(true), *provider_->Get(kPluginVmAllowed));

  SetPluginVmAllowedSetting(false);
  EXPECT_EQ(base::Value(false), *provider_->Get(kPluginVmAllowed));
}

TEST_F(DeviceSettingsProviderTest, DecodePluginVmLicenseKeySetting) {
  SetPluginVmLicenseKeySetting("LICENSE_KEY");
  EXPECT_EQ(base::Value("LICENSE_KEY"), *provider_->Get(kPluginVmLicenseKey));
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

TEST_F(DeviceSettingsProviderTest, DeviceWilcoDtcAllowedSetting) {
  // Policy should not be set by default
  VerifyPolicyValue(kDeviceWilcoDtcAllowed, nullptr);

  SetDeviceWilcoDtcAllowedSetting(true);
  EXPECT_EQ(base::Value(true), *provider_->Get(kDeviceWilcoDtcAllowed));

  SetDeviceWilcoDtcAllowedSetting(false);
  EXPECT_EQ(base::Value(false), *provider_->Get(kDeviceWilcoDtcAllowed));
}

TEST_F(DeviceSettingsProviderTest, DeviceDockMacAddressSourceSetting) {
  // Policy should not be set by default
  VerifyPolicyValue(kDeviceDockMacAddressSource, nullptr);

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
  // Policy should be set to true by default
  base::Value default_value(true);
  VerifyPolicyValue(kDevicePowerwashAllowed, &default_value);

  SetDevicePowerwashAllowed(true);
  EXPECT_EQ(base::Value(true), *provider_->Get(kDevicePowerwashAllowed));

  SetDevicePowerwashAllowed(false);
  EXPECT_EQ(base::Value(false), *provider_->Get(kDevicePowerwashAllowed));
}

TEST_F(DeviceSettingsProviderTest, DeviceLoginScreenSystemInfoEnforced) {
  // Policy should not be set by default
  VerifyPolicyValue(kDeviceLoginScreenSystemInfoEnforced, nullptr);

  SetSystemInfoEnforced(true);
  EXPECT_EQ(base::Value(true),
            *provider_->Get(kDeviceLoginScreenSystemInfoEnforced));

  SetSystemInfoEnforced(false);
  EXPECT_EQ(base::Value(false),
            *provider_->Get(kDeviceLoginScreenSystemInfoEnforced));
}

TEST_F(DeviceSettingsProviderTest, DeviceShowNumericKeyboardForPassword) {
  // Policy should not be set by default
  VerifyPolicyValue(kDeviceShowNumericKeyboardForPassword, nullptr);

  SetShowNumericKeyboardForPassword(true);
  EXPECT_EQ(base::Value(true),
            *provider_->Get(kDeviceShowNumericKeyboardForPassword));

  SetShowNumericKeyboardForPassword(false);
  EXPECT_EQ(base::Value(false),
            *provider_->Get(kDeviceShowNumericKeyboardForPassword));
}

TEST_F(DeviceSettingsProviderTest, DevicePrintersAccessMode_empty) {
  // Policy should be ACCESS_MODE_ALL by default
  base::Value default_value(em::DevicePrintersAccessModeProto::ACCESS_MODE_ALL);
  VerifyPolicyValue(kDevicePrintersAccessMode, &default_value);
}

TEST_F(DeviceSettingsProviderTest, DevicePrintersAccessMode_native) {
  // WHITELIST => ALLOWLIST
  SetNativeDevicePrinterAccessMode(
      em::DeviceNativePrintersAccessModeProto::ACCESS_MODE_WHITELIST);
  BuildAndInstallDevicePolicy();
  base::Value expected_value(
      em::DevicePrintersAccessModeProto::ACCESS_MODE_ALLOWLIST);
  VerifyPolicyValue(kDevicePrintersAccessMode, &expected_value);
}

TEST_F(DeviceSettingsProviderTest, DevicePrintersAccessMode_accessmode) {
  SetDevicePrinterAccessMode(
      em::DevicePrintersAccessModeProto::ACCESS_MODE_ALLOWLIST);
  BuildAndInstallDevicePolicy();
  base::Value expected_value(
      em::DevicePrintersAccessModeProto::ACCESS_MODE_ALLOWLIST);
  VerifyPolicyValue(kDevicePrintersAccessMode, &expected_value);
}

TEST_F(DeviceSettingsProviderTest, DevicePrintersAccessMode_both) {
  // If both are set use the DevicePrintersAccessMode
  SetNativeDevicePrinterAccessMode(
      em::DeviceNativePrintersAccessModeProto::ACCESS_MODE_BLACKLIST);
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

TEST_F(DeviceSettingsProviderTest, DevicePrintersBlocklist_blacklist) {
  std::vector<std::string> values = {"foo", "bar"};

  // If the blacklist only is set, use that.
  SetNativeDevicePrintersBlacklist(values);
  BuildAndInstallDevicePolicy();
  VerifyDevicePrinterList(kDevicePrintersBlocklist, values);
}

TEST_F(DeviceSettingsProviderTest, DevicePrintersBlocklist_blocklist) {
  std::vector<std::string> values = {"foo", "bar"};

  // If the blocklist only is set, use that.
  SetDevicePrintersBlocklist(values);
  BuildAndInstallDevicePolicy();
  VerifyDevicePrinterList(kDevicePrintersBlocklist, values);
}

TEST_F(DeviceSettingsProviderTest, DevicePrintersBlocklist_both) {
  std::vector<std::string> values = {"foo", "bar"};
  std::vector<std::string> other_values = {"baz"};

  // If both are set use the blocklist
  SetNativeDevicePrintersBlacklist(other_values);
  SetDevicePrintersBlocklist(values);
  BuildAndInstallDevicePolicy();
  VerifyDevicePrinterList(kDevicePrintersBlocklist, values);
}

TEST_F(DeviceSettingsProviderTest, DevicePrintersAllowlist_empty) {
  // Policy should not be set by default
  VerifyPolicyValue(kDevicePrintersAllowlist, nullptr);
}

TEST_F(DeviceSettingsProviderTest, DevicePrintersAllowlist_whitelist) {
  std::vector<std::string> values = {"foo", "bar"};

  // If the blacklist only is set, use that.
  SetNativeDevicePrintersWhitelist(values);
  BuildAndInstallDevicePolicy();
  VerifyDevicePrinterList(kDevicePrintersAllowlist, values);
}

TEST_F(DeviceSettingsProviderTest, DevicePrintersAllowlist_allowlist) {
  std::vector<std::string> values = {"foo", "bar"};

  // If the blocklist only is set, use that.
  SetDevicePrintersAllowlist(values);
  BuildAndInstallDevicePolicy();
  VerifyDevicePrinterList(kDevicePrintersAllowlist, values);
}

TEST_F(DeviceSettingsProviderTest, DevicePrintersAllowlist_both) {
  std::vector<std::string> values = {"foo", "bar"};
  std::vector<std::string> other_values = {"baz"};

  // If both are set use the blocklist
  SetNativeDevicePrintersWhitelist(other_values);
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

  base::ListValue expected_feature_flags;
  expected_feature_flags.Append(base::Value("foo"));
  EXPECT_EQ(expected_feature_flags, *provider_->Get(kFeatureFlags));
}

TEST_F(DeviceSettingsProviderTest, DecodeBorealisAllowed) {
  device_policy_->payload().mutable_device_borealis_allowed()->set_allowed(
      true);
  BuildAndInstallDevicePolicy();
  EXPECT_EQ(base::Value(true), *provider_->Get(kBorealisAllowedForDevice));
}

TEST_F(DeviceSettingsProviderTest, DecodeBorealisDisallowed) {
  device_policy_->payload().mutable_device_borealis_allowed()->set_allowed(
      false);
  BuildAndInstallDevicePolicy();
  EXPECT_EQ(base::Value(false), *provider_->Get(kBorealisAllowedForDevice));
}

TEST_F(DeviceSettingsProviderTest, DeviceAllowedBluetoothServices) {
  em::DeviceAllowedBluetoothServicesProto* proto =
      device_policy_->payload().mutable_device_allowed_bluetooth_services();
  proto->add_allowlist("0x1124");
  BuildAndInstallDevicePolicy();
  base::ListValue allowlist;
  allowlist.Append(base::Value("0x1124"));
  EXPECT_EQ(allowlist, *provider_->Get(kDeviceAllowedBluetoothServices));
}
}  // namespace ash
