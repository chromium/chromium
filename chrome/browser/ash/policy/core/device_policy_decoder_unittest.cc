// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/core/device_policy_decoder.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/policy_constants.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr char kInvalidJson[] = R"({"foo": "bar")";

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
  void DecodeDevicePolicyTestHelper(
      const em::ChromeDeviceSettingsProto& device_policy,
      const std::string& policy_path,
      base::Value expected_value) const;
  void DecodeUnsetDevicePolicyTestHelper(
      const em::ChromeDeviceSettingsProto& device_policy,
      const std::string& policy_path) const;
};

base::Value DevicePolicyDecoderTest::GetWallpaperDict() const {
  base::Value::Dict dict;
  dict.Set(kWallpaperUrlPropertyName, kWallpaperUrlPropertyValue);
  dict.Set(kWallpaperHashPropertyName, kWallpaperHashPropertyValue);
  return base::Value(std::move(dict));
}

base::Value DevicePolicyDecoderTest::GetBluetoothServiceAllowedList() const {
  base::Value::List list;
  list.Append(kValidBluetoothServiceUUID4);
  list.Append(kValidBluetoothServiceUUID8);
  list.Append(kValidBluetoothServiceUUID32);
  return base::Value(std::move(list));
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
  std::string error;
  absl::optional<base::Value> decoded_json = DecodeJsonStringAndNormalize(
      kInvalidJson, key::kDeviceWallpaperImage, &error);
  std::string localized_error = l10n_util::GetStringFUTF8(
      IDS_POLICY_PROTO_PARSING_ERROR, base::UTF8ToUTF16(error));
  EXPECT_FALSE(decoded_json.has_value());
  EXPECT_NE(
      std::string::npos,
      localized_error.find(
          "Policy parsing error: Invalid JSON string: Line: 1, column: 14,"));
}

#if GTEST_HAS_DEATH_TEST
TEST_F(DevicePolicyDecoderTest, DecodeJsonStringAndNormalizeInvalidSchema) {
  std::string error;
  EXPECT_DEATH(
      DecodeJsonStringAndNormalize(kWallpaperJson, kInvalidPolicyName, &error),
      "");
}
#endif

TEST_F(DevicePolicyDecoderTest, DecodeJsonStringAndNormalizeInvalidValue) {
  std::string error;
  absl::optional<base::Value> decoded_json = DecodeJsonStringAndNormalize(
      kWallpaperJsonInvalidValue, key::kDeviceWallpaperImage, &error);
  EXPECT_FALSE(decoded_json.has_value());
  std::string localized_error = l10n_util::GetStringFUTF8(
      IDS_POLICY_PROTO_PARSING_ERROR, base::UTF8ToUTF16(error));
  EXPECT_EQ(
      "Policy parsing error: Invalid policy value: Policy type mismatch: "
      "expected: \"string\", actual: \"integer\". (at "
      "DeviceWallpaperImage.url)",
      localized_error);
}

TEST_F(DevicePolicyDecoderTest, DecodeJsonStringAndNormalizeUnknownProperty) {
  std::string error;
  absl::optional<base::Value> decoded_json = DecodeJsonStringAndNormalize(
      kWallpaperJsonUnknownProperty, key::kDeviceWallpaperImage, &error);
  std::string localized_error = l10n_util::GetStringFUTF8(
      IDS_POLICY_PROTO_PARSING_ERROR, base::UTF8ToUTF16(error));
  EXPECT_EQ(GetWallpaperDict(), decoded_json.value());
  EXPECT_EQ(
      "Policy parsing error: Dropped unknown properties: Unknown property: "
      "unknown-field (at DeviceWallpaperImage)",
      localized_error);
}

TEST_F(DevicePolicyDecoderTest, DecodeJsonStringAndNormalizeSuccess) {
  std::string error;
  absl::optional<base::Value> decoded_json = DecodeJsonStringAndNormalize(
      kWallpaperJson, key::kDeviceWallpaperImage, &error);
  EXPECT_EQ(GetWallpaperDict(), decoded_json.value());
  EXPECT_TRUE(error.empty());
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

  base::Value::List signal_strength_telemetry_list;
  signal_strength_telemetry_list.Append("network_telemetry");
  signal_strength_telemetry_list.Append("https_latency");
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

TEST_F(DevicePolicyDecoderTest, DecodeServiceUUIDListSuccess) {
  std::string error;
  absl::optional<base::Value> decoded_json = DecodeJsonStringAndNormalize(
      kValidBluetoothServiceUUIDList, key::kDeviceAllowedBluetoothServices,
      &error);
  EXPECT_EQ(GetBluetoothServiceAllowedList(), decoded_json.value());
  EXPECT_TRUE(error.empty());
}

TEST_F(DevicePolicyDecoderTest, DecodeServiceUUIDListError) {
  std::string error;
  absl::optional<base::Value> decoded_json = DecodeJsonStringAndNormalize(
      kInvalidBluetoothServiceUUIDList, key::kDeviceAllowedBluetoothServices,
      &error);
  EXPECT_FALSE(decoded_json.has_value());
  EXPECT_EQ(
      "Invalid policy value: Invalid value for string (at "
      "DeviceAllowedBluetoothServices[0])",
      error);
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

}  // namespace policy
