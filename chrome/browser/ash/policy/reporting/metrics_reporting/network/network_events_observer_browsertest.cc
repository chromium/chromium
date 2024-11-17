// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/network/network_events_observer.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/user_auth_config.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "chromeos/services/network_health/public/mojom/network_health_types.mojom.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using ::testing::Eq;

namespace reporting {
namespace {

constexpr int kSignalStrength = 50;
constexpr char kWifiGuid[] = "wifi-guid";
constexpr char kWifiDevicePath[] = "/device/wlan";
constexpr char kWifiServicePath[] = "/service/wlan";
constexpr char kWifiConfig[] =
    R"({"GUID": "%s", "Type": "wifi", "State": "online",
    "WiFi.SignalStrengthRssi": %d})";
constexpr int kGoodSignalStrengthRssi = -50;
constexpr int kLowSignalStrengthRssi = -75;

bool IsEventDrivenNetworkTelemetry(const ::reporting::Record& record) {
  if (record.destination() != Destination::TELEMETRY_METRIC) {
    return false;
  }

  MetricData record_data;
  EXPECT_TRUE(record_data.ParseFromString(record.data()));

  return record_data.has_telemetry_data() &&
         record_data.telemetry_data().is_event_driven();
}

Record GetNextRecord(::chromeos::MissiveClientTestObserver* observer,
                     Priority expectedPriority) {
  const std::tuple<Priority, Record>& enqueued_record =
      observer->GetNextEnqueuedRecord();
  Priority priority = std::get<0>(enqueued_record);
  Record record = std::get<1>(enqueued_record);
  EXPECT_THAT(priority, Eq(expectedPriority));
  return record;
}

class NetworkEventsBrowserTest : public ::policy::DevicePolicyCrosBrowserTest {
 protected:
  NetworkEventsBrowserTest() {
    crypto_home_mixin_.MarkUserAsExisting(affiliation_mixin_.account_id());
    crypto_home_mixin_.ApplyAuthConfig(
        affiliation_mixin_.account_id(),
        ash::test::UserAuthConfig::Create(ash::test::kDefaultAuthSetup));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ::policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
        command_line);
    ::policy::DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ::policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    if (content::IsPreTest()) {
      // Preliminary setup - set up affiliated user
      ::policy::AffiliationTestHelper::PreLoginUser(
          affiliation_mixin_.account_id());

      return;
    }
    network_handler_test_helper_ =
        std::make_unique<::ash::NetworkHandlerTestHelper>();
    network_handler_test_helper_->AddDefaultProfiles();
    network_handler_test_helper_->ResetDevicesAndServices();
    auto* const device_client = network_handler_test_helper_->device_test();
    device_client->AddDevice(kWifiDevicePath, shill::kTypeWifi, "wifi0");
    base::RunLoop().RunUntilIdle();
    auto* const service_client = network_handler_test_helper_->service_test();
    service_client->AddService(kWifiServicePath, kWifiGuid, "wifi-name",
                               shill::kTypeWifi, shill::kStateOnline, true);
    service_client->SetServiceProperty(kWifiServicePath, shill::kDeviceProperty,
                                       base::Value(kWifiDevicePath));
    std::string service_config_good_signal =
        base::StringPrintf(kWifiConfig, kWifiGuid, kGoodSignalStrengthRssi);
    network_handler_test_helper_->ConfigureService(service_config_good_signal);
    service_client->SetServiceProperty(
        kWifiServicePath, shill::kProfileProperty,
        base::Value(network_handler_test_helper_->ProfilePathUser()));

    ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  }

  void EnablePolicy() {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ash::kDeviceReportNetworkEvents, true);
  }

  void SetWifiSignalEventDrivenPolicy() {
    base::Value::List telemetry_list;
    telemetry_list.Append("network_telemetry");
    scoped_testing_cros_settings_.device_settings()->Set(
        ash::kReportDeviceSignalStrengthEventDrivenTelemetry,
        base::Value(std::move(telemetry_list)));
  }

  std::unique_ptr<::ash::NetworkHandlerTestHelper> network_handler_test_helper_;

  ::policy::DevicePolicyCrosTestHelper test_helper_;
  ::policy::AffiliationMixin affiliation_mixin_{&mixin_host_, &test_helper_};
  ash::CryptohomeMixin crypto_home_mixin_{&mixin_host_};
  ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

IN_PROC_BROWSER_TEST_F(NetworkEventsBrowserTest,
                       PRE_ConnectionStateAffiliatedUserAndPolicyEnabled) {
  // dummy case to register user.
}

IN_PROC_BROWSER_TEST_F(NetworkEventsBrowserTest,
                       ConnectionStateAffiliatedUserAndPolicyEnabled) {
  chromeos::MissiveClientTestObserver missive_event_observer(
      Destination::EVENT_METRIC);

  EnablePolicy();
  ash::ShillServiceClient::Get()->GetTestInterface()->SetServiceProperty(
      kWifiServicePath, shill::kStateProperty, base::Value(shill::kStateIdle));

  const Record& record =
      GetNextRecord(&missive_event_observer, Priority::SLOW_BATCH);
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::ASH));
  MetricData record_data;

  ASSERT_TRUE(record_data.ParseFromString(record.data()));
  // Testing event found successfully.
  EXPECT_THAT(record_data.event_data().type(),
              Eq(MetricEventType::NETWORK_STATE_CHANGE));
}

IN_PROC_BROWSER_TEST_F(NetworkEventsBrowserTest,
                       PRE_SignalStrengthAffiliatedUserAndPolicyEnabled) {
  // dummy case to register user.
}

IN_PROC_BROWSER_TEST_F(NetworkEventsBrowserTest,
                       SignalStrengthAffiliatedUserAndPolicyEnabled) {
  chromeos::MissiveClientTestObserver missive_event_observer(
      Destination::EVENT_METRIC);
  chromeos::MissiveClientTestObserver missive_telemetry_observer(
      base::BindRepeating(&IsEventDrivenNetworkTelemetry));

  const std::string service_config_low_signal =
      base::StringPrintf(kWifiConfig, kWifiGuid, kLowSignalStrengthRssi);
  network_handler_test_helper_->ConfigureService(service_config_low_signal);

  EnablePolicy();
  SetWifiSignalEventDrivenPolicy();
  ash::ShillServiceClient::Get()->GetTestInterface()->SetServiceProperty(
      kWifiServicePath, shill::kSignalStrengthProperty,
      base::Value(kSignalStrength));

  Record record = GetNextRecord(&missive_event_observer, Priority::SLOW_BATCH);
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::ASH));
  MetricData event_record_data;

  ASSERT_TRUE(event_record_data.ParseFromString(record.data()));
  EXPECT_THAT(event_record_data.event_data().type(),
              Eq(MetricEventType::WIFI_SIGNAL_STRENGTH_LOW));

  record = GetNextRecord(&missive_telemetry_observer, Priority::MANUAL_BATCH);
  ASSERT_TRUE(record.has_source_info());
  EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::ASH));
  MetricData telemetry_record_data;

  ASSERT_TRUE(telemetry_record_data.ParseFromString(record.data()));
  ASSERT_TRUE(telemetry_record_data.telemetry_data().has_networks_telemetry());
  EXPECT_FALSE(telemetry_record_data.telemetry_data()
                   .networks_telemetry()
                   .network_telemetry()
                   .empty());
}

}  // namespace
}  // namespace reporting
