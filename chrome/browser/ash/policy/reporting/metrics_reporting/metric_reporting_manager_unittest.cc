// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/components/settings/cros_settings_names.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/reporting/metrics/fake_metric_report_queue.h"
#include "components/reporting/metrics/fake_reporting_settings.h"
#include "components/reporting/metrics/fake_sampler.h"
#include "components/reporting/metrics/metric_data_collector.h"
#include "components/reporting/metrics/metric_event_observer_manager.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/sampler.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::Return;

namespace reporting {
namespace {

class FakeMetricEventObserver : public MetricEventObserver {
 public:
  FakeMetricEventObserver() = default;

  FakeMetricEventObserver(const FakeMetricEventObserver& other) = delete;
  FakeMetricEventObserver& operator=(const FakeMetricEventObserver& other) =
      delete;

  ~FakeMetricEventObserver() override = default;

  void SetOnEventObservedCallback(MetricRepeatingCallback cb) override {}
  void SetReportingEnabled(bool is_enabled) override {}
};

class FakeMetricEventObserverManager : public MetricEventObserverManager {
 public:
  FakeMetricEventObserverManager(ReportingSettings* reporting_settings,
                                 int* observer_manager_count)
      : MetricEventObserverManager(std::make_unique<FakeMetricEventObserver>(),
                                   nullptr,
                                   reporting_settings,
                                   "",
                                   false,
                                   {}),
        observer_manager_count_(observer_manager_count) {
    ++(*observer_manager_count_);
  }

  FakeMetricEventObserverManager(const FakeMetricEventObserverManager& other) =
      delete;
  FakeMetricEventObserverManager& operator=(
      const FakeMetricEventObserverManager& other) = delete;

  ~FakeMetricEventObserverManager() override { --(*observer_manager_count_); }

 private:
  raw_ptr<int> observer_manager_count_;
};

class FakeCollector : public CollectorBase {
 public:
  explicit FakeCollector(int* collector_count)
      : CollectorBase(nullptr, nullptr), collector_count_(collector_count) {
    ++(*collector_count_);
  }

  FakeCollector(const FakeCollector& other) = delete;
  FakeCollector& operator=(const FakeCollector& other) = delete;

  ~FakeCollector() override { --(*collector_count_); }

 protected:
  void OnMetricDataCollected(absl::optional<MetricData>) override {}

 private:
  raw_ptr<int> collector_count_;
};

class MockDelegate : public MetricReportingManager::Delegate {
 public:
  MockDelegate() = default;

  MockDelegate(const MockDelegate& other) = delete;
  MockDelegate& operator=(const MockDelegate& other) = delete;

  ~MockDelegate() override = default;

  MOCK_METHOD(bool, IsAffiliated, (Profile * profile), (override));

  MOCK_METHOD(bool, IsDeprovisioned, (), (override));

  MOCK_METHOD(std::unique_ptr<MetricReportQueue>,
              CreateMetricReportQueue,
              (EventType event_type,
               Destination destination,
               Priority priority),
              (override));

  MOCK_METHOD(std::unique_ptr<MetricReportQueue>,
              CreatePeriodicUploadReportQueue,
              (EventType event_type,
               Destination destination,
               Priority priority,
               ReportingSettings* reporting_settings,
               const std::string& rate_setting_path,
               base::TimeDelta default_rate,
               int rate_unit_to_ms),
              (override));

  MOCK_METHOD(std::unique_ptr<CollectorBase>,
              CreateOneShotCollector,
              (Sampler * sampler,
               MetricReportQueue* metric_report_queue,
               ReportingSettings* reporting_settings,
               const std::string& enable_setting_path,
               bool setting_enabled_default_value),
              (override));

  MOCK_METHOD(std::unique_ptr<CollectorBase>,
              CreatePeriodicCollector,
              (Sampler * sampler,
               MetricReportQueue* metric_report_queue,
               ReportingSettings* reporting_settings,
               const std::string& enable_setting_path,
               bool setting_enabled_default_value,
               const std::string& rate_setting_path,
               base::TimeDelta default_rate,
               int rate_unit_to_ms),
              (override));

  MOCK_METHOD(std::unique_ptr<CollectorBase>,
              CreatePeriodicEventCollector,
              (Sampler * sampler,
               std::unique_ptr<EventDetector> event_detector,
               std::vector<Sampler*> additional_samplers,
               MetricReportQueue* metric_report_queue,
               ReportingSettings* reporting_settings,
               const std::string& enable_setting_path,
               bool setting_enabled_default_value,
               const std::string& rate_setting_path,
               base::TimeDelta default_rate,
               int rate_unit_to_ms),
              (override));

  MOCK_METHOD(std::unique_ptr<MetricEventObserverManager>,
              CreateEventObserverManager,
              (std::unique_ptr<MetricEventObserver> event_observer,
               MetricReportQueue* metric_report_queue,
               ReportingSettings* reporting_settings,
               const std::string& enable_setting_path,
               bool setting_enabled_default_value,
               std::vector<Sampler*> additional_samplers),
              (override));
};

struct MetricReportingSettingData {
  std::string enable_setting_path;
  bool setting_enabled_default_value;
  std::string rate_setting_path;
  int rate_unit_to_ms;
};

const MetricReportingSettingData network_info_settings = {
    ::ash::kReportDeviceNetworkConfiguration, true, "", 0};
const MetricReportingSettingData cpu_info_settings = {
    ::ash::kReportDeviceCpuInfo, false, "", 0};
const MetricReportingSettingData memory_info_settings = {
    ::ash::kReportDeviceMemoryInfo, false, "", 0};
const MetricReportingSettingData bus_info_settings = {
    ::ash::kReportDeviceSecurityStatus, false, "", 0};
const MetricReportingSettingData input_info_settings = {
    ::ash::kReportDeviceGraphicsStatus, false, "", 0};
const MetricReportingSettingData network_telemetry_settings = {
    ::ash::kReportDeviceNetworkStatus, true,
    ::ash::kReportDeviceNetworkTelemetryCollectionRateMs, 1};
const MetricReportingSettingData network_event_settings = {
    ::ash::kReportDeviceNetworkStatus, true,
    ::ash::kReportDeviceNetworkTelemetryEventCheckingRateMs, 1};
const MetricReportingSettingData audio_metric_settings = {
    ::ash::kReportDeviceAudioStatus, true,
    ::ash::kReportDeviceAudioStatusCheckingRateMs, 1};
const MetricReportingSettingData peripheral_metric_settings = {
    ::ash::kReportDevicePeripherals, false, "", 0};

struct MetricReportingManagerTestCase {
  std::string test_name;
  std::vector<base::Feature> enabled_features;
  std::vector<base::Feature> disabled_features;
  bool is_affiliated;
  MetricReportingSettingData setting_data;
  int expected_count_before_login;
  int expected_count_after_login;
};

class MetricReportingManagerTest
    : public ::testing::TestWithParam<MetricReportingManagerTestCase> {
 protected:
  void SetUp() override {
    info_queue_ = std::make_unique<test::FakeMetricReportQueue>();
    telemetry_queue_ = std::make_unique<test::FakeMetricReportQueue>();
    event_queue_ = std::make_unique<test::FakeMetricReportQueue>();
    peripheral_queue_ = std::make_unique<test::FakeMetricReportQueue>();
    user_telemetry_queue_ = std::make_unique<test::FakeMetricReportQueue>();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<test::FakeMetricReportQueue> info_queue_;
  std::unique_ptr<test::FakeMetricReportQueue> telemetry_queue_;
  std::unique_ptr<test::FakeMetricReportQueue> event_queue_;
  std::unique_ptr<test::FakeMetricReportQueue> peripheral_queue_;
  std::unique_ptr<test::FakeMetricReportQueue> user_telemetry_queue_;
};

TEST_F(MetricReportingManagerTest, InitiallyDeprovisioned) {
  auto fake_reporting_settings =
      std::make_unique<test::FakeReportingSettings>();
  auto mock_delegate = std::make_unique<::testing::NiceMock<MockDelegate>>();
  const auto init_delay = mock_delegate->GetInitDelay();
  int one_shot_collector_count = 0;
  int periodic_collector_count = 0;
  int periodic_event_collector_count = 0;
  int observer_manager_count = 0;

  ON_CALL(*mock_delegate, IsDeprovisioned).WillByDefault(Return(true));
  ON_CALL(*mock_delegate, IsAffiliated).WillByDefault(Return(true));
  ON_CALL(*mock_delegate,
          CreateMetricReportQueue(EventType::kDevice, Destination::INFO_METRIC,
                                  Priority::SLOW_BATCH))
      .WillByDefault(Return(ByMove(std::move(info_queue_))));
  ON_CALL(*mock_delegate,
          CreateMetricReportQueue(EventType::kDevice, Destination::EVENT_METRIC,
                                  Priority::SLOW_BATCH))
      .WillByDefault(Return(ByMove(std::move(event_queue_))));
  ON_CALL(*mock_delegate,
          CreatePeriodicUploadReportQueue(
              EventType::kDevice, Destination::TELEMETRY_METRIC,
              Priority::MANUAL_BATCH, _, ::ash::kReportUploadFrequency, _, 1))
      .WillByDefault(Return(ByMove(std::move(telemetry_queue_))));
  ON_CALL(*mock_delegate,
          CreatePeriodicUploadReportQueue(
              EventType::kUser, Destination::TELEMETRY_METRIC,
              Priority::MANUAL_BATCH, _, ::ash::kReportUploadFrequency, _, 1))
      .WillByDefault(Return(ByMove(std::move(user_telemetry_queue_))));

  ON_CALL(*mock_delegate, CreateOneShotCollector).WillByDefault([&]() {
    return std::make_unique<FakeCollector>(&one_shot_collector_count);
  });
  ON_CALL(*mock_delegate, CreatePeriodicCollector).WillByDefault([&]() {
    return std::make_unique<FakeCollector>(&periodic_collector_count);
  });
  ON_CALL(*mock_delegate, CreatePeriodicEventCollector).WillByDefault([&]() {
    return std::make_unique<FakeCollector>(&periodic_event_collector_count);
  });
  ON_CALL(*mock_delegate, CreateEventObserverManager).WillByDefault([&]() {
    return std::make_unique<FakeMetricEventObserverManager>(
        fake_reporting_settings.get(), &observer_manager_count);
  });

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate), nullptr);

  task_environment_.FastForwardBy(init_delay);

  EXPECT_EQ(one_shot_collector_count, 0);
  EXPECT_EQ(periodic_collector_count, 0);
  EXPECT_EQ(periodic_event_collector_count, 0);
  EXPECT_EQ(observer_manager_count, 0);

  metric_reporting_manager->OnLogin(nullptr);

  task_environment_.FastForwardBy(init_delay);

  EXPECT_EQ(one_shot_collector_count, 0);
  EXPECT_EQ(periodic_collector_count, 0);
  EXPECT_EQ(periodic_event_collector_count, 0);
  EXPECT_EQ(observer_manager_count, 0);
}

class MetricReportingManagerInfoTest : public MetricReportingManagerTest {};

TEST_P(MetricReportingManagerInfoTest, Default) {
  const MetricReportingManagerTestCase& test_case = GetParam();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(test_case.enabled_features,
                                       test_case.disabled_features);

  auto mock_delegate = std::make_unique<::testing::NiceMock<MockDelegate>>();
  const auto init_delay = mock_delegate->GetInitDelay();
  auto* const mock_delegate_ptr = mock_delegate.get();
  auto* const info_queue_ptr = info_queue_.get();
  int collector_count = 0;
  ON_CALL(*mock_delegate_ptr, IsAffiliated)
      .WillByDefault(Return(test_case.is_affiliated));
  ON_CALL(*mock_delegate_ptr,
          CreateMetricReportQueue(EventType::kDevice, Destination::INFO_METRIC,
                                  Priority::SLOW_BATCH))
      .WillByDefault(Return(ByMove(std::move(info_queue_))));
  ON_CALL(*mock_delegate_ptr,
          CreateOneShotCollector(
              _, info_queue_ptr, _, test_case.setting_data.enable_setting_path,
              test_case.setting_data.setting_enabled_default_value))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate), nullptr);

  EXPECT_EQ(collector_count, 0);

  task_environment_.FastForwardBy(init_delay);

  EXPECT_EQ(collector_count, test_case.expected_count_before_login);

  metric_reporting_manager->OnLogin(nullptr);

  task_environment_.FastForwardBy(init_delay);

  EXPECT_EQ(collector_count, test_case.expected_count_after_login);

  ON_CALL(*mock_delegate_ptr, IsDeprovisioned).WillByDefault(Return(true));
  metric_reporting_manager->DeviceSettingsUpdated();

  EXPECT_EQ(collector_count, 0);
}

INSTANTIATE_TEST_SUITE_P(
    MetricReportingManagerInfoTests,
    MetricReportingManagerInfoTest,
    ::testing::ValuesIn<MetricReportingManagerTestCase>(
        {{"NetworkInfo",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, network_info_settings,
          /*expected_count_before_login=*/1,
          /*expected_count_after_login=*/1},
         {"CpuInfo",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, cpu_info_settings,
          /*expected_count_before_login=*/1,
          /*expected_count_after_login=*/1},
         {"MemoryInfo",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, memory_info_settings,
          /*expected_count_before_login=*/1,
          /*expected_count_after_login=*/1},
         {"BusInfo",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, bus_info_settings,
          /*expected_count_before_login=*/1,
          /*expected_count_after_login=*/1},
         {"InputInfo",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, input_info_settings,
          /*expected_count_before_login=*/1,
          /*expected_count_after_login=*/1}}),
    [](const testing::TestParamInfo<MetricReportingManagerInfoTest::ParamType>&
           info) { return info.param.test_name; });

class MetricReportingManagerEventTest : public MetricReportingManagerTest {};

TEST_P(MetricReportingManagerEventTest, Default) {
  const MetricReportingManagerTestCase& test_case = GetParam();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(test_case.enabled_features,
                                       test_case.disabled_features);

  auto fake_reporting_settings =
      std::make_unique<test::FakeReportingSettings>();
  auto mock_delegate = std::make_unique<::testing::NiceMock<MockDelegate>>();
  auto* const mock_delegate_ptr = mock_delegate.get();
  auto* const event_queue_ptr = event_queue_.get();
  int observer_manager_count = 0;
  ON_CALL(*mock_delegate_ptr, IsAffiliated)
      .WillByDefault(Return(test_case.is_affiliated));
  ON_CALL(*mock_delegate_ptr,
          CreateMetricReportQueue(EventType::kDevice, Destination::EVENT_METRIC,
                                  Priority::SLOW_BATCH))
      .WillByDefault(Return(ByMove(std::move(event_queue_))));
  ON_CALL(*mock_delegate_ptr,
          CreateEventObserverManager(
              _, event_queue_ptr, _, test_case.setting_data.enable_setting_path,
              test_case.setting_data.setting_enabled_default_value, _))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate), nullptr);

  EXPECT_EQ(observer_manager_count, test_case.expected_count_before_login);

  metric_reporting_manager->OnLogin(nullptr);

  EXPECT_EQ(observer_manager_count, test_case.expected_count_after_login);

  ON_CALL(*mock_delegate_ptr, IsDeprovisioned).WillByDefault(Return(true));
  metric_reporting_manager->DeviceSettingsUpdated();

  EXPECT_EQ(observer_manager_count, 0);
}

INSTANTIATE_TEST_SUITE_P(
    MetricReportingManagerEventTests,
    MetricReportingManagerEventTest,
    ::testing::ValuesIn<MetricReportingManagerTestCase>(
        {{"NetworkEvent_Unaffiliated",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, network_event_settings,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"NetworkEvent_Default",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, network_event_settings,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/1},
         {"AudioEvent_Unaffiliated",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, audio_metric_settings,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"AudioEvent_Default",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, audio_metric_settings,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/1}}),
    [](const testing::TestParamInfo<MetricReportingManagerInfoTest::ParamType>&
           info) { return info.param.test_name; });

class MetricReportingManagerPeripheralTest : public MetricReportingManagerTest {
};

// These tests cover both peripheral telemetry and events since they share a
// queue.
TEST_P(MetricReportingManagerPeripheralTest, Default) {
  const MetricReportingManagerTestCase& test_case = GetParam();

  auto fake_reporting_settings =
      std::make_unique<test::FakeReportingSettings>();
  auto mock_delegate = std::make_unique<::testing::NiceMock<MockDelegate>>();
  auto* const mock_delegate_ptr = mock_delegate.get();
  auto* const peripheral_queue_ptr = peripheral_queue_.get();
  int observer_manager_count = 0;
  ON_CALL(*mock_delegate_ptr, IsAffiliated)
      .WillByDefault(Return(test_case.is_affiliated));
  ON_CALL(*mock_delegate_ptr,
          CreateMetricReportQueue(EventType::kDevice,
                                  Destination::PERIPHERAL_EVENTS,
                                  Priority::SECURITY))
      .WillByDefault(Return(ByMove(std::move(peripheral_queue_))));
  ON_CALL(*mock_delegate_ptr,
          CreateEventObserverManager(
              _, peripheral_queue_ptr, _,
              test_case.setting_data.enable_setting_path,
              test_case.setting_data.setting_enabled_default_value, _))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate), nullptr);

  EXPECT_EQ(observer_manager_count, test_case.expected_count_before_login);

  metric_reporting_manager->OnLogin(nullptr);

  EXPECT_EQ(observer_manager_count, test_case.expected_count_after_login);

  ON_CALL(*mock_delegate_ptr, IsDeprovisioned).WillByDefault(Return(true));
  metric_reporting_manager->DeviceSettingsUpdated();

  EXPECT_EQ(observer_manager_count, 0);
}

INSTANTIATE_TEST_SUITE_P(
    MetricReportingManagerPeripheralTests,
    MetricReportingManagerPeripheralTest,
    ::testing::ValuesIn<MetricReportingManagerTestCase>(
        {{"PeripheralEvent_Unaffiliated",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, peripheral_metric_settings,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"PeripheralEvent_Default",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, peripheral_metric_settings,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/1}}),
    [](const testing::TestParamInfo<MetricReportingManagerInfoTest::ParamType>&
           info) { return info.param.test_name; });

class MetricReportingManagerTelemetryTest : public MetricReportingManagerTest {
};

TEST_F(MetricReportingManagerTelemetryTest, OneShotCollectorBootPerformance) {
  auto mock_delegate = std::make_unique<::testing::NiceMock<MockDelegate>>();
  const auto init_delay = mock_delegate->GetInitDelay();
  const auto upload_delay = mock_delegate->GetInitialUploadDelay();
  auto* const mock_delegate_ptr = mock_delegate.get();
  auto* const telemetry_queue_ptr = telemetry_queue_.get();
  int collector_count = 0;

  ON_CALL(*mock_delegate_ptr,
          CreatePeriodicUploadReportQueue(
              EventType::kDevice, Destination::TELEMETRY_METRIC,
              Priority::MANUAL_BATCH, _, ::ash::kReportUploadFrequency, _, 1))
      .WillByDefault(Return(ByMove(std::move(telemetry_queue_))));
  ON_CALL(*mock_delegate_ptr,
          CreateOneShotCollector(_, telemetry_queue_ptr, _,
                                 ::ash::kReportDeviceBootMode, true))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate), nullptr);

  EXPECT_EQ(collector_count, 0);

  task_environment_.FastForwardBy(init_delay);

  EXPECT_EQ(collector_count, 1);

  task_environment_.FastForwardBy(upload_delay);

  EXPECT_EQ(telemetry_queue_ptr->GetNumFlush(), 1);

  ON_CALL(*mock_delegate_ptr, IsDeprovisioned).WillByDefault(Return(true));
  metric_reporting_manager->DeviceSettingsUpdated();

  EXPECT_EQ(collector_count, 0);
}

TEST_P(MetricReportingManagerTelemetryTest, Default) {
  const MetricReportingManagerTestCase& test_case = GetParam();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(test_case.enabled_features,
                                       test_case.disabled_features);

  auto mock_delegate = std::make_unique<::testing::NiceMock<MockDelegate>>();
  const auto init_delay = mock_delegate->GetInitDelay();
  const auto upload_delay = mock_delegate->GetInitialUploadDelay();
  auto* const mock_delegate_ptr = mock_delegate.get();
  auto* const telemetry_queue_ptr = telemetry_queue_.get();
  auto* const user_telemetry_queue_ptr = user_telemetry_queue_.get();
  int collector_count = 0;
  ON_CALL(*mock_delegate_ptr, IsAffiliated)
      .WillByDefault(Return(test_case.is_affiliated));
  ON_CALL(*mock_delegate_ptr,
          CreatePeriodicUploadReportQueue(
              EventType::kDevice, Destination::TELEMETRY_METRIC,
              Priority::MANUAL_BATCH, _, ::ash::kReportUploadFrequency, _, 1))
      .WillByDefault(Return(ByMove(std::move(telemetry_queue_))));
  ON_CALL(*mock_delegate_ptr,
          CreatePeriodicUploadReportQueue(
              EventType::kUser, Destination::TELEMETRY_METRIC,
              Priority::MANUAL_BATCH, _, ::ash::kReportUploadFrequency, _, 1))
      .WillByDefault(Return(ByMove(std::move(user_telemetry_queue_))));
  ON_CALL(
      *mock_delegate_ptr,
      CreatePeriodicCollector(
          _, telemetry_queue_ptr, _, test_case.setting_data.enable_setting_path,
          test_case.setting_data.setting_enabled_default_value,
          test_case.setting_data.rate_setting_path, _,
          test_case.setting_data.rate_unit_to_ms))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });
  ON_CALL(*mock_delegate_ptr,
          CreatePeriodicCollector(
              _, user_telemetry_queue_ptr, _,
              test_case.setting_data.enable_setting_path,
              test_case.setting_data.setting_enabled_default_value,
              test_case.setting_data.rate_setting_path, _,
              test_case.setting_data.rate_unit_to_ms))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate), nullptr);

  task_environment_.FastForwardBy(init_delay);

  EXPECT_EQ(collector_count, test_case.expected_count_before_login);

  task_environment_.FastForwardBy(upload_delay);

  EXPECT_EQ(telemetry_queue_ptr->GetNumFlush(), 1);

  metric_reporting_manager->OnLogin(nullptr);

  EXPECT_EQ(collector_count, test_case.expected_count_before_login);

  task_environment_.FastForwardBy(init_delay);

  EXPECT_EQ(collector_count, test_case.expected_count_after_login);

  const int expected_login_flush_count = test_case.is_affiliated ? 1 : 0;
  task_environment_.FastForwardBy(upload_delay);

  EXPECT_EQ(telemetry_queue_ptr->GetNumFlush(), 1 + expected_login_flush_count);

  ON_CALL(*mock_delegate_ptr, IsDeprovisioned).WillByDefault(Return(true));
  metric_reporting_manager->DeviceSettingsUpdated();

  EXPECT_EQ(collector_count, 0);
}

INSTANTIATE_TEST_SUITE_P(
    MetricReportingManagerTelemetryTests,
    MetricReportingManagerTelemetryTest,
    ::testing::ValuesIn<MetricReportingManagerTestCase>(
        {{"NetworkTelemetry_Unaffiliated", /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, network_telemetry_settings,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"NetworkTelemetry_Default", /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, network_telemetry_settings,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/2},
         {"AudioTelemetry_Unaffiliated", /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, audio_metric_settings,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"AudioTelemetry_Default", /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, audio_metric_settings,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/1}}),
    [](const testing::TestParamInfo<
        MetricReportingManagerTelemetryTest::ParamType>& info) {
      return info.param.test_name;
    });

class MetricReportingManagerPeriodicEventTest
    : public MetricReportingManagerTest {};

TEST_P(MetricReportingManagerPeriodicEventTest, Default) {
  const MetricReportingManagerTestCase& test_case = GetParam();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(test_case.enabled_features,
                                       test_case.disabled_features);

  auto mock_delegate = std::make_unique<::testing::NiceMock<MockDelegate>>();
  const auto init_delay = mock_delegate->GetInitDelay();
  auto* const mock_delegate_ptr = mock_delegate.get();
  auto* const event_queue_ptr = event_queue_.get();
  int collector_count = 0;
  ON_CALL(*mock_delegate_ptr, IsAffiliated)
      .WillByDefault(Return(test_case.is_affiliated));
  ON_CALL(*mock_delegate_ptr,
          CreateMetricReportQueue(EventType::kDevice, Destination::EVENT_METRIC,
                                  Priority::SLOW_BATCH))
      .WillByDefault(Return(ByMove(std::move(event_queue_))));
  ON_CALL(*mock_delegate_ptr,
          CreatePeriodicEventCollector(
              _, _, _, event_queue_ptr, _,
              test_case.setting_data.enable_setting_path,
              test_case.setting_data.setting_enabled_default_value,
              test_case.setting_data.rate_setting_path, _,
              test_case.setting_data.rate_unit_to_ms))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate), nullptr);

  task_environment_.FastForwardBy(init_delay);

  EXPECT_EQ(collector_count, test_case.expected_count_before_login);

  metric_reporting_manager->OnLogin(nullptr);

  EXPECT_EQ(collector_count, test_case.expected_count_before_login);

  task_environment_.FastForwardBy(init_delay);

  EXPECT_EQ(collector_count, test_case.expected_count_after_login);

  ON_CALL(*mock_delegate_ptr, IsDeprovisioned).WillByDefault(Return(true));
  metric_reporting_manager->DeviceSettingsUpdated();

  EXPECT_EQ(collector_count, 0);
}

INSTANTIATE_TEST_SUITE_P(
    MetricReportingManagerPeriodicEventTests,
    MetricReportingManagerPeriodicEventTest,
    ::testing::ValuesIn<MetricReportingManagerTestCase>(
        {{"NetworkPeriodicEvent_Unaffiliated",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, network_event_settings,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"NetworkPeriodicEvent_Default",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, network_event_settings,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/1}}),
    [](const testing::TestParamInfo<
        MetricReportingManagerPeriodicEventTest::ParamType>& info) {
      return info.param.test_name;
    });
}  // namespace
}  // namespace reporting
