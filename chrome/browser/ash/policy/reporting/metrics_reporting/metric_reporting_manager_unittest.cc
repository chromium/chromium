// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/components/settings/cros_settings_names.h"
#include "ash/components/settings/cros_settings_provider.h"
#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/dbus/cros_healthd/cros_healthd_client.h"
#include "chromeos/dbus/cros_healthd/fake_cros_healthd_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_test_helper.h"
#include "chromeos/services/cros_healthd/public/cpp/service_connection.h"
#include "chromeos/services/network_health/public/mojom/network_health.mojom.h"
#include "components/reporting/client/mock_report_queue.h"
#include "components/reporting/metrics/fake_sampler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace reporting {
namespace {

class FakeDelegate : public MetricReportingManager::Delegate {
 public:
  FakeDelegate() = default;

  FakeDelegate(const FakeDelegate& other) = delete;
  FakeDelegate& operator=(const FakeDelegate& other) = delete;

  ~FakeDelegate() override = default;

  std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> CreateReportQueue(
      Destination destination) override {
    switch (destination) {
      case INFO_METRIC:
        return std::move(info_queue_);
      case TELEMETRY_METRIC:
        return std::move(telemetry_queue_);
      case EVENT_METRIC:
        return std::move(event_queue_);
      default:
        NOTREACHED();
        return std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter>(
            nullptr,
            base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get()));
    }
  }

  std::unique_ptr<Sampler> CreateHttpsLatencySampler() override {
    if (!latency_sampler_) {
      latency_sampler_ = std::make_unique<test::FakeSampler>();
    }
    return std::move(latency_sampler_);
  }

  bool IsDeprovisioned() override { return is_deprovisioned_; }

  bool IsAffiliated(Profile* profile) override { return is_affiliated_; }

  void SetIsAffiliated(bool is_affiliated) { is_affiliated_ = is_affiliated; }

  void SetInfoQueue(
      std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> info_queue) {
    info_queue_ = std::move(info_queue);
  }

  void SetTelemetryQueue(
      std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> telemetry_queue) {
    telemetry_queue_ = std::move(telemetry_queue);
  }

  void SetEventQueue(
      std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> event_queue) {
    event_queue_ = std::move(event_queue);
  }

  void SetHttpsLatencySampler(
      std::unique_ptr<test::FakeSampler> latency_sampler) {
    latency_sampler_ = std::move(latency_sampler);
  }

  void SetIsDeprovisioned(bool is_deprovisioned) {
    is_deprovisioned_ = is_deprovisioned;
  }

 private:
  bool is_affiliated_ = true;
  bool is_deprovisioned_ = false;

  std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> info_queue_{
      nullptr,
      base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get())};
  std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> telemetry_queue_{
      nullptr,
      base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get())};
  std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> event_queue_{
      nullptr,
      base::OnTaskRunnerDeleter(base::SequencedTaskRunnerHandle::Get())};

  std::unique_ptr<test::FakeSampler> latency_sampler_;
};

struct NetworkHealthReportingTestCase {
  std::string test_name;
  bool is_feature_enabled;
  bool is_deprovisioned;
  bool is_affiliated;
  // optional to test the cases where the policies are not set.
  absl::optional<bool> info_policy_enabled;
  absl::optional<bool> telemetry_policy_enabled;
  RoutineVerdict latency_verdict;
  int expected_info_count;
  int expected_telemetry_count;
  int expected_event_count;
  int expected_flush_per_period;
};

constexpr int kNetworkHealthRateMs = 60000;

class NetworkHealthReportingTest
    : public ::testing::TestWithParam<NetworkHealthReportingTestCase> {
 protected:
  NetworkHealthReportingTest() = default;

  NetworkHealthReportingTest(const NetworkHealthReportingTest&) = delete;
  NetworkHealthReportingTest& operator=(const NetworkHealthReportingTest&) =
      delete;

  ~NetworkHealthReportingTest() override = default;

  void SetUp() override {
    ::ash::CrosHealthdClient::InitializeFake();

    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner({});

    scoped_testing_cros_settings_.device_settings()->SetInteger(
        ::ash::kReportUploadFrequency, kNetworkHealthRateMs);
    scoped_testing_cros_settings_.device_settings()->SetInteger(
        ::ash::kReportDeviceNetworkTelemetryCollectionRateMs,
        kNetworkHealthRateMs);
    scoped_testing_cros_settings_.device_settings()->SetInteger(
        ::ash::kReportDeviceNetworkTelemetryEventCheckingRateMs,
        kNetworkHealthRateMs);

    const std::string kWifiPath = "wifi/path1";
    const std::string kProfilePath = "/profile/path";
    network_handler_test_helper_.profile_test()->AddProfile(kProfilePath,
                                                            "user_hash");
    auto* const device_client = network_handler_test_helper_.device_test();
    auto* const service_client = network_handler_test_helper_.service_test();
    base::RunLoop().RunUntilIdle();

    device_client->ClearDevices();
    service_client->ClearServices();
    device_client->AddDevice(kWifiPath, shill::kTypeEthernet, "ethernet");
    service_client->AddService(kWifiPath, "guid", "name", shill::kTypeWifi,
                               shill::kStateOnline,
                               /*is_visible=*/true);
    service_client->SetServiceProperty(kWifiPath, shill::kProfileProperty,
                                       base::Value(kProfilePath));
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    ::ash::CrosHealthdClient::Shutdown();
    ::ash::cros_healthd::ServiceConnection::GetInstance()->FlushForTesting();
  }

  void EmitSignalStrengthEvent() {
    base::RunLoop run_loop;
    ::ash::cros_healthd::FakeCrosHealthdClient::Get()
        ->EmitSignalStrengthChangedEventForTesting(
            "guid", ::chromeos::network_health::mojom::UInt32Value::New(50));
    base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                     run_loop.QuitClosure());
    run_loop.Run();
  }

  void SetWifiResult() {
    auto telemetry_info = chromeos::cros_healthd::mojom::TelemetryInfo::New();
    std::vector<chromeos::cros_healthd::mojom::NetworkInterfaceInfoPtr>
        network_interfaces;

    auto wireless_link_info =
        chromeos::cros_healthd::mojom::WirelessLinkInfo::New("", 0, 0, 0, 0, 0,
                                                             0);
    auto wireless_interface_info =
        chromeos::cros_healthd::mojom::WirelessInterfaceInfo::New(
            "path1", false, std::move(wireless_link_info));
    network_interfaces.push_back(
        chromeos::cros_healthd::mojom::NetworkInterfaceInfo::
            NewWirelessInterfaceInfo(std::move(wireless_interface_info)));
    auto network_interface_result =
        chromeos::cros_healthd::mojom::NetworkInterfaceResult::
            NewNetworkInterfaceInfo(std::move(network_interfaces));

    telemetry_info->network_interface_result =
        std::move(network_interface_result);
    ::ash::cros_healthd::FakeCrosHealthdClient::Get()
        ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  ::ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;

 private:
  ::ash::NetworkHandlerTestHelper network_handler_test_helper_;

  ::ash::ScopedTestDeviceSettingsService test_device_settings_service_;
};

TEST_P(NetworkHealthReportingTest, Info_Telemetry_LatencyEvent) {
  const NetworkHealthReportingTestCase& test_case = GetParam();

  if (test_case.info_policy_enabled.has_value()) {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ::ash::kReportDeviceNetworkConfiguration,
        test_case.info_policy_enabled.value());
  }
  if (test_case.telemetry_policy_enabled.has_value()) {
    scoped_testing_cros_settings_.device_settings()->SetBoolean(
        ::ash::kReportDeviceNetworkStatus,
        test_case.telemetry_policy_enabled.value());
  }
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureState(
      MetricReportingManager::kEnableNetworkTelemetryReporting,
      test_case.is_feature_enabled);

  auto fake_delegate = std::make_unique<FakeDelegate>();
  auto* const fake_delegate_ptr = fake_delegate.get();

  fake_delegate->SetIsAffiliated(test_case.is_affiliated);
  fake_delegate->SetIsDeprovisioned(test_case.is_deprovisioned);
  auto https_latency_sampler = std::make_unique<test::FakeSampler>();
  MetricData https_latency_data;
  https_latency_data.mutable_telemetry_data()
      ->mutable_networks_telemetry()
      ->mutable_https_latency_data()
      ->set_verdict(test_case.latency_verdict);
  https_latency_sampler->SetMetricData(std::move(https_latency_data));
  fake_delegate->SetHttpsLatencySampler(std::move(https_latency_sampler));
  SetWifiResult();

  int info_report_count = 0;
  int telemetry_report_count = 0;
  int telemetry_flush_count = 0;
  int event_report_count = 0;
  auto info_queue = std::unique_ptr<MockReportQueue, base::OnTaskRunnerDeleter>(
      new ::testing::NiceMock<MockReportQueue>(),
      base::OnTaskRunnerDeleter(task_runner_));
  auto telemetry_queue =
      std::unique_ptr<MockReportQueue, base::OnTaskRunnerDeleter>(
          new ::testing::NiceMock<MockReportQueue>(),
          base::OnTaskRunnerDeleter(task_runner_));
  auto event_queue =
      std::unique_ptr<MockReportQueue, base::OnTaskRunnerDeleter>(
          new ::testing::NiceMock<MockReportQueue>(),
          base::OnTaskRunnerDeleter(task_runner_));
  ON_CALL(*info_queue, AddRecord).WillByDefault([&]() { ++info_report_count; });
  ON_CALL(*telemetry_queue, AddRecord).WillByDefault([&]() {
    ++telemetry_report_count;
  });
  ON_CALL(*telemetry_queue, Flush).WillByDefault([&]() {
    ++telemetry_flush_count;
  });
  ON_CALL(*event_queue, AddRecord).WillByDefault([&]() {
    ++event_report_count;
  });

  fake_delegate->SetInfoQueue(std::move(info_queue));
  fake_delegate->SetTelemetryQueue(std::move(telemetry_queue));
  fake_delegate->SetEventQueue(std::move(event_queue));

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(fake_delegate), nullptr);
  base::RunLoop run_loop;
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(info_report_count, test_case.expected_info_count);

  task_environment_.FastForwardBy(base::Milliseconds(kNetworkHealthRateMs));
  EXPECT_EQ(telemetry_report_count, 0);
  EXPECT_EQ(telemetry_flush_count, test_case.expected_flush_per_period);
  EXPECT_EQ(event_report_count, 0);

  metric_reporting_manager->OnLogin(nullptr);

  // Reset flush count.
  telemetry_flush_count = 0;
  task_environment_.FastForwardBy(base::Milliseconds(kNetworkHealthRateMs));
  EXPECT_EQ(telemetry_report_count, test_case.expected_telemetry_count);
  EXPECT_EQ(telemetry_flush_count, test_case.expected_flush_per_period);
  EXPECT_EQ(event_report_count, test_case.expected_event_count);

  fake_delegate_ptr->SetIsDeprovisioned(true);
  metric_reporting_manager->DeviceSettingsUpdated();

  // Device is deprovisioned, so no reporting, reset all counts.
  telemetry_report_count = 0;
  telemetry_flush_count = 0;
  event_report_count = 0;
  task_environment_.FastForwardBy(base::Milliseconds(kNetworkHealthRateMs));
  EXPECT_EQ(telemetry_report_count, 0);
  EXPECT_EQ(telemetry_flush_count, 0);
  EXPECT_EQ(event_report_count, 0);
}

TEST_F(NetworkHealthReportingTest, NetworkEventsObserver) {
  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      ::ash::kReportDeviceNetworkStatus, true);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatureState(
      MetricReportingManager::kEnableNetworkTelemetryReporting, true);

  auto fake_delegate = std::make_unique<FakeDelegate>();
  fake_delegate->SetIsAffiliated(true);

  int event_reported_count = 0;
  auto event_queue =
      std::unique_ptr<MockReportQueue, base::OnTaskRunnerDeleter>(
          new ::testing::NiceMock<MockReportQueue>(),
          base::OnTaskRunnerDeleter(task_runner_));
  ON_CALL(*event_queue, AddRecord).WillByDefault([&]() {
    ++event_reported_count;
  });
  fake_delegate->SetEventQueue(std::move(event_queue));

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(fake_delegate), nullptr);

  metric_reporting_manager->OnLogin(nullptr);
  base::RunLoop run_loop;
  EmitSignalStrengthEvent();
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(event_reported_count, 1);
}

INSTANTIATE_TEST_SUITE_P(
    NetworkHealthReportingTests,
    NetworkHealthReportingTest,
    ::testing::ValuesIn<NetworkHealthReportingTestCase>(
        {{"FeatureDisabled", /*is_feature_enabled=*/false,
          /*is_deprovisioned=*/false,
          /*is_affiliated=*/true, /*info_policy_enabled=*/true,
          /*telemetry_policy_enabled=*/true,
          /*latency_verdict=*/RoutineVerdict::PROBLEM,
          /*expected_info_count=*/0,
          /*expected_telemetry_count=*/0, /*expected_event_count=*/0,
          /*expected_flush_per_period=*/1},
         {"Deprovisioned", /*is_feature_enabled=*/true,
          /*is_deprovisioned=*/true,
          /*is_affiliated=*/true, /*info_policy_enabled=*/true,
          /*telemetry_policy_enabled=*/true,
          /*latency_verdict=*/RoutineVerdict::PROBLEM,
          /*expected_info_count=*/0,
          /*expected_telemetry_count=*/0, /*expected_event_count=*/0,
          /*expected_flush_per_period=*/0},
         {"NotAffiliated", /*is_feature_enabled=*/true,
          /*is_deprovisioned=*/false,
          /*is_affiliated=*/false, /*info_policy_enabled=*/true,
          /*telemetry_policy_enabled=*/true,
          /*latency_verdict=*/RoutineVerdict::PROBLEM,
          /*expected_info_count=*/1,
          /*expected_telemetry_count=*/0, /*expected_event_count=*/0,
          /*expected_flush_per_period=*/1},
         {"InfoPolicyDisabled", /*is_feature_enabled=*/true,
          /*is_deprovisioned=*/false,
          /*is_affiliated=*/true, /*info_policy_enabled=*/false,
          /*telemetry_policy_enabled=*/true,
          /*latency_verdict=*/RoutineVerdict::PROBLEM,
          /*expected_info_count=*/0,
          /*expected_telemetry_count=*/1, /*expected_event_count=*/1,
          /*expected_flush_per_period=*/1},
         {"TelemetryPolicyDisabled", /*is_feature_enabled=*/true,
          /*is_deprovisioned=*/false,
          /*is_affiliated=*/true, /*info_policy_enabled=*/true,
          /*telemetry_policy_enabled=*/false,
          /*latency_verdict=*/RoutineVerdict::PROBLEM,
          /*expected_info_count=*/1,
          /*expected_telemetry_count=*/0, /*expected_event_count=*/0,
          /*expected_flush_per_period=*/1},
         {"InfoPolicyDefaultEnabled", /*is_feature_enabled=*/true,
          /*is_deprovisioned=*/false,
          /*is_affiliated=*/true, /*info_policy_enabled=*/absl::nullopt,
          /*telemetry_policy_enabled=*/false,
          /*latency_verdict=*/RoutineVerdict::PROBLEM,
          /*expected_info_count=*/1,
          /*expected_telemetry_count=*/0, /*expected_event_count=*/0,
          /*expected_flush_per_period=*/1},
         {"TelemetryPolicyDefaultEnabled", /*is_feature_enabled=*/true,
          /*is_deprovisioned=*/false,
          /*is_affiliated=*/true, /*info_policy_enabled=*/false,
          /*telemetry_policy_enabled=*/absl::nullopt,
          /*latency_verdict=*/RoutineVerdict::PROBLEM,
          /*expected_info_count=*/0,
          /*expected_telemetry_count=*/1, /*expected_event_count=*/1,
          /*expected_flush_per_period=*/1},
         {"LatencyVerdictNoProblem", /*is_feature_enabled=*/true,
          /*is_deprovisioned=*/false,
          /*is_affiliated=*/true, /*info_policy_enabled=*/true,
          /*telemetry_policy_enabled=*/true,
          /*latency_verdict=*/RoutineVerdict::NO_PROBLEM,
          /*expected_info_count=*/1,
          /*expected_telemetry_count=*/1, /*expected_event_count=*/0,
          /*expected_flush_per_period=*/1},
         {"Default", /*is_feature_enabled=*/true, /*is_deprovisioned=*/false,
          /*is_affiliated=*/true, /*info_policy_enabled=*/true,
          /*telemetry_policy_enabled=*/true,
          /*latency_verdict=*/RoutineVerdict::PROBLEM,
          /*expected_info_count=*/1,
          /*expected_telemetry_count=*/1, /*expected_event_count=*/1,
          /*expected_flush_per_period=*/1}}),
    [](const testing::TestParamInfo<NetworkHealthReportingTest::ParamType>&
           info) { return info.param.test_name; });

struct HealthdInfoReportingTestCase {
  std::string test_name;
  bool is_deprovisioned;
  std::string policy_path;
  bool policy_enabled;
  int expected_info_count;
};

class HealthdInfoReportingTest
    : public ::testing::TestWithParam<HealthdInfoReportingTestCase> {
 public:
 protected:
  void SetUp() override {
    task_runner_ = base::ThreadPool::CreateSequencedTaskRunner({});
    chromeos::cros_healthd::FakeCrosHealthdClient::InitializeFake();
  }

  void TearDown() override {
    chromeos::cros_healthd::FakeCrosHealthdClient::Shutdown();
    chromeos::cros_healthd::ServiceConnection::GetInstance()->FlushForTesting();
  }

  void SetHealthdMetricSamplerResult() {
    auto telemetry_info = chromeos::cros_healthd::mojom::TelemetryInfo::New();

    telemetry_info
        ->cpu_result = chromeos::cros_healthd::mojom::CpuResult::NewCpuInfo(
        chromeos::cros_healthd::mojom::CpuInfo::New(
            0, chromeos::cros_healthd::mojom::CpuArchitectureEnum::kX86_64,
            std::vector<chromeos::cros_healthd::mojom::PhysicalCpuInfoPtr>(),
            std::vector<
                chromeos::cros_healthd::mojom::CpuTemperatureChannelPtr>(),
            chromeos::cros_healthd::mojom::KeylockerInfo::New(false)));

    telemetry_info->memory_result =
        chromeos::cros_healthd::mojom::MemoryResult::NewMemoryInfo(
            chromeos::cros_healthd::mojom::MemoryInfo::New(
                0, 0, 0, 0,
                chromeos::cros_healthd::mojom::MemoryEncryptionInfo::New(
                    chromeos::cros_healthd::mojom::EncryptionState::
                        kEncryptionDisabled,
                    0, 0,
                    chromeos::cros_healthd::mojom::CryptoAlgorithm::
                        kAesXts128)));

    std::vector<chromeos::cros_healthd::mojom::BusDevicePtr> bus_devices;
    auto tbt_device = chromeos::cros_healthd::mojom::BusDevice::New();
    tbt_device->bus_info =
        chromeos::cros_healthd::mojom::BusInfo::NewThunderboltBusInfo(
            chromeos::cros_healthd::mojom::ThunderboltBusInfo::New(
                chromeos::cros_healthd::mojom::ThunderboltSecurityLevel::kNone,
                std::vector<chromeos::cros_healthd::mojom::
                                ThunderboltBusInterfaceInfoPtr>()));
    bus_devices.push_back(std::move(tbt_device));
    telemetry_info->bus_result =
        chromeos::cros_healthd::mojom::BusResult::NewBusDevices(
            std::move(bus_devices));

    chromeos::cros_healthd::FakeCrosHealthdClient::Get()
        ->SetProbeTelemetryInfoResponseForTesting(telemetry_info);
  }

  base::test::TaskEnvironment task_environment_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  ::ash::ScopedTestingCrosSettings scoped_testing_cros_settings_;
};

TEST_P(HealthdInfoReportingTest, HealthdInfoTest) {
  const HealthdInfoReportingTestCase& test_case = GetParam();

  scoped_testing_cros_settings_.device_settings()->SetBoolean(
      test_case.policy_path, test_case.policy_enabled);

  auto fake_delegate = std::make_unique<FakeDelegate>();

  fake_delegate->SetIsDeprovisioned(test_case.is_deprovisioned);
  SetHealthdMetricSamplerResult();

  auto info_queue = std::unique_ptr<MockReportQueue, base::OnTaskRunnerDeleter>(
      new ::testing::NiceMock<MockReportQueue>(),
      base::OnTaskRunnerDeleter(task_runner_));
  auto telemetry_queue =
      std::unique_ptr<MockReportQueue, base::OnTaskRunnerDeleter>(
          new ::testing::NiceMock<MockReportQueue>(),
          base::OnTaskRunnerDeleter(task_runner_));
  auto event_queue =
      std::unique_ptr<MockReportQueue, base::OnTaskRunnerDeleter>(
          new ::testing::NiceMock<MockReportQueue>(),
          base::OnTaskRunnerDeleter(task_runner_));
  auto* const info_queue_ptr = info_queue.get();

  fake_delegate->SetInfoQueue(std::move(info_queue));
  fake_delegate->SetTelemetryQueue(std::move(telemetry_queue));
  fake_delegate->SetEventQueue(std::move(event_queue));

  EXPECT_CALL(*info_queue_ptr, AddRecord).Times(test_case.expected_info_count);
  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(fake_delegate), nullptr);

  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(
    HealthdInfoReportingTests,
    HealthdInfoReportingTest,
    ::testing::ValuesIn<HealthdInfoReportingTestCase>({
        {"CpuReportingEnabled", /*is_deprovisioned=*/false,
         /*policy_path=*/::ash::kReportDeviceCpuInfo,
         /*policy_enabled=*/true,
         /*expected_info_count=*/1},
        {"CpuReportingDisabled", /*is_deprovisioned=*/false,
         /*policy_path=*/::ash::kReportDeviceCpuInfo,
         /*policy_enabled=*/false,
         /*expected_info_count=*/0},
        {"CpuReportingDeprovisioned", /*is_deprovisioned=*/true,
         /*policy_path=*/::ash::kReportDeviceCpuInfo,
         /*policy_enabled=*/true,
         /*expected_info_count=*/0},
        {"MemoryReportingEnabled", /*is_deprovisioned=*/false,
         /*policy_path=*/::ash::kReportDeviceMemoryInfo,
         /*policy_enabled=*/true,
         /*expected_info_count=*/1},
        {"MemoryReportingDisabled", /*is_deprovisioned=*/false,
         /*policy_path=*/::ash::kReportDeviceMemoryInfo,
         /*policy_enabled=*/false,
         /*expected_info_count=*/0},
        {"MemoryReportingDeprovisioned", /*is_deprovisioned=*/true,
         /*policy_path=*/::ash::kReportDeviceMemoryInfo,
         /*policy_enabled=*/true,
         /*expected_info_count=*/0},
        {"TbtReportingEnabled", /*is_deprovisioned=*/false,
         /*policy_path=*/::ash::kReportDeviceSecurityStatus,
         /*policy_enabled=*/true,
         /*expected_info_count=*/1},
        {"TbtReportingDisabled", /*is_deprovisioned=*/false,
         /*policy_path=*/::ash::kReportDeviceSecurityStatus,
         /*policy_enabled=*/false,
         /*expected_info_count=*/0},
        {"TbtReportingDeprovisioned", /*is_deprovisioned=*/true,
         /*policy_path=*/::ash::kReportDeviceSecurityStatus,
         /*policy_enabled=*/true,
         /*expected_info_count=*/0},
    }),
    [](const testing::TestParamInfo<HealthdInfoReportingTest::ParamType>&
           info) { return info.param.test_name; });

}  // namespace
}  // namespace reporting
