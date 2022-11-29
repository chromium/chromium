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
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/event_driven_telemetry_collector_pool.h"
#include "components/reporting/metrics/fakes/fake_metric_event_observer.h"
#include "components/reporting/metrics/fakes/fake_metric_report_queue.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/metrics/fakes/fake_sampler.h"
#include "components/reporting/metrics/metric_event_observer_manager.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
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

class FakeMetricEventObserverManager : public MetricEventObserverManager {
 public:
  FakeMetricEventObserverManager(ReportingSettings* reporting_settings,
                                 int* observer_manager_count)
      : MetricEventObserverManager(
            std::make_unique<test::FakeMetricEventObserver>(),
            /*metric_report_queue=*/nullptr,
            reporting_settings,
            /*enable_setting_path=*/"",
            /*setting_enabled_default_value=*/false,
            /*collector_pool=*/nullptr),
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
  FakeCollector() : CollectorBase(/*sampler=*/nullptr) {}

  explicit FakeCollector(int* collector_count)
      : CollectorBase(/*sampler=*/nullptr), collector_count_(collector_count) {
    if (collector_count_) {
      ++(*collector_count_);
    }
  }

  FakeCollector(const FakeCollector& other) = delete;
  FakeCollector& operator=(const FakeCollector& other) = delete;

  ~FakeCollector() override {
    if (collector_count_) {
      --(*collector_count_);
    }
  }

 protected:
  // CollectorBase:
  void OnMetricDataCollected(bool is_event_driven,
                             absl::optional<MetricData> metric_data) override {}
  bool CanCollect() const override { return true; }

 private:
  raw_ptr<int> collector_count_ = nullptr;
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
               bool setting_enabled_default_value,
               base::TimeDelta init_delay),
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
               int rate_unit_to_ms,
               base::TimeDelta init_delay),
              (override));

  MOCK_METHOD(std::unique_ptr<MetricEventObserverManager>,
              CreateEventObserverManager,
              (std::unique_ptr<MetricEventObserver> event_observer,
               MetricReportQueue* metric_report_queue,
               ReportingSettings* reporting_settings,
               const std::string& enable_setting_path,
               bool setting_enabled_default_value,
               EventDrivenTelemetryCollectorPool* collector_pool,
               base::TimeDelta init_delay),
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
const MetricReportingSettingData app_event_settings = {
    ::ash::kReportDeviceAppInfo, false, "", 1};

struct MetricReportingManagerTestCase {
  std::string test_name;
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
  bool is_affiliated;
  MetricReportingSettingData setting_data;
  bool has_init_delay;
  int expected_count_before_login;
  int expected_count_after_login;
};

test::FakeMetricReportQueue* CreateMockMetricReportQueueHelper(
    ::testing::NiceMock<MockDelegate>* mock_delegate,
    EventType event_type,
    Destination destination,
    Priority priority) {
  auto metric_report_queue = std::make_unique<test::FakeMetricReportQueue>();
  auto* metric_report_queue_ptr = metric_report_queue.get();
  // Only one report queue should be created with the given args: `event_type`,
  // `destination`, and `priority`.
  ON_CALL(*mock_delegate,
          CreateMetricReportQueue(event_type, destination, priority))
      .WillByDefault(Return(ByMove(std::move(metric_report_queue))));
  return metric_report_queue_ptr;
}

class MetricReportingManagerTest
    : public ::testing::TestWithParam<MetricReportingManagerTestCase> {
 protected:
  void SetUp() override {
    mock_delegate_ = std::make_unique<::testing::NiceMock<MockDelegate>>();
    info_queue_ptr_ = CreateMockMetricReportQueueHelper(
        mock_delegate_.get(), EventType::kDevice, Destination::INFO_METRIC,
        Priority::SLOW_BATCH);
    event_queue_ptr_ = CreateMockMetricReportQueueHelper(
        mock_delegate_.get(), EventType::kDevice, Destination::EVENT_METRIC,
        Priority::SLOW_BATCH);
    user_telemetry_queue_ptr_ = CreateMockMetricReportQueueHelper(
        mock_delegate_.get(), EventType::kUser, Destination::TELEMETRY_METRIC,
        Priority::MANUAL_BATCH);
    peripheral_queue_ptr_ = CreateMockMetricReportQueueHelper(
        mock_delegate_.get(), EventType::kUser, Destination::PERIPHERAL_EVENTS,
        Priority::SECURITY);
    user_event_queue_ptr_ = CreateMockMetricReportQueueHelper(
        mock_delegate_.get(), EventType::kUser, Destination::EVENT_METRIC,
        Priority::SLOW_BATCH);

    auto telemetry_queue = std::make_unique<test::FakeMetricReportQueue>();
    telemetry_queue_ptr_ = telemetry_queue.get();
    // Only one periodic upload report queue should be created.
    ON_CALL(*mock_delegate_, CreatePeriodicUploadReportQueue)
        .WillByDefault(Return(ByMove(std::move(telemetry_queue))));
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  test::FakeMetricReportQueue* info_queue_ptr_;
  test::FakeMetricReportQueue* telemetry_queue_ptr_;
  test::FakeMetricReportQueue* event_queue_ptr_;
  test::FakeMetricReportQueue* peripheral_queue_ptr_;
  test::FakeMetricReportQueue* user_telemetry_queue_ptr_;
  test::FakeMetricReportQueue* user_event_queue_ptr_;

  std::unique_ptr<::testing::NiceMock<MockDelegate>> mock_delegate_;
};

TEST_F(MetricReportingManagerTest, InitiallyDeprovisioned) {
  auto fake_reporting_settings =
      std::make_unique<test::FakeReportingSettings>();
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
  ON_CALL(*mock_delegate_, CreateEventObserverManager).WillByDefault([&]() {
    return std::make_unique<FakeMetricEventObserverManager>(
        fake_reporting_settings.get(), &observer_manager_count);
  });

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

  EXPECT_EQ(one_shot_collector_count, 0);
  EXPECT_EQ(periodic_collector_count, 0);
  EXPECT_EQ(periodic_event_collector_count, 0);
  EXPECT_EQ(observer_manager_count, 0);

  metric_reporting_manager->OnLogin(nullptr);

  EXPECT_EQ(one_shot_collector_count, 0);
  EXPECT_EQ(periodic_collector_count, 0);
  EXPECT_EQ(periodic_event_collector_count, 0);
  EXPECT_EQ(observer_manager_count, 0);
}

class MetricReportingManagerInfoTest : public MetricReportingManagerTest {};

TEST_P(MetricReportingManagerInfoTest, Default) {
  const MetricReportingManagerTestCase& test_case = GetParam();
  const base::TimeDelta init_delay = test_case.has_init_delay
                                         ? metrics::InitDelayParam::Get()
                                         : base::TimeDelta();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(test_case.enabled_features,
                                       test_case.disabled_features);

  auto* const mock_delegate_ptr = mock_delegate_.get();
  int collector_count = 0;
  ON_CALL(*mock_delegate_ptr, IsAffiliated)
      .WillByDefault(Return(test_case.is_affiliated));
  ON_CALL(*mock_delegate_ptr,
          CreateOneShotCollector(
              _, info_queue_ptr_, _, test_case.setting_data.enable_setting_path,
              test_case.setting_data.setting_enabled_default_value, init_delay))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

  EXPECT_EQ(collector_count, test_case.expected_count_before_login);

  metric_reporting_manager->OnLogin(nullptr);

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
          /*has_init_delay=*/true,
          /*expected_count_before_login=*/1,
          /*expected_count_after_login=*/1},
         {"CpuInfo",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, cpu_info_settings, /*has_init_delay=*/true,
          /*expected_count_before_login=*/1,
          /*expected_count_after_login=*/1},
         {"MemoryInfo",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, memory_info_settings,
          /*has_init_delay=*/true,
          /*expected_count_before_login=*/1,
          /*expected_count_after_login=*/1},
         {"BusInfo",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, bus_info_settings, /*has_init_delay=*/true,
          /*expected_count_before_login=*/1,
          /*expected_count_after_login=*/1},
         {"GraphicsInfo",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, graphics_info_settings,
          /*has_init_delay=*/true,
          /*expected_count_before_login=*/2,
          /*expected_count_after_login=*/2}}),
    [](const testing::TestParamInfo<MetricReportingManagerInfoTest::ParamType>&
           info) { return info.param.test_name; });

class MetricReportingManagerEventTest : public MetricReportingManagerTest {};

TEST_P(MetricReportingManagerEventTest, Default) {
  const MetricReportingManagerTestCase& test_case = GetParam();
  const base::TimeDelta init_delay = test_case.has_init_delay
                                         ? metrics::InitDelayParam::Get()
                                         : base::TimeDelta();

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
          test_case.setting_data.setting_enabled_default_value, _, init_delay))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });
  ON_CALL(
      *mock_delegate_ptr,
      CreateEventObserverManager(
          _, user_event_queue_ptr_, _,
          test_case.setting_data.enable_setting_path,
          test_case.setting_data.setting_enabled_default_value, _, init_delay))
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
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"NetworkEvent_Default",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, network_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/1},
         {"HttpsLatencyEvent_Default",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, network_event_settings,
          /*has_init_delay=*/true,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/1},
         {"AudioEvent_Unaffiliated",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, audio_metric_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"AudioEvent_Default",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, audio_metric_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/1},
         {"AppEvents_Unaffiliated",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, app_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"AppEvents_Default",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, app_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"AppEvents_FeatureFlagEnabled",
          /*enabled_features=*/{kEnableAppMetricsReporting},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, app_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/1},
         {"AppEvents_FeatureFlagDisabled",
          /*enabled_features=*/{},
          /*disabled_features=*/{kEnableAppMetricsReporting},
          /*is_affiliated=*/true, app_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0}}),
    [](const testing::TestParamInfo<MetricReportingManagerInfoTest::ParamType>&
           info) { return info.param.test_name; });

class MetricReportingManagerPeripheralTest : public MetricReportingManagerTest {
};

// These tests cover both peripheral telemetry and events since they share a
// queue.
TEST_P(MetricReportingManagerPeripheralTest, Default) {
  const MetricReportingManagerTestCase& test_case = GetParam();
  const base::TimeDelta init_delay = test_case.has_init_delay
                                         ? metrics::InitDelayParam::Get()
                                         : base::TimeDelta();

  auto fake_reporting_settings =
      std::make_unique<test::FakeReportingSettings>();
  auto* const mock_delegate_ptr = mock_delegate_.get();
  int observer_manager_count = 0;
  ON_CALL(*mock_delegate_ptr, IsAffiliated)
      .WillByDefault(Return(test_case.is_affiliated));
  ON_CALL(
      *mock_delegate_ptr,
      CreateEventObserverManager(
          _, peripheral_queue_ptr_, _,
          test_case.setting_data.enable_setting_path,
          test_case.setting_data.setting_enabled_default_value, _, init_delay))
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
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"PeripheralEvent_Default",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, peripheral_metric_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/1}}),
    [](const testing::TestParamInfo<MetricReportingManagerInfoTest::ParamType>&
           info) { return info.param.test_name; });

class MetricReportingManagerTelemetryTest : public MetricReportingManagerTest {
};

TEST_F(MetricReportingManagerTelemetryTest, OneShotCollectorBootPerformance) {
  const auto upload_delay = mock_delegate_->GetInitialUploadDelay();
  auto* const mock_delegate_ptr = mock_delegate_.get();
  int collector_count = 0;

  ON_CALL(*mock_delegate_ptr,
          CreateOneShotCollector(_, telemetry_queue_ptr_, _,
                                 ::ash::kReportDeviceBootMode, true,
                                 metrics::InitDelayParam::Get()))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

  EXPECT_EQ(collector_count, 1);

  task_environment_.FastForwardBy(upload_delay +
                                  metrics::InitDelayParam::Get());

  EXPECT_EQ(telemetry_queue_ptr_->GetNumFlush(), 1);

  ON_CALL(*mock_delegate_ptr, IsDeprovisioned).WillByDefault(Return(true));
  metric_reporting_manager->DeviceSettingsUpdated();

  EXPECT_EQ(collector_count, 0);
}

TEST_P(MetricReportingManagerTelemetryTest, Default) {
  const MetricReportingManagerTestCase& test_case = GetParam();
  const base::TimeDelta init_delay = test_case.has_init_delay
                                         ? metrics::InitDelayParam::Get()
                                         : base::TimeDelta();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(test_case.enabled_features,
                                       test_case.disabled_features);

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
              test_case.setting_data.rate_unit_to_ms, init_delay))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });
  ON_CALL(*mock_delegate_ptr,
          CreatePeriodicCollector(
              _, user_telemetry_queue_ptr_, _,
              test_case.setting_data.enable_setting_path,
              test_case.setting_data.setting_enabled_default_value,
              test_case.setting_data.rate_setting_path, _,
              test_case.setting_data.rate_unit_to_ms, init_delay))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

  EXPECT_EQ(collector_count, test_case.expected_count_before_login);

  task_environment_.FastForwardBy(upload_delay +
                                  metrics::InitDelayParam::Get());

  EXPECT_EQ(telemetry_queue_ptr_->GetNumFlush(), 1);

  metric_reporting_manager->OnLogin(nullptr);

  EXPECT_EQ(collector_count, test_case.expected_count_after_login);

  const int expected_login_flush_count = test_case.is_affiliated ? 1 : 0;
  task_environment_.FastForwardBy(upload_delay +
                                  metrics::InitDelayParam::Get());

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
          /*has_init_delay=*/true,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"NetworkTelemetry_Default", /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, network_telemetry_settings,
          /*has_init_delay=*/true,
          /*expected_count_before_login=*/0,
          // 3 collectors should be created after login, network telemetry,
          // https latency, and network bandwidth.
          /*expected_count_after_login=*/3},
         {"AudioTelemetry_Unaffiliated", /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, audio_metric_settings,
          /*has_init_delay=*/true,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"AudioTelemetry_Default", /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, audio_metric_settings,
          /*has_init_delay=*/true,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/1},
         {"DisplaysTelemetry_Unaffiliated", /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, displays_telemetry_settings,
          /*has_init_delay=*/true,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"DisplaysTelemetry_Default", /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, displays_telemetry_settings,
          /*has_init_delay=*/true,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/1}}),
    [](const testing::TestParamInfo<
        MetricReportingManagerTelemetryTest::ParamType>& info) {
      return info.param.test_name;
    });

struct EventDrivenTelemetryCollectorPoolTestCase {
  std::string test_name;
  MetricEventType event_type;
  std::string setting_name;
};

class EventDrivenTelemetryCollectorPoolTest
    : public ::testing::TestWithParam<
          EventDrivenTelemetryCollectorPoolTestCase> {
 protected:
  void SetUp() override {
    auto https_latency_collector = std::make_unique<FakeCollector>();
    https_latency_collector_ptr_ = https_latency_collector.get();
    auto network_telemetry_collector = std::make_unique<FakeCollector>();
    network_telemetry_collector_ptr_ = network_telemetry_collector.get();

    auto https_latency_sampler = std::make_unique<test::FakeSampler>();
    auto* const https_latency_sampler_ptr = https_latency_sampler.get();
    auto network_telemetry_sampler = std::make_unique<test::FakeSampler>();
    auto* const network_telemetry_sampler_ptr = network_telemetry_sampler.get();

    mock_delegate_ = std::make_unique<::testing::NiceMock<MockDelegate>>();

    // Info queue.
    CreateMockMetricReportQueueHelper(mock_delegate_.get(), EventType::kDevice,
                                      Destination::INFO_METRIC,
                                      Priority::SLOW_BATCH);
    // Event queue.
    CreateMockMetricReportQueueHelper(mock_delegate_.get(), EventType::kDevice,
                                      Destination::EVENT_METRIC,
                                      Priority::SLOW_BATCH);
    // User telemetry queue.
    CreateMockMetricReportQueueHelper(mock_delegate_.get(), EventType::kUser,
                                      Destination::TELEMETRY_METRIC,
                                      Priority::MANUAL_BATCH);
    // Peripherals queue.
    CreateMockMetricReportQueueHelper(mock_delegate_.get(), EventType::kUser,
                                      Destination::PERIPHERAL_EVENTS,
                                      Priority::SECURITY);
    // User event queue.
    CreateMockMetricReportQueueHelper(mock_delegate_.get(), EventType::kUser,
                                      Destination::EVENT_METRIC,
                                      Priority::SLOW_BATCH);
    // Telemetry queue.
    ON_CALL(*mock_delegate_, CreatePeriodicUploadReportQueue)
        .WillByDefault(
            Return(ByMove(std::make_unique<test::FakeMetricReportQueue>())));

    ON_CALL(*mock_delegate_, GetHttpsLatencySampler)
        .WillByDefault(Return(ByMove(std::move(https_latency_sampler))));
    ON_CALL(*mock_delegate_, GetNetworkTelemetrySampler)
        .WillByDefault(Return(ByMove(std::move(network_telemetry_sampler))));
    ON_CALL(*mock_delegate_,
            CreatePeriodicCollector(network_telemetry_sampler_ptr, _, _, _, _,
                                    _, _, _, _))
        .WillByDefault(Return(ByMove(std::move(network_telemetry_collector))));
    ON_CALL(*mock_delegate_, CreatePeriodicCollector(https_latency_sampler_ptr,
                                                     _, _, _, _, _, _, _, _))
        .WillByDefault(Return(ByMove(std::move(https_latency_collector))));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;

  raw_ptr<CollectorBase> https_latency_collector_ptr_;
  raw_ptr<CollectorBase> network_telemetry_collector_ptr_;

  std::unique_ptr<::testing::NiceMock<MockDelegate>> mock_delegate_;
};

TEST_P(EventDrivenTelemetryCollectorPoolTest,
       SettingBasedTelemetry_AffiliatedOnly) {
  EventDrivenTelemetryCollectorPoolTestCase test_case = GetParam();

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

  std::vector<CollectorBase*> event_telemetry =
      metric_reporting_manager->GetTelemetryCollectors(test_case.event_type);

  ASSERT_TRUE(event_telemetry.empty());

  metric_reporting_manager->OnLogin(nullptr);

  event_telemetry =
      metric_reporting_manager->GetTelemetryCollectors(test_case.event_type);
  ASSERT_THAT(event_telemetry, SizeIs(2));
  EXPECT_THAT(event_telemetry[0], Eq(network_telemetry_collector_ptr_));
  EXPECT_THAT(event_telemetry[1], Eq(https_latency_collector_ptr_));
}

INSTANTIATE_TEST_SUITE_P(
    EventDrivenTelemetryCollectorPoolTests,
    EventDrivenTelemetryCollectorPoolTest,
    ::testing::ValuesIn<EventDrivenTelemetryCollectorPoolTestCase>(
        {{"SignalStrengthLow", MetricEventType::NETWORK_SIGNAL_STRENGTH_LOW,
          ash::kReportDeviceSignalStrengthEventDrivenTelemetry},
         {"SignalStrengthRecovered",
          MetricEventType::NETWORK_SIGNAL_STRENGTH_RECOVERED,
          ash::kReportDeviceSignalStrengthEventDrivenTelemetry}}),
    [](const testing::TestParamInfo<
        EventDrivenTelemetryCollectorPoolTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace reporting
