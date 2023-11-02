// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/reporting/metrics/configured_sampler.h"
#include "components/reporting/metrics/event_driven_telemetry_sampler_pool.h"
#include "components/reporting/metrics/fake_metric_report_queue.h"
#include "components/reporting/metrics/fake_reporting_settings.h"
#include "components/reporting/metrics/fake_sampler.h"
#include "components/reporting/metrics/metric_data_collector.h"
#include "components/reporting/metrics/metric_event_observer_manager.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using testing::_;
using testing::ByMove;
using testing::Eq;
using testing::Ne;
using testing::Return;
using testing::SizeIs;
using testing::StrEq;

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
                                   /*metric_report_queue=*/nullptr,
                                   reporting_settings,
                                   /*enable_setting_path=*/"",
                                   /*setting_enabled_default_value=*/false,
                                   /*sampler_pool=*/nullptr),
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

  MOCK_METHOD(bool, IsAffiliated, (Profile * profile), (const, override));

  MOCK_METHOD(bool, IsDeprovisioned, (), (const, override));

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
               EventDrivenTelemetrySamplerPool* sampler_pool,
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
               EventDrivenTelemetrySamplerPool* sampler_pool),
              (override));

  MOCK_METHOD(std::unique_ptr<Sampler>,
              GetHttpsLatencySampler,
              (),
              (const, override));

  MOCK_METHOD(std::unique_ptr<Sampler>,
              GetNetworkTelemetrySampler,
              (),
              (const, override));
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
// This is used for testing both the InputInfo and DisplayInfo, grouping them
// together since the collection is done using the same policy.
const MetricReportingSettingData graphics_info_settings = {
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
const MetricReportingSettingData displays_telemetry_settings = {
    ::ash::kReportDeviceGraphicsStatus, false, ::ash::kReportUploadFrequency,
    1};

struct MetricReportingManagerTestCase {
  std::string test_name;
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
  bool is_affiliated;
  MetricReportingSettingData setting_data;
  int expected_count_before_login;
  int expected_count_after_login;
};

class MetricReportingManagerTest
    : public ::testing::TestWithParam<MetricReportingManagerTestCase> {
 protected:
  void SetUp() override {
    auto info_queue = std::make_unique<test::FakeMetricReportQueue>();
    info_queue_ptr_ = info_queue.get();
    auto telemetry_queue = std::make_unique<test::FakeMetricReportQueue>();
    telemetry_queue_ptr_ = telemetry_queue.get();
    auto event_queue = std::make_unique<test::FakeMetricReportQueue>();
    event_queue_ptr_ = event_queue.get();
    auto user_telemetry_queue = std::make_unique<test::FakeMetricReportQueue>();
    user_telemetry_queue_ptr_ = user_telemetry_queue.get();
    auto peripheral_queue = std::make_unique<test::FakeMetricReportQueue>();
    peripheral_queue_ptr_ = peripheral_queue.get();

    mock_delegate_ = std::make_unique<::testing::NiceMock<MockDelegate>>();

    ON_CALL(*mock_delegate_, CreateMetricReportQueue(EventType::kDevice,
                                                     Destination::INFO_METRIC,
                                                     Priority::SLOW_BATCH))
        .WillByDefault(Return(ByMove(std::move(info_queue))));
    ON_CALL(*mock_delegate_, CreateMetricReportQueue(EventType::kDevice,
                                                     Destination::EVENT_METRIC,
                                                     Priority::SLOW_BATCH))
        .WillByDefault(Return(ByMove(std::move(event_queue))));
    ON_CALL(*mock_delegate_,
            CreatePeriodicUploadReportQueue(
                EventType::kDevice, Destination::TELEMETRY_METRIC,
                Priority::MANUAL_BATCH, _, ::ash::kReportUploadFrequency, _,
                /*rate_unit_to_ms=*/1))
        .WillByDefault(Return(ByMove(std::move(telemetry_queue))));
    ON_CALL(
        *mock_delegate_,
        CreateMetricReportQueue(EventType::kUser, Destination::TELEMETRY_METRIC,
                                Priority::MANUAL_BATCH))
        .WillByDefault(Return(ByMove(std::move(user_telemetry_queue))));
    ON_CALL(*mock_delegate_,
            CreateMetricReportQueue(EventType::kDevice,
                                    Destination::PERIPHERAL_EVENTS,
                                    Priority::SECURITY))
        .WillByDefault(Return(ByMove(std::move(peripheral_queue))));
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  test::FakeMetricReportQueue* info_queue_ptr_;
  test::FakeMetricReportQueue* telemetry_queue_ptr_;
  test::FakeMetricReportQueue* event_queue_ptr_;
  test::FakeMetricReportQueue* peripheral_queue_ptr_;
  test::FakeMetricReportQueue* user_telemetry_queue_ptr_;

  std::unique_ptr<::testing::NiceMock<MockDelegate>> mock_delegate_;
};

TEST_F(MetricReportingManagerTest, InitiallyDeprovisioned) {
  auto fake_reporting_settings =
      std::make_unique<test::FakeReportingSettings>();
  const auto init_delay = mock_delegate_->GetInitDelay();
  int one_shot_collector_count = 0;
  int periodic_collector_count = 0;
  int periodic_event_collector_count = 0;
  int observer_manager_count = 0;

  ON_CALL(*mock_delegate_, IsDeprovisioned).WillByDefault(Return(true));
  ON_CALL(*mock_delegate_, IsAffiliated).WillByDefault(Return(true));

  ON_CALL(*mock_delegate_, CreateOneShotCollector).WillByDefault([&]() {
    return std::make_unique<FakeCollector>(&one_shot_collector_count);
  });
  ON_CALL(*mock_delegate_, CreatePeriodicCollector).WillByDefault([&]() {
    return std::make_unique<FakeCollector>(&periodic_collector_count);
  });
  ON_CALL(*mock_delegate_, CreatePeriodicEventCollector).WillByDefault([&]() {
    return std::make_unique<FakeCollector>(&periodic_event_collector_count);
  });
  ON_CALL(*mock_delegate_, CreateEventObserverManager).WillByDefault([&]() {
    return std::make_unique<FakeMetricEventObserverManager>(
        fake_reporting_settings.get(), &observer_manager_count);
  });

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

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

  const auto init_delay = mock_delegate_->GetInitDelay();
  auto* const mock_delegate_ptr = mock_delegate_.get();
  int collector_count = 0;
  ON_CALL(*mock_delegate_ptr, IsAffiliated)
      .WillByDefault(Return(test_case.is_affiliated));
  ON_CALL(*mock_delegate_ptr,
          CreateOneShotCollector(
              _, info_queue_ptr_, _, test_case.setting_data.enable_setting_path,
              test_case.setting_data.setting_enabled_default_value))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

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
         {"GraphicsInfo",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, graphics_info_settings,
          /*expected_count_before_login=*/2,
          /*expected_count_after_login=*/2}}),
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
  auto* const mock_delegate_ptr = mock_delegate_.get();
  int observer_manager_count = 0;
  ON_CALL(*mock_delegate_ptr, IsAffiliated)
      .WillByDefault(Return(test_case.is_affiliated));
  ON_CALL(
      *mock_delegate_ptr,
      CreateEventObserverManager(
          _, event_queue_ptr_, _, test_case.setting_data.enable_setting_path,
          test_case.setting_data.setting_enabled_default_value, _))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

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
  auto* const mock_delegate_ptr = mock_delegate_.get();
  int observer_manager_count = 0;
  ON_CALL(*mock_delegate_ptr, IsAffiliated)
      .WillByDefault(Return(test_case.is_affiliated));
  ON_CALL(*mock_delegate_ptr,
          CreateEventObserverManager(
              _, peripheral_queue_ptr_, _,
              test_case.setting_data.enable_setting_path,
              test_case.setting_data.setting_enabled_default_value, _))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

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
  const auto init_delay = mock_delegate_->GetInitDelay();
  const auto upload_delay = mock_delegate_->GetInitialUploadDelay();
  auto* const mock_delegate_ptr = mock_delegate_.get();
  int collector_count = 0;

  ON_CALL(*mock_delegate_ptr,
          CreateOneShotCollector(_, telemetry_queue_ptr_, _,
                                 ::ash::kReportDeviceBootMode, true))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

  EXPECT_EQ(collector_count, 0);

  task_environment_.FastForwardBy(init_delay);

  EXPECT_EQ(collector_count, 1);

  task_environment_.FastForwardBy(upload_delay);

  EXPECT_EQ(telemetry_queue_ptr_->GetNumFlush(), 1);

  ON_CALL(*mock_delegate_ptr, IsDeprovisioned).WillByDefault(Return(true));
  metric_reporting_manager->DeviceSettingsUpdated();

  EXPECT_EQ(collector_count, 0);
}

TEST_P(MetricReportingManagerTelemetryTest, Default) {
  const MetricReportingManagerTestCase& test_case = GetParam();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(test_case.enabled_features,
                                       test_case.disabled_features);

  const auto init_delay = mock_delegate_->GetInitDelay();
  const auto upload_delay = mock_delegate_->GetInitialUploadDelay();
  auto* const mock_delegate_ptr = mock_delegate_.get();
  int collector_count = 0;
  ON_CALL(*mock_delegate_ptr, IsAffiliated)
      .WillByDefault(Return(test_case.is_affiliated));
  ON_CALL(*mock_delegate_ptr,
          CreatePeriodicCollector(
              _, telemetry_queue_ptr_, _,
              test_case.setting_data.enable_setting_path,
              test_case.setting_data.setting_enabled_default_value,
              test_case.setting_data.rate_setting_path, _,
              test_case.setting_data.rate_unit_to_ms))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });
  ON_CALL(*mock_delegate_ptr,
          CreatePeriodicCollector(
              _, user_telemetry_queue_ptr_, _,
              test_case.setting_data.enable_setting_path,
              test_case.setting_data.setting_enabled_default_value,
              test_case.setting_data.rate_setting_path, _,
              test_case.setting_data.rate_unit_to_ms))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

  task_environment_.FastForwardBy(init_delay);

  EXPECT_EQ(collector_count, test_case.expected_count_before_login);

  task_environment_.FastForwardBy(upload_delay);

  EXPECT_EQ(telemetry_queue_ptr_->GetNumFlush(), 1);

  metric_reporting_manager->OnLogin(nullptr);

  EXPECT_EQ(collector_count, test_case.expected_count_before_login);

  task_environment_.FastForwardBy(init_delay);

  EXPECT_EQ(collector_count, test_case.expected_count_after_login);

  const int expected_login_flush_count = test_case.is_affiliated ? 1 : 0;
  task_environment_.FastForwardBy(upload_delay);

  EXPECT_EQ(telemetry_queue_ptr_->GetNumFlush(),
            1 + expected_login_flush_count);

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
          // 3 collectors should be created after login, network telemetry,
          // https latency, and network bandwidth.
          /*expected_count_after_login=*/3},
         {"AudioTelemetry_Unaffiliated", /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, audio_metric_settings,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"AudioTelemetry_Default", /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, audio_metric_settings,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/1},
         {"DisplaysTelemetry_Unaffiliated", /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, displays_telemetry_settings,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"DisplaysTelemetry_Default", /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, displays_telemetry_settings,
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

  const auto init_delay = mock_delegate_->GetInitDelay();
  auto* const mock_delegate_ptr = mock_delegate_.get();
  int collector_count = 0;
  ON_CALL(*mock_delegate_ptr, IsAffiliated)
      .WillByDefault(Return(test_case.is_affiliated));
  ON_CALL(*mock_delegate_ptr,
          CreatePeriodicEventCollector(
              _, _, _, event_queue_ptr_, _,
              test_case.setting_data.enable_setting_path,
              test_case.setting_data.setting_enabled_default_value,
              test_case.setting_data.rate_setting_path, _,
              test_case.setting_data.rate_unit_to_ms))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

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

struct EventDrivenTelemetrySamplerPoolTestCase {
  std::string test_name;
  MetricEventType event_type;
  std::string setting_name;
};

class EventDrivenTelemetrySamplerPoolTest
    : public ::testing::TestWithParam<EventDrivenTelemetrySamplerPoolTestCase> {
 protected:
  void SetUp() override {
    auto https_latency_sampler = std::make_unique<test::FakeSampler>();
    https_latency_sampler_ptr_ = https_latency_sampler.get();
    auto network_telemetry_sampler = std::make_unique<test::FakeSampler>();
    network_telemetry_sampler_ptr_ = network_telemetry_sampler.get();

    mock_delegate_ = std::make_unique<::testing::NiceMock<MockDelegate>>();
    ON_CALL(*mock_delegate_, GetHttpsLatencySampler)
        .WillByDefault(Return(ByMove(std::move(https_latency_sampler))));
    ON_CALL(*mock_delegate_, GetNetworkTelemetrySampler)
        .WillByDefault(Return(ByMove(std::move(network_telemetry_sampler))));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  Sampler* https_latency_sampler_ptr_;
  Sampler* network_telemetry_sampler_ptr_;

  std::unique_ptr<::testing::NiceMock<MockDelegate>> mock_delegate_;
};

TEST_P(EventDrivenTelemetrySamplerPoolTest,
       SettingBasedTelemetry_AffiliatedOnly) {
  EventDrivenTelemetrySamplerPoolTestCase test_case = GetParam();

  ash::ScopedTestingCrosSettings cros_settings;
  base::Value::List telemetry_list;
  telemetry_list.Append("invalid");
  telemetry_list.Append("network_telemetry");
  telemetry_list.Append("https_latency");
  telemetry_list.Append("https_latency");  // duplicate.
  telemetry_list.Append("invalid");

  cros_settings.device_settings()->Set(test_case.setting_name,
                                       base::Value(std::move(telemetry_list)));

  ON_CALL(*mock_delegate_, IsDeprovisioned).WillByDefault(Return(false));
  ON_CALL(*mock_delegate_, IsAffiliated).WillByDefault(Return(true));

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

  std::vector<ConfiguredSampler*> event_telemetry =
      metric_reporting_manager->GetTelemetrySamplers(test_case.event_type);

  ASSERT_TRUE(event_telemetry.empty());

  metric_reporting_manager->OnLogin(nullptr);

  event_telemetry =
      metric_reporting_manager->GetTelemetrySamplers(test_case.event_type);
  ASSERT_THAT(event_telemetry, SizeIs(2));
  EXPECT_THAT(event_telemetry[0]->GetSampler(),
              Eq(network_telemetry_sampler_ptr_));
  EXPECT_THAT(event_telemetry[0]->GetEnableSettingPath(),
              StrEq(ash::kReportDeviceNetworkStatus));
  EXPECT_THAT(event_telemetry[1]->GetSampler(), Eq(https_latency_sampler_ptr_));
  EXPECT_THAT(event_telemetry[1]->GetEnableSettingPath(),
              StrEq(ash::kReportDeviceNetworkStatus));
}

INSTANTIATE_TEST_SUITE_P(
    EventDrivenTelemetrySamplerPoolTests,
    EventDrivenTelemetrySamplerPoolTest,
    ::testing::ValuesIn<EventDrivenTelemetrySamplerPoolTestCase>(
        {{"SignalStrengthLow", MetricEventType::NETWORK_SIGNAL_STRENGTH_LOW,
          ash::kReportDeviceSignalStrengthEventDrivenTelemetry},
         {"SignalStrengthRecovered",
          MetricEventType::NETWORK_SIGNAL_STRENGTH_RECOVERED,
          ash::kReportDeviceSignalStrengthEventDrivenTelemetry}}),
    [](const testing::TestParamInfo<
        EventDrivenTelemetrySamplerPoolTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace reporting
