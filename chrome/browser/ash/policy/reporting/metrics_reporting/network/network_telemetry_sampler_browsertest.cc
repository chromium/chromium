// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_mixin.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/ash/services/cros_healthd/public/cpp/fake_cros_healthd.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_probe.mojom.h"
#include "chromeos/dbus/missive/missive_client_test_observer.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/mock_clock.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_launcher.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

using enterprise_management::DeviceReportingProto;
using enterprise_management::PolicyData;
using testing::Eq;
using testing::SizeIs;
using testing::StrEq;

namespace reporting {
namespace {

constexpr char kWifiDevicePath[] = "/device/wlan";
constexpr char kWifiDeviceName[] = "reporting-wifi0";
constexpr char kAccessPointAddress[] = "00:00:5e:00:53:af";
constexpr bool kEncryptionOn = true;
constexpr bool kPowerManagementOn = true;
constexpr int64_t kTxBitRateMbps = 8;
constexpr int64_t kRxBitRateMbps = 4;
constexpr int64_t kTxPowerDbm = 2;
constexpr int64_t kLinkQuality = 1;
constexpr char kWifiGuid[] = "wifi-guid";
constexpr int kSignalStrength = 80;
constexpr int kSignalStrengthRssi = -50;
constexpr char kIpAddress[] = "192.168.1.240";
constexpr char kGateway[] = "192.168.1.1";
constexpr char kIPConfigPath[] = "ip/config/path";

bool IsNetworkTelemetry(const Record& record) {
  MetricData record_data;
  return record.destination() == Destination::TELEMETRY_METRIC &&
         record_data.ParseFromString(record.data()) &&
         !record_data.telemetry_data()
              .networks_telemetry()
              .network_telemetry()
              .empty();
}

void VerifyNetworkTelemetryData(const MetricData& metric_data) {
  ASSERT_THAT(
      metric_data.telemetry_data().networks_telemetry().network_telemetry(),
      SizeIs(1));
  const auto& network_telemetry =
      metric_data.telemetry_data().networks_telemetry().network_telemetry(0);
  EXPECT_THAT(network_telemetry.guid(), StrEq(kWifiGuid));
  EXPECT_THAT(network_telemetry.type(), Eq(NetworkType::WIFI));
  EXPECT_THAT(network_telemetry.connection_state(),
              Eq(NetworkConnectionState::ONLINE));
  EXPECT_THAT(network_telemetry.ip_address(), StrEq(kIpAddress));
  EXPECT_THAT(network_telemetry.gateway(), StrEq(kGateway));
  EXPECT_THAT(network_telemetry.signal_strength(), Eq(kSignalStrength));
  EXPECT_THAT(network_telemetry.signal_strength_dbm(), Eq(kSignalStrengthRssi));
  EXPECT_THAT(network_telemetry.tx_bit_rate_mbps(), Eq(kTxBitRateMbps));
  EXPECT_THAT(network_telemetry.rx_bit_rate_mbps(), Eq(kRxBitRateMbps));
  EXPECT_THAT(network_telemetry.link_quality(), Eq(kLinkQuality));
  EXPECT_TRUE(network_telemetry.power_management_enabled());
  EXPECT_TRUE(network_telemetry.encryption_on());
}

class DeviceSettingsServiceWaiter
    : public ::ash::DeviceSettingsService::Observer {
 public:
  DeviceSettingsServiceWaiter() {
    CHECK(::ash::DeviceSettingsService::IsInitialized());
    device_settings_observation_.Observe(::ash::DeviceSettingsService::Get());
  }

  DeviceSettingsServiceWaiter(const DeviceSettingsServiceWaiter&) = delete;
  DeviceSettingsServiceWaiter& operator=(const DeviceSettingsServiceWaiter&) =
      delete;

  ~DeviceSettingsServiceWaiter() override = default;

  void Wait() { run_loop.Run(); }

 private:
  // ::ash::DeviceSettingsService::Observer:
  void DeviceSettingsUpdated() override { run_loop.Quit(); }

  base::RunLoop run_loop;
  base::ScopedObservation<::ash::DeviceSettingsService,
                          ::ash::DeviceSettingsService::Observer>
      device_settings_observation_{this};
};

class NetworkTelemetrySamplerBrowserTest
    : public ::policy::DevicePolicyCrosBrowserTest {
 protected:
  NetworkTelemetrySamplerBrowserTest() {
    test::MockClock::Get();
    crypto_home_mixin_.MarkUserAsExisting(affiliation_mixin_.account_id());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ::policy::AffiliationTestHelper::AppendCommandLineSwitchesForLoginManager(
        command_line);
    ::policy::DevicePolicyCrosBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ::policy::DevicePolicyCrosBrowserTest::SetUpOnMainThread();
    if (::content::IsPreTest()) {
      // Preliminary setup - set up affiliated user.
      ::policy::AffiliationTestHelper::PreLoginUser(
          affiliation_mixin_.account_id());
      return;
    }
    SetupNetworks();
    ::policy::AffiliationTestHelper::LoginUser(affiliation_mixin_.account_id());
  }

  void SetupNetworks() {
    network_handler_test_helper_ =
        std::make_unique<::ash::NetworkHandlerTestHelper>();
    network_handler_test_helper_->AddDefaultProfiles();
    network_handler_test_helper_->ResetDevicesAndServices();

    network_handler_test_helper_->device_test()->AddDevice(
        kWifiDevicePath, shill::kTypeWifi, kWifiDeviceName);
    network_handler_test_helper_->device_test()->SetDeviceProperty(
        kWifiDevicePath, shill::kInterfaceProperty,
        base::Value(kWifiDeviceName),
        /*notify_changed=*/true);
    base::RunLoop().RunUntilIdle();

    auto ip_config_properties = base::Value::Dict()
                                    .Set(shill::kAddressProperty, kIpAddress)
                                    .Set(shill::kGatewayProperty, kGateway);
    network_handler_test_helper_->ip_config_test()->AddIPConfig(
        kIPConfigPath, std::move(ip_config_properties));

    std::string service_config = base::StringPrintf(
        R"({"GUID": "%s", "Type": "%s", "State": "%s", "Strength": %d,
        "WiFi.SignalStrengthRssi": %d, "IPConfig": "%s"})",
        kWifiGuid, shill::kTypeWifi, shill::kStateOnline, kSignalStrength,
        kSignalStrengthRssi, kIPConfigPath);
    const std::string service_path =
        network_handler_test_helper_->ConfigureService(service_config);
    ASSERT_FALSE(service_path.empty());
    network_handler_test_helper_->service_test()->SetServiceProperty(
        service_path, shill::kDeviceProperty, base::Value(kWifiDevicePath));
    network_handler_test_helper_->service_test()->SetServiceProperty(
        service_path, shill::kProfileProperty,
        base::Value(network_handler_test_helper_->ProfilePathUser()));

    SetWifiInterfaceData();
  }

  void SetWifiInterfaceData() {
    auto telemetry_info = ::ash::cros_healthd::mojom::TelemetryInfo::New();
    std::vector<::ash::cros_healthd::mojom::NetworkInterfaceInfoPtr>
        network_interfaces;

    auto wireless_link_info = ::ash::cros_healthd::mojom::WirelessLinkInfo::New(
        kAccessPointAddress, kTxBitRateMbps, kRxBitRateMbps, kTxPowerDbm,
        kEncryptionOn, kLinkQuality, kSignalStrengthRssi);
    auto wireless_interface_info =
        ::ash::cros_healthd::mojom::WirelessInterfaceInfo::New(
            kWifiDeviceName, kPowerManagementOn, std::move(wireless_link_info));
    network_interfaces.push_back(
        ::ash::cros_healthd::mojom::NetworkInterfaceInfo::
            NewWirelessInterfaceInfo(std::move(wireless_interface_info)));
    auto network_interface_result =
        ::ash::cros_healthd::mojom::NetworkInterfaceResult::
            NewNetworkInterfaceInfo(std::move(network_interfaces));

    telemetry_info->network_interface_result =
        std::move(network_interface_result);
    ::ash::cros_healthd::FakeCrosHealthd::Get()
        ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
  }

  void SetReportNetworkStatusPolicy(bool enabled) {
    bool network_status_enabled;
    base::test::TestFuture<void> test_future;
    base::CallbackListSubscription subscription =
        ::ash::CrosSettings::Get()->AddSettingsObserver(
            ::ash::kReportDeviceNetworkStatus,
            test_future.GetRepeatingCallback());
    device_reporting()->set_report_network_status(enabled);
    policy_helper_.RefreshDevicePolicy();

    ASSERT_TRUE(test_future.Wait());
    ASSERT_TRUE(::ash::CrosSettings::Get()->GetBoolean(
        ::ash::kReportDeviceNetworkStatus, &network_status_enabled));
    ASSERT_THAT(network_status_enabled, Eq(enabled));
  }

  void SetReportNetworkTelemetryCollectionRateMs(int64_t rate) {
    int collection_rate;
    base::test::TestFuture<void> test_future;
    base::CallbackListSubscription subscription =
        ::ash::CrosSettings::Get()->AddSettingsObserver(
            ::ash::kReportDeviceNetworkTelemetryCollectionRateMs,
            test_future.GetRepeatingCallback());
    device_reporting()->set_report_network_telemetry_collection_rate_ms(rate);
    policy_helper_.RefreshDevicePolicy();

    ASSERT_TRUE(test_future.Wait());
    ASSERT_TRUE(::ash::CrosSettings::Get()->GetInteger(
        ::ash::kReportDeviceNetworkTelemetryCollectionRateMs,
        &collection_rate));
    ASSERT_THAT(collection_rate, Eq(rate));
  }

  void Deprovision() {
    DeviceSettingsServiceWaiter waiter;
    policy_helper_.device_policy()->policy_data().set_state(
        PolicyData::DEPROVISIONED);
    policy_helper_.RefreshDevicePolicy();
    waiter.Wait();

    ASSERT_THAT(::ash::DeviceSettingsService::Get()->policy_data()->state(),
                Eq(PolicyData::DEPROVISIONED));
  }

  DeviceReportingProto* device_reporting() {
    return policy_helper_.device_policy()->payload().mutable_device_reporting();
  }

  std::unique_ptr<::ash::NetworkHandlerTestHelper> network_handler_test_helper_;
  ::policy::DevicePolicyCrosTestHelper policy_helper_;
  ::policy::AffiliationMixin affiliation_mixin_{&mixin_host_, &policy_helper_};
  ::ash::CryptohomeMixin crypto_home_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(NetworkTelemetrySamplerBrowserTest, PRE_Default) {
  // Simple case that sets up the affiliated user through SetUpOnMainThread
  // PRE-condition.
}

// TODO(crbug.com/40939150): Test is flaky on multiple CrOS builders.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_Default DISABLED_Default
#else
#define MAYBE_Default Default
#endif
IN_PROC_BROWSER_TEST_F(NetworkTelemetrySamplerBrowserTest, MAYBE_Default) {
  ::chromeos::MissiveClientTestObserver missive_observer(
      base::BindRepeating(&IsNetworkTelemetry));

  {
    // Initial collection, policy not set but default is true.
    MetricData record_data;
    test::MockClock::Get().Advance(metrics::kInitialCollectionDelay);
    const auto [priority, record] = missive_observer.GetNextEnqueuedRecord();

    EXPECT_THAT(priority, Eq(Priority::MANUAL_BATCH));
    ASSERT_TRUE(record.has_source_info());
    EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::ASH));
    ASSERT_TRUE(record_data.ParseFromString(record.data()));
    VerifyNetworkTelemetryData(record_data);
    ASSERT_FALSE(missive_observer.HasNewEnqueuedRecord());
  }

  {
    // Collection rate policy not set, collect on default collection rate.
    MetricData record_data;
    test::MockClock::Get().Advance(
        metrics::kDefaultNetworkTelemetryCollectionRate);
    const auto [priority, record] = missive_observer.GetNextEnqueuedRecord();

    EXPECT_THAT(priority, Eq(Priority::MANUAL_BATCH));
    ASSERT_TRUE(record.has_source_info());
    EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::ASH));
    ASSERT_TRUE(record_data.ParseFromString(record.data()));
    VerifyNetworkTelemetryData(record_data);
    ASSERT_FALSE(missive_observer.HasNewEnqueuedRecord());
  }

  SetReportNetworkStatusPolicy(false);
  {
    // Reporting is disabled, no network telemetry data should be collected.
    MetricData record_data;
    test::MockClock::Get().Advance(
        metrics::kDefaultNetworkTelemetryCollectionRate);
    base::RunLoop().RunUntilIdle();

    ASSERT_FALSE(missive_observer.HasNewEnqueuedRecord());
  }

  // Set collection rate policy to double the default rate.
  base::TimeDelta collection_rate =
      metrics::kDefaultNetworkTelemetryCollectionRate * 2;
  SetReportNetworkTelemetryCollectionRateMs(collection_rate.InMilliseconds());

  // Advance time by few hours before re-enabling the policy.
  test::MockClock::Get().Advance(base::Hours(5));
  SetReportNetworkStatusPolicy(true);
  {
    // Policy re-enabled, one immediate collection should take place.
    MetricData record_data;
    const auto [priority, record] = missive_observer.GetNextEnqueuedRecord();

    EXPECT_THAT(priority, Eq(Priority::MANUAL_BATCH));
    ASSERT_TRUE(record.has_source_info());
    EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::ASH));
    ASSERT_TRUE(record_data.ParseFromString(record.data()));
    VerifyNetworkTelemetryData(record_data);
    base::RunLoop().RunUntilIdle();
    ASSERT_FALSE(missive_observer.HasNewEnqueuedRecord());
  }

  {
    // Collection rate policy set to double the default rate.
    MetricData record_data;
    test::MockClock::Get().Advance(
        metrics::kDefaultNetworkTelemetryCollectionRate);
    base::RunLoop().RunUntilIdle();

    // No data collected, only half of time elapsed.
    ASSERT_FALSE(missive_observer.HasNewEnqueuedRecord());

    // Advance the remaining time.
    test::MockClock::Get().Advance(
        collection_rate - metrics::kDefaultNetworkTelemetryCollectionRate);
    const auto [priority, record] = missive_observer.GetNextEnqueuedRecord();

    EXPECT_THAT(priority, Eq(Priority::MANUAL_BATCH));
    ASSERT_TRUE(record.has_source_info());
    EXPECT_THAT(record.source_info().source(), Eq(SourceInfo::ASH));
    ASSERT_TRUE(record_data.ParseFromString(record.data()));
    VerifyNetworkTelemetryData(record_data);
    base::RunLoop().RunUntilIdle();
    ASSERT_FALSE(missive_observer.HasNewEnqueuedRecord());
  }

  Deprovision();
  {
    // Device is deprovisioned, no network telemetry data should be collected.
    MetricData record_data;
    test::MockClock::Get().Advance(collection_rate);
    base::RunLoop().RunUntilIdle();

    ASSERT_FALSE(missive_observer.HasNewEnqueuedRecord());
  }
}

}  // namespace
}  // namespace reporting
