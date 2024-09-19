// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_policy_decoder.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/strings/grit/components_strings.h"
#include "policy_common_definitions.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kInvalidJson[] = R"({"foo": "bar")";

// Prefix of the invalid-JSON error. The remainder of the error depends on which
// specific JSON parser is used.
constexpr char16_t kInvalidJsonParsingErrorPrefix[] =
    u"Policy parsing error: Invalid JSON string:";

constexpr char kInvalidPolicyName[] = "invalid-policy-name";

constexpr char kWallpaperJson[] = R"({
      "url": "https://example.com/device_wallpaper.jpg",
      "hash": "examplewallpaperhash"
    })";

constexpr char kWallpaperJsonInvalidValue[] = R"({
      "url": 123,
      "hash": "examplewallpaperhash"
    })";

constexpr char kWallpaperJsonUnknownProperty[] = R"({
    "url": "https://example.com/device_wallpaper.jpg",
    "hash": "examplewallpaperhash",
    "unknown-field": "random-value"
  })";

constexpr char kWallpaperUrlPropertyName[] = "url";
constexpr char kWallpaperUrlPropertyValue[] =
    "https://example.com/device_wallpaper.jpg";
constexpr char kWallpaperHashPropertyName[] = "hash";
constexpr char kWallpaperHashPropertyValue[] = "examplewallpaperhash";
constexpr char kValidBluetoothServiceUUID4[] = "0x1124";
constexpr char kValidBluetoothServiceUUID8[] = "0000180F";
constexpr char kValidBluetoothServiceUUID32[] =
    "00002A00-0000-1000-8000-00805F9B34FB";
constexpr char kValidBluetoothServiceUUIDList[] =
    "[\"0x1124\", \"0000180F\", \"00002A00-0000-1000-8000-00805F9B34FB\"]";
constexpr char kInvalidBluetoothServiceUUIDList[] = "[\"wrong-uuid\"]";

constexpr char kDeviceLocalAccountKioskAccountId[] = "kiosk_account_id";

constexpr char kValidDeviceWeeklyScheduledSuspendList[] = R"([
    {
      "start": {
        "day_of_week": "MONDAY",
        "time": 64800000
      },
      "end": {
        "day_of_week": "TUESDAY",
        "time": 28800000
      }
    },
    {
      "start": {
        "day_of_week": "FRIDAY",
        "time": 75600000
      },
      "end": {
        "day_of_week": "MONDAY",
        "time": 25200000
      }
    }
])";

constexpr char kValidDeviceRestrictionScheduleJson[] = R"([
  {
    "start": {
        "day_of_week": "WEDNESDAY",
        "milliseconds_since_midnight": 43200000
    },
    "end": {
        "day_of_week": "WEDNESDAY",
        "milliseconds_since_midnight": 75600000
    }
  },
  {
    "start": {
        "day_of_week": "FRIDAY",
        "milliseconds_since_midnight": 64800000
    },
    "end": {
        "day_of_week": "MONDAY",
        "milliseconds_since_midnight": 21600000
    }
  }
])";

}  // namespace

class DevicePolicyDecoderTest : public testing::Test {
 public:
  DevicePolicyDecoderTest() = default;

  DevicePolicyDecoderTest(const DevicePolicyDecoderTest&) = delete;
  DevicePolicyDecoderTest& operator=(const DevicePolicyDecoderTest&) = delete;

  ~DevicePolicyDecoderTest() override = default;

 protected:
  base::Value GetWallpaperDict() const;
  base::Value GetBluetoothServiceAllowedList() const;
  std::vector<WeeklyTimeInterval> GetDeviceWeeklyScheduledSuspendList() const;
  void DecodeDevicePolicyTestHelper(
      const em::ChromeDeviceSettingsProto& device_policy,
      const std::string& policy_path,
      base::Value expected_value) const;
  void DecodeUnsetDevicePolicyTestHelper(
      const em::ChromeDeviceSettingsProto& device_policy,
      const std::string& policy_path) const;
};

base::Value DevicePolicyDecoderTest::GetWallpaperDict() const {
  return base::Value(
      base::Value::Dict()
          .Set(kWallpaperUrlPropertyName, kWallpaperUrlPropertyValue)
          .Set(kWallpaperHashPropertyName, kWallpaperHashPropertyValue));
}

base::Value DevicePolicyDecoderTest::GetBluetoothServiceAllowedList() const {
  return base::Value(base::Value::List()
                         .Append(kValidBluetoothServiceUUID4)
                         .Append(kValidBluetoothServiceUUID8)
                         .Append(kValidBluetoothServiceUUID32));
}

std::vector<WeeklyTimeInterval>
DevicePolicyDecoderTest::GetDeviceWeeklyScheduledSuspendList() const {
  using time_proto = em::WeeklyTimeProto;
  std::vector<WeeklyTimeInterval> ret;
  ret.emplace_back(
      WeeklyTime(time_proto::MONDAY, base::Hours(18).InMilliseconds(),
                 /*timezone_offset=*/std::nullopt),
      WeeklyTime(time_proto::TUESDAY, base::Hours(8).InMilliseconds(),
                 /*timezone_offset=*/std::nullopt));
  ret.emplace_back(
      WeeklyTime(time_proto::FRIDAY, base::Hours(21).InMilliseconds(),
                 /*timezone_offset=*/std::nullopt),
      WeeklyTime(time_proto::MONDAY, base::Hours(7).InMilliseconds(),
                 /*timezone_offset=*/std::nullopt));
  return ret;
}

void DevicePolicyDecoderTest::DecodeDevicePolicyTestHelper(
    const em::ChromeDeviceSettingsProto& device_policy,
    const std::string& policy_path,
    base::Value expected_value) const {
  PolicyBundle bundle;
  PolicyMap& policies = bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, ""));

  base::WeakPtr<ExternalDataManager> external_data_manager;

  DecodeDevicePolicy(device_policy, external_data_manager, &policies);

  // It is safe to use `GetValueUnsafe()` as multiple policy types are handled.
  const base::Value* actual_value = policies.GetValueUnsafe(policy_path);
  ASSERT_NE(actual_value, nullptr);
  EXPECT_EQ(*actual_value, expected_value);
}

void DevicePolicyDecoderTest::DecodeUnsetDevicePolicyTestHelper(
    const em::ChromeDeviceSettingsProto& device_policy,
    const std::string& policy_path) const {
  PolicyBundle bundle;
  PolicyMap& policies = bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, ""));

  base::WeakPtr<ExternalDataManager> external_data_manager;

  DecodeDevicePolicy(device_policy, external_data_manager, &policies);

  // It is safe to use `GetValueUnsafe()` as multiple policy types are handled.
  const base::Value* actual_value = policies.GetValueUnsafe(policy_path);
  EXPECT_EQ(actual_value, nullptr);
}

TEST_F(DevicePolicyDecoderTest, DecodeJsonStringAndNormalizeJSONParseError) {
  auto decoding_result =
      DecodeJsonStringAndNormalize(kInvalidJson, key::kDeviceWallpaperImage);
  ASSERT_FALSE(decoding_result.has_value());
  std::string localized_error =
      l10n_util::GetStringFUTF8(IDS_POLICY_PROTO_PARSING_ERROR,
                                base::UTF8ToUTF16(decoding_result.error()));
  EXPECT_NE(std::string::npos,
            localized_error.find("Policy parsing error: Invalid JSON string"));
}

#if GTEST_HAS_DEATH_TEST
TEST_F(DevicePolicyDecoderTest, DecodeJsonStringAndNormalizeInvalidSchema) {
  std::string error;
  EXPECT_DEATH(std::ignore = DecodeJsonStringAndNormalize(kWallpaperJson,
                                                          kInvalidPolicyName),
               "");
}
#endif

TEST_F(DevicePolicyDecoderTest, DecodeJsonStringAndNormalizeInvalidValue) {
  auto decoding_result = DecodeJsonStringAndNormalize(
      kWallpaperJsonInvalidValue, key::kDeviceWallpaperImage);
  ASSERT_FALSE(decoding_result.has_value());
  std::string localized_error =
      l10n_util::GetStringFUTF8(IDS_POLICY_PROTO_PARSING_ERROR,
                                base::UTF8ToUTF16(decoding_result.error()));
  EXPECT_EQ(
      "Policy parsing error: Invalid policy value: Policy type mismatch: "
      "expected: \"string\", actual: \"integer\". (at "
      "DeviceWallpaperImage.url)",
      localized_error);
}

TEST_F(DevicePolicyDecoderTest, DecodeJsonStringAndNormalizeUnknownProperty) {
  auto decoding_result = DecodeJsonStringAndNormalize(
      kWallpaperJsonUnknownProperty, key::kDeviceWallpaperImage);
  ASSERT_TRUE(decoding_result.has_value());
  ASSERT_TRUE(decoding_result->non_fatal_errors.has_value());

  std::string localized_error = l10n_util::GetStringFUTF8(
      IDS_POLICY_PROTO_PARSING_ERROR,
      base::UTF8ToUTF16(decoding_result->non_fatal_errors.value()));
  EXPECT_EQ(GetWallpaperDict(), decoding_result->decoded_json);
  EXPECT_EQ(
      "Policy parsing error: Dropped unknown properties: Unknown property: "
      "unknown-field (at DeviceWallpaperImage)",
      localized_error);
}

TEST_F(DevicePolicyDecoderTest, DecodeJsonStringAndNormalizeSuccess) {
  auto decoding_result =
      DecodeJsonStringAndNormalize(kWallpaperJson, key::kDeviceWallpaperImage);
  ASSERT_TRUE(decoding_result.has_value());
  EXPECT_EQ(GetWallpaperDict(), decoding_result->decoded_json);
  EXPECT_FALSE(decoding_result->non_fatal_errors.has_value());
}

TEST_F(DevicePolicyDecoderTest, DeviceActivityHeartbeatEnabled) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kDeviceActivityHeartbeatEnabled);

  base::Value device_activity_heartbeat_enabled_value(true);
  device_policy.mutable_device_reporting()
      ->set_device_activity_heartbeat_enabled(
          device_activity_heartbeat_enabled_value.GetBool());

  DecodeDevicePolicyTestHelper(
      device_policy, key::kDeviceActivityHeartbeatEnabled,
      std::move(device_activity_heartbeat_enabled_value));
}

TEST_F(DevicePolicyDecoderTest, DeviceActivityHeartbeatCollectionRateMs) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(
      device_policy, key::kDeviceActivityHeartbeatCollectionRateMs);

  base::Value device_activity_heartbeat_collection_rate_ms_value(120000);
  device_policy.mutable_device_reporting()
      ->set_device_activity_heartbeat_collection_rate_ms(
          device_activity_heartbeat_collection_rate_ms_value.GetInt());

  DecodeDevicePolicyTestHelper(
      device_policy, key::kDeviceActivityHeartbeatCollectionRateMs,
      std::move(device_activity_heartbeat_collection_rate_ms_value));
}

TEST_F(DevicePolicyDecoderTest, ReportDeviceLoginLogout) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kReportDeviceLoginLogout);

  base::Value report_login_logout_value(true);
  device_policy.mutable_device_reporting()->set_report_login_logout(
      report_login_logout_value.GetBool());

  DecodeDevicePolicyTestHelper(device_policy, key::kReportDeviceLoginLogout,
                               std::move(report_login_logout_value));
}

TEST_F(DevicePolicyDecoderTest, ReportDeviceCRDSessions) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy, key::kReportCRDSessions);

  base::Value report_crd_sessions_value(true);
  device_policy.mutable_device_reporting()->set_report_crd_sessions(
      report_crd_sessions_value.GetBool());

  DecodeDevicePolicyTestHelper(device_policy, key::kReportCRDSessions,
                               std::move(report_crd_sessions_value));
}

TEST_F(DevicePolicyDecoderTest, ReportDeviceNetworkTelemetryCollectionRateMs) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(
      device_policy, key::kReportDeviceNetworkTelemetryCollectionRateMs);

  base::Value collection_rate_ms_value(120000);
  device_policy.mutable_device_reporting()
      ->set_report_network_telemetry_collection_rate_ms(
          collection_rate_ms_value.GetInt());

  DecodeDevicePolicyTestHelper(
      device_policy, key::kReportDeviceNetworkTelemetryCollectionRateMs,
      std::move(collection_rate_ms_value));
}

TEST_F(DevicePolicyDecoderTest,
       ReportDeviceNetworkTelemetryEventCheckingRateMs) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(
      device_policy, key::kReportDeviceNetworkTelemetryEventCheckingRateMs);

  base::Value event_checking_rate_ms_value(80000);
  device_policy.mutable_device_reporting()
      ->set_report_network_telemetry_event_checking_rate_ms(
          event_checking_rate_ms_value.GetInt());

  DecodeDevicePolicyTestHelper(
      device_policy, key::kReportDeviceNetworkTelemetryEventCheckingRateMs,
      std::move(event_checking_rate_ms_value));
}

TEST_F(DevicePolicyDecoderTest, ReportDeviceAudioStatusCheckingRateMs) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(
      device_policy, key::kReportDeviceAudioStatusCheckingRateMs);

  base::Value event_checking_rate_ms_value(80000);
  device_policy.mutable_device_reporting()
      ->set_report_device_audio_status_checking_rate_ms(
          event_checking_rate_ms_value.GetInt());

  DecodeDevicePolicyTestHelper(device_policy,
                               key::kReportDeviceAudioStatusCheckingRateMs,
                               std::move(event_checking_rate_ms_value));
}

TEST_F(DevicePolicyDecoderTest, DeviceReportRuntimeCountersCheckingRateMs) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(
      device_policy, key::kDeviceReportRuntimeCountersCheckingRateMs);

  base::Value event_checking_rate_ms_value(90000000);
  device_policy.mutable_device_reporting()
      ->set_device_report_runtime_counters_checking_rate_ms(
          event_checking_rate_ms_value.GetInt());

  DecodeDevicePolicyTestHelper(device_policy,
                               key::kDeviceReportRuntimeCountersCheckingRateMs,
                               std::move(event_checking_rate_ms_value));
}

TEST_F(DevicePolicyDecoderTest, ReportDevicePeripherals) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kReportDevicePeripherals);

  base::Value report_peripherals_value(true);
  device_policy.mutable_device_reporting()->set_report_peripherals(
      report_peripherals_value.GetBool());

  DecodeDevicePolicyTestHelper(device_policy, key::kReportDevicePeripherals,
                               std::move(report_peripherals_value));
}

TEST_F(DevicePolicyDecoderTest, ReportDeviceAudioStatus) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kReportDeviceAudioStatus);

  base::Value report_audio_status_value(true);
  device_policy.mutable_device_reporting()->set_report_audio_status(
      report_audio_status_value.GetBool());

  DecodeDevicePolicyTestHelper(device_policy, key::kReportDeviceAudioStatus,
                               std::move(report_audio_status_value));
}

TEST_F(DevicePolicyDecoderTest, DeviceReportRuntimeCounters) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kDeviceReportRuntimeCounters);

  base::Value report_runtime_counters_value(true);
  device_policy.mutable_device_reporting()->set_report_runtime_counters(
      report_runtime_counters_value.GetBool());

  DecodeDevicePolicyTestHelper(device_policy, key::kDeviceReportRuntimeCounters,
                               std::move(report_runtime_counters_value));
}

TEST_F(DevicePolicyDecoderTest, ReportDeviceSecurityStatus) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kReportDeviceSecurityStatus);

  base::Value report_security_status_value(true);
  device_policy.mutable_device_reporting()->set_report_security_status(
      report_security_status_value.GetBool());

  DecodeDevicePolicyTestHelper(device_policy, key::kReportDeviceSecurityStatus,
                               std::move(report_security_status_value));
}

TEST_F(DevicePolicyDecoderTest, ReportDeviceNetworkConfiguration) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kReportDeviceNetworkConfiguration);

  base::Value report_network_configuration_value(true);
  device_policy.mutable_device_reporting()->set_report_network_configuration(
      report_network_configuration_value.GetBool());

  DecodeDevicePolicyTestHelper(device_policy,
                               key::kReportDeviceNetworkConfiguration,
                               std::move(report_network_configuration_value));
}

TEST_F(DevicePolicyDecoderTest, ReportDeviceNetworkStatus) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kReportDeviceNetworkStatus);

  base::Value report_network_status_value(true);
  device_policy.mutable_device_reporting()->set_report_network_status(
      report_network_status_value.GetBool());

  DecodeDevicePolicyTestHelper(device_policy, key::kReportDeviceNetworkStatus,
                               std::move(report_network_status_value));
}

TEST_F(DevicePolicyDecoderTest, kReportDeviceOsUpdateStatus) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kReportDeviceOsUpdateStatus);

  base::Value report_os_update_status_value(true);
  device_policy.mutable_device_reporting()->set_report_os_update_status(
      report_os_update_status_value.GetBool());

  DecodeDevicePolicyTestHelper(device_policy, key::kReportDeviceOsUpdateStatus,
                               std::move(report_os_update_status_value));
}

TEST_F(DevicePolicyDecoderTest,
       ReportDeviceSignalStrengthEventDrivenTelemetry) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(
      device_policy, key::kReportDeviceSignalStrengthEventDrivenTelemetry);

  auto signal_strength_telemetry_list =
      base::Value::List().Append("network_telemetry").Append("https_latency");
  device_policy.mutable_device_reporting()
      ->mutable_report_signal_strength_event_driven_telemetry()
      ->add_entries("network_telemetry");
  device_policy.mutable_device_reporting()
      ->mutable_report_signal_strength_event_driven_telemetry()
      ->add_entries("https_latency");

  DecodeDevicePolicyTestHelper(
      device_policy, key::kReportDeviceSignalStrengthEventDrivenTelemetry,
      base::Value(std::move(signal_strength_telemetry_list)));
}

TEST_F(DevicePolicyDecoderTest, DeviceReportNetworkEvents) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kDeviceReportNetworkEvents);

  base::Value report_network_events_value(true);
  device_policy.mutable_device_reporting()->set_report_network_events(
      report_network_events_value.GetBool());

  DecodeDevicePolicyTestHelper(device_policy, key::kDeviceReportNetworkEvents,
                               std::move(report_network_events_value));
}

TEST_F(DevicePolicyDecoderTest, DecodeServiceUUIDListSuccess) {
  auto decoding_result = DecodeJsonStringAndNormalize(
      kValidBluetoothServiceUUIDList, key::kDeviceAllowedBluetoothServices);
  ASSERT_TRUE(decoding_result.has_value());
  EXPECT_EQ(GetBluetoothServiceAllowedList(), decoding_result->decoded_json);
  EXPECT_FALSE(decoding_result->non_fatal_errors.has_value());
}

TEST_F(DevicePolicyDecoderTest, DecodeServiceUUIDListError) {
  auto decoding_result = DecodeJsonStringAndNormalize(
      kInvalidBluetoothServiceUUIDList, key::kDeviceAllowedBluetoothServices);
  ASSERT_FALSE(decoding_result.has_value());
  EXPECT_EQ(
      "Invalid policy value: Invalid value for string (at "
      "DeviceAllowedBluetoothServices[0])",
      decoding_result.error());
}

TEST_F(DevicePolicyDecoderTest,
       DecodeLoginScreenPromptOnMultipleMatchingCertificates) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(
      device_policy,
      key::kDeviceLoginScreenPromptOnMultipleMatchingCertificates);

  base::Value login_screen_prompt_value(true);
  device_policy.mutable_login_screen_prompt_on_multiple_matching_certificates()
      ->set_value(login_screen_prompt_value.GetBool());

  DecodeDevicePolicyTestHelper(
      device_policy,
      key::kDeviceLoginScreenPromptOnMultipleMatchingCertificates,
      std::move(login_screen_prompt_value));
}

TEST_F(DevicePolicyDecoderTest, DecodeDeviceEncryptedReportingPipelineEnabled) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(
      device_policy, key::kDeviceEncryptedReportingPipelineEnabled);

  base::Value prompt_value(true);
  device_policy.mutable_device_encrypted_reporting_pipeline_enabled()
      ->set_enabled(prompt_value.GetBool());

  DecodeDevicePolicyTestHelper(device_policy,
                               key::kDeviceEncryptedReportingPipelineEnabled,
                               std::move(prompt_value));
}

TEST_F(DevicePolicyDecoderTest, DecodeDeviceAutofillSAMLUsername) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kDeviceAutofillSAMLUsername);

  base::Value autofill_saml_username_value("login_hint");
  device_policy.mutable_saml_username()
      ->set_url_parameter_to_autofill_saml_username(
          autofill_saml_username_value.GetString());

  DecodeDevicePolicyTestHelper(device_policy, key::kDeviceAutofillSAMLUsername,
                               std::move(autofill_saml_username_value));
}

TEST_F(DevicePolicyDecoderTest, DeviceReportXDREvents) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy, key::kDeviceReportXDREvents);

  base::Value device_report_xdr_events_value(true);
  device_policy.mutable_device_report_xdr_events()->set_enabled(
      device_report_xdr_events_value.GetBool());

  DecodeDevicePolicyTestHelper(device_policy, key::kDeviceReportXDREvents,
                               std::move(device_report_xdr_events_value));
}

TEST_F(DevicePolicyDecoderTest, DeviceHindiInscriptLayoutEnabled) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kDeviceHindiInscriptLayoutEnabled);

  base::Value device_hindi_inscript_layout_enabled_value(true);
  device_policy.mutable_device_hindi_inscript_layout_enabled()->set_enabled(
      device_hindi_inscript_layout_enabled_value.GetBool());

  DecodeDevicePolicyTestHelper(
      device_policy, key::kDeviceHindiInscriptLayoutEnabled,
      std::move(device_hindi_inscript_layout_enabled_value));
}

TEST_F(DevicePolicyDecoderTest, DeviceSystemAecEnabled) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kDeviceSystemAecEnabled);

  base::Value device_system_aec_enabled_value(true);
  device_policy.mutable_device_system_aec_enabled()
      ->set_device_system_aec_enabled(
          device_system_aec_enabled_value.GetBool());

  DecodeDevicePolicyTestHelper(device_policy, key::kDeviceSystemAecEnabled,
                               std::move(device_system_aec_enabled_value));
}

TEST_F(DevicePolicyDecoderTest,
       DecodeDeviceLocalAccountsWithoutEphemeralModeField) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy, key::kDeviceLocalAccounts);

  em::DeviceLocalAccountInfoProto* account =
      device_policy.mutable_device_local_accounts()->add_account();
  account->set_account_id(kDeviceLocalAccountKioskAccountId);
  account->set_type(
      em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_WEB_KIOSK_APP);

  DecodeDevicePolicyTestHelper(
      device_policy, key::kDeviceLocalAccounts,
      base::Value(base::Value::List().Append(
          base::Value::Dict()
              .Set(ash::kAccountsPrefDeviceLocalAccountsKeyId,
                   kDeviceLocalAccountKioskAccountId)
              .Set(ash::kAccountsPrefDeviceLocalAccountsKeyType,
                   static_cast<int>(DeviceLocalAccountType::kWebKioskApp))
              .Set(ash::kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
                   static_cast<int>(
                       DeviceLocalAccount::EphemeralMode::kUnset)))));
}

TEST_F(DevicePolicyDecoderTest,
       DecodeDeviceLocalAccountsWithEphemeralModeField) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy, key::kDeviceLocalAccounts);

  em::DeviceLocalAccountInfoProto* account =
      device_policy.mutable_device_local_accounts()->add_account();
  account->set_account_id(kDeviceLocalAccountKioskAccountId);
  account->set_type(em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_KIOSK_APP);
  account->set_ephemeral_mode(
      em::DeviceLocalAccountInfoProto::EPHEMERAL_MODE_DISABLE);

  DecodeDevicePolicyTestHelper(
      device_policy, key::kDeviceLocalAccounts,
      base::Value(base::Value::List().Append(
          base::Value::Dict()
              .Set(ash::kAccountsPrefDeviceLocalAccountsKeyId,
                   kDeviceLocalAccountKioskAccountId)
              .Set(ash::kAccountsPrefDeviceLocalAccountsKeyType,
                   static_cast<int>(DeviceLocalAccountType::kKioskApp))
              .Set(ash::kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
                   static_cast<int>(
                       DeviceLocalAccount::EphemeralMode::kDisable)))));
}

TEST_F(DevicePolicyDecoderTest, DeviceLowBatterySoundEnabled) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kDeviceLowBatterySoundEnabled);

  base::Value device_low_battery_sound_enabled_value(true);
  device_policy.mutable_device_low_battery_sound()->set_enabled(
      device_low_battery_sound_enabled_value.GetBool());

  DecodeDevicePolicyTestHelper(
      device_policy, key::kDeviceLowBatterySoundEnabled,
      std::move(device_low_battery_sound_enabled_value));
}

TEST_F(DevicePolicyDecoderTest, DeviceChargingSoundsEnabled) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kDeviceChargingSoundsEnabled);

  base::Value device_charging_sounds_enabled_value(true);
  device_policy.mutable_device_charging_sounds()->set_enabled(
      device_charging_sounds_enabled_value.GetBool());

  DecodeDevicePolicyTestHelper(device_policy, key::kDeviceChargingSoundsEnabled,
                               std::move(device_charging_sounds_enabled_value));
}

TEST_F(DevicePolicyDecoderTest, DecodeDeviceAuthenticationURLBlocklist) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kDeviceAuthenticationURLBlocklist);

  em::StringList* blocklist =
      device_policy.mutable_device_authentication_url_blocklist()
          ->mutable_value();

  auto blocklist_items =
      base::Value::List().Append("example.com").Append("*.example.com");

  for (auto& item : blocklist_items) {
    blocklist->add_entries(item.GetString());
  }

  DecodeDevicePolicyTestHelper(device_policy,
                               key::kDeviceAuthenticationURLBlocklist,
                               base::Value(std::move(blocklist_items)));
}

TEST_F(DevicePolicyDecoderTest, DecodeDeviceAuthenticationURLAllowlist) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kDeviceAuthenticationURLAllowlist);

  em::StringList* allowlist =
      device_policy.mutable_device_authentication_url_allowlist()
          ->mutable_value();

  auto allowlist_items = base::Value::List()
                             .Append("allow.example.com")
                             .Append("*.allow.example.com");

  for (auto& item : allowlist_items) {
    allowlist->add_entries(item.GetString());
  }

  DecodeDevicePolicyTestHelper(device_policy,
                               key::kDeviceAuthenticationURLAllowlist,
                               base::Value(std::move(allowlist_items)));
}

TEST_F(DevicePolicyDecoderTest, DeviceSwitchFunctionKeysBehaviorEnabled) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(
      device_policy, key::kDeviceSwitchFunctionKeysBehaviorEnabled);

  base::Value device_switch_function_keys_behavior_enabled(true);
  device_policy.mutable_device_switch_function_keys_behavior_enabled()
      ->set_enabled(device_switch_function_keys_behavior_enabled.GetBool());

  DecodeDevicePolicyTestHelper(
      device_policy, key::kDeviceSwitchFunctionKeysBehaviorEnabled,
      std::move(device_switch_function_keys_behavior_enabled));
}

TEST_F(DevicePolicyDecoderTest, DeviceEphemeralNetworkPoliciesEnabled) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(
      device_policy, key::kDeviceEphemeralNetworkPoliciesEnabled);

  device_policy.mutable_device_ephemeral_network_policies_enabled()->set_value(
      true);

  DecodeDevicePolicyTestHelper(device_policy,
                               key::kDeviceEphemeralNetworkPoliciesEnabled,
                               /*expected_value=*/base::Value(true));
}

TEST_F(DevicePolicyDecoderTest, DeviceLoginScreenTouchVirtualKeyboardPolicy) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kTouchVirtualKeyboardEnabled);

  device_policy.mutable_deviceloginscreentouchvirtualkeyboardenabled()
      ->set_value(true);

  DecodeDevicePolicyTestHelper(device_policy, key::kTouchVirtualKeyboardEnabled,
                               base::Value(true));
}

TEST_F(DevicePolicyDecoderTest, DeviceExtendedAutoUpdateEnabled) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kDeviceExtendedAutoUpdateEnabled);

  base::Value deviceextendedautoupdateenabled(true);
  device_policy.mutable_deviceextendedautoupdateenabled()->set_value(
      deviceextendedautoupdateenabled.GetBool());

  DecodeDevicePolicyTestHelper(device_policy,
                               key::kDeviceExtendedAutoUpdateEnabled,
                               std::move(deviceextendedautoupdateenabled));
}

TEST_F(DevicePolicyDecoderTest, DecodeDeviceWeeklyScheduledSuspendSuccess) {
  auto decoding_result =
      DecodeJsonStringAndNormalize(kValidDeviceWeeklyScheduledSuspendList,
                                   key::kDeviceWeeklyScheduledSuspend);
  ASSERT_TRUE(decoding_result.has_value());
  ASSERT_TRUE(decoding_result->decoded_json.is_list());

  std::vector<WeeklyTimeInterval> actual_list;
  for (const auto& item : decoding_result->decoded_json.GetList()) {
    ASSERT_TRUE(item.is_dict());
    std::unique_ptr<WeeklyTimeInterval> interval =
        WeeklyTimeInterval::ExtractFromDict(item.GetDict(),
                                            /*timezone_offset=*/std::nullopt);
    ASSERT_TRUE(interval);
    actual_list.emplace_back(std::move(*interval));
  }

  EXPECT_EQ(GetDeviceWeeklyScheduledSuspendList(), actual_list);
}

TEST_F(DevicePolicyDecoderTest,
       DecodeDeviceWeeklyScheduledSuspendInvalidJsonError) {
  auto decoding_result = DecodeJsonStringAndNormalize(
      kInvalidJson, key::kDeviceWeeklyScheduledSuspend);
  EXPECT_FALSE(decoding_result.has_value());
  EXPECT_THAT(
      l10n_util::GetStringFUTF8(IDS_POLICY_PROTO_PARSING_ERROR,
                                base::UTF8ToUTF16(decoding_result.error())),
      ::testing::HasSubstr("Policy parsing error: Invalid JSON string"));
}

TEST_F(DevicePolicyDecoderTest,
       DecodeDeviceAuthenticationFlowAutoReloadInterval) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(
      device_policy, key::kDeviceAuthenticationFlowAutoReloadInterval);

  base::Value auth_flow_reload_interval(15);
  device_policy.mutable_deviceauthenticationflowautoreloadinterval()->set_value(
      auth_flow_reload_interval.GetInt());

  DecodeDevicePolicyTestHelper(device_policy,
                               key::kDeviceAuthenticationFlowAutoReloadInterval,
                               std::move(auth_flow_reload_interval));
}

TEST_F(DevicePolicyDecoderTest, DeviceExtensionsSystemLogEnabled) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kDeviceExtensionsSystemLogEnabled);

  base::Value deviceextensionssystemlogenabled(true);
  device_policy.mutable_deviceextensionssystemlogenabled()->set_value(
      deviceextensionssystemlogenabled.GetBool());

  DecodeDevicePolicyTestHelper(device_policy,
                               key::kDeviceExtensionsSystemLogEnabled,
                               std::move(deviceextensionssystemlogenabled));
}

TEST_F(DevicePolicyDecoderTest, DeviceAllowEnterpriseRemoteAccessConnections) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(
      device_policy, key::kDeviceAllowEnterpriseRemoteAccessConnections);

  base::Value value(true);
  device_policy.mutable_deviceallowenterpriseremoteaccessconnections()
      ->set_value(value.GetBool());

  DecodeDevicePolicyTestHelper(
      device_policy, key::kDeviceAllowEnterpriseRemoteAccessConnections,
      std::move(value));
}

TEST_F(DevicePolicyDecoderTest, DevicePostQuantumKeyAgreementEnabled) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kDevicePostQuantumKeyAgreementEnabled);

  base::Value devicepostquantumkeyagreementenabled(true);
  device_policy.mutable_devicepostquantumkeyagreementenabled()->set_value(
      devicepostquantumkeyagreementenabled.GetBool());

  DecodeDevicePolicyTestHelper(device_policy,
                               key::kDevicePostQuantumKeyAgreementEnabled,
                               std::move(devicepostquantumkeyagreementenabled));
}

TEST_F(DevicePolicyDecoderTest, DecodeDeviceRestrictionSchedule) {
  em::ChromeDeviceSettingsProto device_policy;

  DecodeUnsetDevicePolicyTestHelper(device_policy,
                                    key::kDeviceRestrictionSchedule);

  auto decoding_result = DecodeJsonStringAndNormalize(
      kValidDeviceRestrictionScheduleJson, key::kDeviceRestrictionSchedule);
  ASSERT_TRUE(decoding_result.has_value());
  base::Value device_restriction_schedule(
      decoding_result->decoded_json.Clone());

  device_policy.mutable_devicerestrictionschedule()->set_value(
      kValidDeviceRestrictionScheduleJson);

  DecodeDevicePolicyTestHelper(device_policy, key::kDeviceRestrictionSchedule,
                               std::move(device_restriction_schedule));
}

TEST_F(DevicePolicyDecoderTest, DecodeDeviceRestrictionScheduleError) {
  em::ChromeDeviceSettingsProto device_policy;
  device_policy.mutable_devicerestrictionschedule()->set_value(kInvalidJson);

  PolicyBundle bundle;
  PolicyMap& policies = bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, ""));

  base::WeakPtr<ExternalDataManager> external_data_manager;
  DecodeDevicePolicy(device_policy, external_data_manager, &policies);

  const PolicyMap::Entry* entry = policies.Get(key::kDeviceRestrictionSchedule);
  ASSERT_NE(entry, nullptr);
  EXPECT_TRUE(entry->HasMessage(PolicyMap::MessageType::kError));
  EXPECT_TRUE(base::StartsWith(
      entry->GetLocalizedMessages(PolicyMap::MessageType::kError,
                                  PolicyMap::Entry::L10nLookupFunction()),
      kInvalidJsonParsingErrorPrefix));
}

}  // namespace policy
