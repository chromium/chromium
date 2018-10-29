// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/device_settings_provider.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/settings/device_settings_test_helper.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/user.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace chromeos {

using ::testing::AtLeast;
using ::testing::AnyNumber;
using ::testing::Mock;
using ::testing::_;

namespace {

const char kDisabledMessage[] = "This device has been disabled.";

constexpr em::AutoUpdateSettingsProto_ConnectionType kConnectionTypes[] = {
    em::AutoUpdateSettingsProto::CONNECTION_TYPE_ETHERNET,
    em::AutoUpdateSettingsProto::CONNECTION_TYPE_WIFI,
    em::AutoUpdateSettingsProto::CONNECTION_TYPE_WIMAX,
    em::AutoUpdateSettingsProto::CONNECTION_TYPE_BLUETOOTH,
    em::AutoUpdateSettingsProto::CONNECTION_TYPE_CELLULAR,
};

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
        base::Bind(&DeviceSettingsProviderTest::SettingChanged,
                   base::Unretained(this)),
        &device_settings_service_, local_state_.Get()));
    Mock::VerifyAndClearExpectations(this);
  }

  void TearDown() override { DeviceSettingsTestBase::TearDown(); }

  void BuildAndInstallDevicePolicy() {
    EXPECT_CALL(*this, SettingChanged(_)).Times(AtLeast(1));
    device_policy_.Build();
    session_manager_client_.set_device_policy(device_policy_.GetBlob());
    ReloadDeviceSettings();
    Mock::VerifyAndClearExpectations(this);
  }

  // Helper routine to enable/disable all reporting settings in policy.
  void SetReportingSettings(bool enable_reporting, int frequency) {
    em::DeviceReportingProto* proto =
        device_policy_.payload().mutable_device_reporting();
    proto->set_report_version_info(enable_reporting);
    proto->set_report_activity_times(enable_reporting);
    proto->set_report_boot_mode(enable_reporting);
    proto->set_report_location(enable_reporting);
    proto->set_report_network_interfaces(enable_reporting);
    proto->set_report_users(enable_reporting);
    proto->set_report_hardware_status(enable_reporting);
    proto->set_report_session_status(enable_reporting);
    proto->set_report_os_update_status(enable_reporting);
    proto->set_report_running_kiosk_app(enable_reporting);
    proto->set_device_status_frequency(frequency);
    BuildAndInstallDevicePolicy();
  }

  // Helper routine to enable/disable all reporting settings in policy.
  void SetHeartbeatSettings(bool enable_heartbeat, int frequency) {
    em::DeviceHeartbeatSettingsProto* proto =
        device_policy_.payload().mutable_device_heartbeat_settings();
    proto->set_heartbeat_enabled(enable_heartbeat);
    proto->set_heartbeat_frequency(frequency);
    BuildAndInstallDevicePolicy();
  }

  // Helper routine to enable/disable log upload settings in policy.
  void SetLogUploadSettings(bool enable_system_log_upload) {
    em::DeviceLogUploadSettingsProto* proto =
        device_policy_.payload().mutable_device_log_upload_settings();
    proto->set_system_log_upload_enabled(enable_system_log_upload);
    BuildAndInstallDevicePolicy();
  }

  // Helper routine to set device wallpaper setting in policy.
  void SetWallpaperSettings(const std::string& wallpaper_settings) {
    em::DeviceWallpaperImageProto* proto =
        device_policy_.payload().mutable_device_wallpaper_image();
    proto->set_device_wallpaper_image(wallpaper_settings);
    BuildAndInstallDevicePolicy();
  }

  enum MetricsOption { DISABLE_METRICS, ENABLE_METRICS, REMOVE_METRICS_POLICY };

  // Helper routine to enable/disable metrics report upload settings in policy.
  void SetMetricsReportingSettings(MetricsOption option) {
    if (option == REMOVE_METRICS_POLICY) {
      // Remove policy altogether
      device_policy_.payload().clear_metrics_enabled();
    } else {
      // Enable or disable policy
      em::MetricsEnabledProto* proto =
          device_policy_.payload().mutable_metrics_enabled();
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
      kReportDeviceBootMode,
      // Device location reporting is not currently supported.
      // kReportDeviceLocation,
      kReportDeviceNetworkInterfaces,
      kReportDeviceUsers,
      kReportDeviceHardwareStatus,
      kReportDeviceSessionStatus,
      kReportOsUpdateStatus,
      kReportRunningKioskApp
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
                         const base::Value* const ptr_to_expected_value) {
    // The pointer might be null, so check before dereferencing.
    if (ptr_to_expected_value)
      EXPECT_EQ(*ptr_to_expected_value, *provider_->Get(policy_key));
    else
      EXPECT_EQ(nullptr, provider_->Get(policy_key));
  }

  // Helper routine to set LoginScreenDomainAutoComplete policy.
  void SetDomainAutoComplete(const std::string& domain) {
    em::LoginScreenDomainAutoCompleteProto* proto =
        device_policy_.payload().mutable_login_screen_domain_auto_complete();
    proto->set_login_screen_domain_auto_complete(domain);
    BuildAndInstallDevicePolicy();
  }

  // Helper routine to check value of the LoginScreenDomainAutoComplete policy.
  void VerifyDomainAutoComplete(
      const base::Value* const ptr_to_expected_value) {
    VerifyPolicyValue(kAccountsPrefLoginScreenDomainAutoComplete,
                      ptr_to_expected_value);
  }

  // Helper routine to set AutoUpdates connection types policy.
  void SetAutoUpdateConnectionTypes(const std::vector<int>& values) {
    em::AutoUpdateSettingsProto* proto =
        device_policy_.payload().mutable_auto_update_settings();
    proto->set_update_disabled(false);
    for (auto const& value : values) {
      proto->add_allowed_connection_types(kConnectionTypes[value]);
    }
    BuildAndInstallDevicePolicy();
  }

  // Helper routine to set HostnameTemplate policy.
  void SetHostnameTemplate(const std::string& hostname_template) {
    em::NetworkHostnameProto* proto =
        device_policy_.payload().mutable_network_hostname();
    proto->set_device_hostname_template(hostname_template);
    BuildAndInstallDevicePolicy();
  }

  // Helper routine to set the DeviceSamlLoginAuthenticationType policy.
  void SetSamlLoginAuthenticationType(
      em::SamlLoginAuthenticationTypeProto::Type value) {
    em::SamlLoginAuthenticationTypeProto* proto =
        device_policy_.payload().mutable_saml_login_authentication_type();
    proto->set_saml_login_authentication_type(value);
    BuildAndInstallDevicePolicy();
  }

  // Helper routine that sets the device DeviceAutoUpdateTimeRestricitons policy
  void SetDeviceAutoUpdateTimeRestrictions(const std::string& json_string) {
    em::AutoUpdateSettingsProto* proto =
        device_policy_.payload().mutable_auto_update_settings();
    proto->set_disallowed_time_intervals(json_string);
    BuildAndInstallDevicePolicy();
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
  EXPECT_EQ(CrosSettingsProvider::TRUSTED,
            provider_->PrepareTrustedValues(base::Closure()));
  const base::Value* value = provider_->Get(kStatsReportingPref);
  ASSERT_TRUE(value);
  bool bool_value;
  EXPECT_TRUE(value->GetAsBoolean(&bool_value));
  EXPECT_FALSE(bool_value);
}

TEST_F(DeviceSettingsProviderTest, InitializationTestUnowned) {
  // Have the service check the key.
  owner_key_util_->Clear();
  ReloadDeviceSettings();

  // The trusted flag should be set before the call to PrepareTrustedValues.
  EXPECT_EQ(CrosSettingsProvider::TRUSTED,
            provider_->PrepareTrustedValues(base::Closure()));
  const base::Value* value = provider_->Get(kReleaseChannel);
  ASSERT_TRUE(value);
  std::string string_value;
  EXPECT_TRUE(value->GetAsString(&string_value));
  EXPECT_TRUE(string_value.empty());

  // Sets should succeed though and be readable from the cache.
  EXPECT_CALL(*this, SettingChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*this, SettingChanged(kReleaseChannel)).Times(1);
  base::Value new_value("stable-channel");
  provider_->Set(kReleaseChannel, new_value);
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
  bool bool_value;
  EXPECT_TRUE(saved_value->GetAsBoolean(&bool_value));
  EXPECT_TRUE(bool_value);
}

TEST_F(DeviceSettingsProviderTest, NoPolicyDefaultsOff) {
  // Missing policy should default to reporting enabled for non-enterprise-
  // enrolled devices, see crbug/456186.
  SetMetricsReportingSettings(REMOVE_METRICS_POLICY);
  const base::Value* saved_value = provider_->Get(kStatsReportingPref);
  ASSERT_TRUE(saved_value);
  bool bool_value;
  EXPECT_TRUE(saved_value->GetAsBoolean(&bool_value));
  EXPECT_FALSE(bool_value);
}

TEST_F(DeviceSettingsProviderTest, SetPrefFailed) {
  SetMetricsReportingSettings(DISABLE_METRICS);

  // If we are not the owner no sets should work.
  base::Value value(true);
  EXPECT_CALL(*this, SettingChanged(kStatsReportingPref)).Times(1);
  provider_->Set(kStatsReportingPref, value);
  Mock::VerifyAndClearExpectations(this);

  // This shouldn't trigger a write.
  session_manager_client_.set_device_policy(std::string());
  FlushDeviceSettings();
  EXPECT_EQ(std::string(), session_manager_client_.device_policy());

  // Verify the change has not been applied.
  const base::Value* saved_value = provider_->Get(kStatsReportingPref);
  ASSERT_TRUE(saved_value);
  bool bool_value;
  EXPECT_TRUE(saved_value->GetAsBoolean(&bool_value));
  EXPECT_FALSE(bool_value);
}

TEST_F(DeviceSettingsProviderTest, SetPrefSucceed) {
  owner_key_util_->SetPrivateKey(device_policy_.GetSigningKey());
  InitOwner(AccountId::FromUserEmail(device_policy_.policy_data().username()),
            true);
  FlushDeviceSettings();

  base::Value value(true);
  EXPECT_CALL(*this, SettingChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*this, SettingChanged(kStatsReportingPref)).Times(1);
  provider_->Set(kStatsReportingPref, value);
  Mock::VerifyAndClearExpectations(this);

  // Process the store.
  session_manager_client_.set_device_policy(std::string());
  FlushDeviceSettings();

  // Verify that the device policy has been adjusted.
  ASSERT_TRUE(device_settings_service_.device_settings());
  EXPECT_TRUE(device_settings_service_.device_settings()->
                  metrics_enabled().metrics_enabled());

  // Verify the change has been applied.
  const base::Value* saved_value = provider_->Get(kStatsReportingPref);
  ASSERT_TRUE(saved_value);
  bool bool_value;
  EXPECT_TRUE(saved_value->GetAsBoolean(&bool_value));
  EXPECT_TRUE(bool_value);
}

TEST_F(DeviceSettingsProviderTest, SetPrefTwice) {
  owner_key_util_->SetPrivateKey(device_policy_.GetSigningKey());
  InitOwner(AccountId::FromUserEmail(device_policy_.policy_data().username()),
            true);
  FlushDeviceSettings();

  EXPECT_CALL(*this, SettingChanged(_)).Times(AnyNumber());

  base::Value value1("beta");
  provider_->Set(kReleaseChannel, value1);
  base::Value value2("dev");
  provider_->Set(kReleaseChannel, value2);

  // Let the changes propagate through the system.
  session_manager_client_.set_device_policy(std::string());
  FlushDeviceSettings();

  // Verify the second change has been applied.
  const base::Value* saved_value = provider_->Get(kReleaseChannel);
  EXPECT_TRUE(value2.Equals(saved_value));

  Mock::VerifyAndClearExpectations(this);
}

TEST_F(DeviceSettingsProviderTest, PolicyRetrievalFailedBadSignature) {
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_.GetSigningKey());
  device_policy_.policy().set_policy_data_signature("bad signature");
  session_manager_client_.set_device_policy(device_policy_.GetBlob());
  ReloadDeviceSettings();

  // Verify that the cached settings blob is not "trusted".
  EXPECT_EQ(DeviceSettingsService::STORE_VALIDATION_ERROR,
            device_settings_service_.status());
  EXPECT_EQ(CrosSettingsProvider::PERMANENTLY_UNTRUSTED,
            provider_->PrepareTrustedValues(base::Closure()));
}

TEST_F(DeviceSettingsProviderTest, PolicyRetrievalNoPolicy) {
  owner_key_util_->SetPublicKeyFromPrivateKey(*device_policy_.GetSigningKey());
  session_manager_client_.set_device_policy(std::string());
  ReloadDeviceSettings();

  // Verify that the cached settings blob is not "trusted".
  EXPECT_EQ(DeviceSettingsService::STORE_NO_POLICY,
            device_settings_service_.status());
  EXPECT_EQ(CrosSettingsProvider::PERMANENTLY_UNTRUSTED,
            provider_->PrepareTrustedValues(base::Closure()));
}

TEST_F(DeviceSettingsProviderTest, PolicyFailedPermanentlyNotification) {
  session_manager_client_.set_device_policy(std::string());

  EXPECT_CALL(*this, GetTrustedCallback());
  EXPECT_EQ(CrosSettingsProvider::TEMPORARILY_UNTRUSTED,
            provider_->PrepareTrustedValues(
                base::Bind(&DeviceSettingsProviderTest::GetTrustedCallback,
                           base::Unretained(this))));

  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(this);

  EXPECT_EQ(CrosSettingsProvider::PERMANENTLY_UNTRUSTED,
            provider_->PrepareTrustedValues(base::Closure()));
}

TEST_F(DeviceSettingsProviderTest, PolicyLoadNotification) {
  EXPECT_CALL(*this, GetTrustedCallback());

  EXPECT_EQ(CrosSettingsProvider::TEMPORARILY_UNTRUSTED,
            provider_->PrepareTrustedValues(
                base::Bind(&DeviceSettingsProviderTest::GetTrustedCallback,
                           base::Unretained(this))));

  ReloadDeviceSettings();
  Mock::VerifyAndClearExpectations(this);
}

TEST_F(DeviceSettingsProviderTest, LegacyDeviceLocalAccounts) {
  em::DeviceLocalAccountInfoProto* account =
      device_policy_.payload().mutable_device_local_accounts()->add_account();
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
  device_policy_.policy_data().mutable_device_state()->set_device_mode(
      em::DeviceState::DEVICE_MODE_DISABLED);
  device_policy_.policy_data().mutable_device_state()->
      mutable_disabled_state()->set_message(kDisabledMessage);
  BuildAndInstallDevicePolicy();
  // Verify that the device state has been decoded correctly.
  const base::Value expected_disabled_value(true);
  EXPECT_EQ(expected_disabled_value, *provider_->Get(kDeviceDisabled));
  const base::Value expected_disabled_message_value(kDisabledMessage);
  EXPECT_EQ(expected_disabled_message_value,
            *provider_->Get(kDeviceDisabledMessage));

  // Verify that a change to the device state triggers a notification.
  device_policy_.policy_data().mutable_device_state()->clear_device_mode();
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
  const std::vector<int> no_values = {};
  SetAutoUpdateConnectionTypes(no_values);
  VerifyPolicyValue(kAllowedConnectionTypesForUpdate, nullptr);

  const std::vector<int> single_value = {0};
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

TEST_F(DeviceSettingsProviderTest, SetWallpaperSettings) {
  // Invalid format should be ignored.
  const std::string invalid_format = "\\\\invalid\\format";
  SetWallpaperSettings(invalid_format);
  EXPECT_EQ(nullptr, provider_->Get(kDeviceWallpaperImage));

  // Set with valid json format.
  const std::string valid_format(R"({"url":"foo", "hash": "bar"})");
  SetWallpaperSettings(valid_format);
  std::unique_ptr<base::DictionaryValue> expected_value =
      base::DictionaryValue::From(base::JSONReader::Read(valid_format));
  EXPECT_EQ(*expected_value, *provider_->Get(kDeviceWallpaperImage));
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
  test_list.GetList().push_back(std::move(interval));
  SetDeviceAutoUpdateTimeRestrictions(extra_field);
  VerifyPolicyValue(kDeviceAutoUpdateTimeRestrictions, &test_list);
}

}  // namespace chromeos
