// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service_test_base.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_prefs.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/kiosk/vision/pref_names.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/event_driven_telemetry_collector_pool.h"
#include "components/reporting/metrics/fakes/fake_metric_event_observer.h"
#include "components/reporting/metrics/fakes/fake_metric_report_queue.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/metrics/fakes/fake_sampler.h"
#include "components/reporting/metrics/metric_event_observer.h"
#include "components/reporting/metrics/metric_event_observer_manager.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/rate_limiter_interface.h"
#include "components/reporting/util/rate_limiter_slide_window.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::AnyNumber;
using testing::ByMove;
using testing::Eq;
using testing::IsNull;
using testing::Ne;
using testing::Not;
using testing::NotNull;
using testing::Pointer;
using testing::Return;
using testing::SizeIs;
using testing::StrEq;
using testing::WithArg;

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

  // For event observers that require lifetime management.
  FakeMetricEventObserverManager(
      ReportingSettings* reporting_settings,
      int* observer_manager_count,
      std::unique_ptr<MetricEventObserver> event_observer)
      : MetricEventObserverManager(std::move(event_observer),
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
                             std::optional<MetricData> metric_data) override {}
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

  // Wraps around the `CreateMetricReportQueueMock` to simplify mocking
  // `MetricReportQueue` creation for a specific rate limiter.
  std::unique_ptr<MetricReportQueue> CreateMetricReportQueue(
      EventType event_type,
      Destination destination,
      Priority priority,
      std::unique_ptr<RateLimiterInterface> rate_limiter,
      std::optional<SourceInfo> source_info) override {
    return CreateMetricReportQueueMock(event_type, destination, priority,
                                       rate_limiter.get(), source_info);
  }

  MOCK_METHOD(bool, IsUserAffiliated, (Profile & profile), (const, override));

  MOCK_METHOD(bool, IsDeprovisioned, (), (const, override));

  MOCK_METHOD(std::unique_ptr<MetricReportQueue>,
              CreateMetricReportQueueMock,
              (EventType event_type,
               Destination destination,
               Priority priority,
               RateLimiterInterface* rate_limiter,
               std::optional<SourceInfo> source_info),
              ());

  MOCK_METHOD(std::unique_ptr<MetricReportQueue>,
              CreatePeriodicUploadReportQueue,
              (EventType event_type,
               Destination destination,
               Priority priority,
               ReportingSettings* reporting_settings,
               const std::string& rate_setting_path,
               base::TimeDelta default_rate,
               int rate_unit_to_ms,
               std::optional<SourceInfo> source_info),
              (override));

  MOCK_METHOD(std::unique_ptr<CollectorBase>,
              CreateManualCollector,
              (Sampler * sampler,
               MetricReportQueue* metric_report_queue,
               ReportingSettings* reporting_settings,
               const std::string& enable_setting_path,
               bool setting_enabled_default_value),
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

  MOCK_METHOD(std::unique_ptr<RateLimiterSlideWindow>,
              CreateSlidingWindowRateLimiter,
              (size_t total_size,
               base::TimeDelta time_window,
               size_t bucket_count),
              (override));

  MOCK_METHOD(std::unique_ptr<Sampler>,
              GetHttpsLatencySampler,
              (),
              (const, override));

  MOCK_METHOD(std::unique_ptr<Sampler>,
              GetNetworkTelemetrySampler,
              (),
              (const, override));

  MOCK_METHOD(bool,
              IsAppServiceAvailableForProfile,
              (Profile * profile),
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
    ::ash::kDeviceReportNetworkEvents, false, "", 0};
const MetricReportingSettingData https_latency_event_settings = {
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
    ::ash::reporting::kReportAppInventory, false, "", 1};
const MetricReportingSettingData device_activity_telemetry_settings = {
    ::ash::kDeviceActivityHeartbeatEnabled, false,
    ::ash::kDeviceActivityHeartbeatCollectionRateMs, 1};
const MetricReportingSettingData runtime_counters_telemetry_settings = {
    ::ash::kDeviceReportRuntimeCounters, false,
    ::ash::kDeviceReportRuntimeCountersCheckingRateMs, 1};
const MetricReportingSettingData website_event_settings = {
    kReportWebsiteActivityAllowlist, false, "", 1};
const MetricReportingSettingData fatal_crash_event_settings = {
    ::ash::kReportDeviceCrashReportInfo, false, "", 1};
const MetricReportingSettingData chrome_fatal_crash_event_settings = {
    ::ash::kReportDeviceCrashReportInfo, false, "", 1};

struct MetricReportingManagerTestCase {
  std::string test_name;
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;
  // Is the user affiliated.
  bool is_affiliated;
  MetricReportingSettingData setting_data;
  bool has_init_delay;
  // Count of initialized components before login.
  int expected_count_before_login;
  // Count of initialized components after login. This count is cumulative,
  // which means that it also includes the count before login.
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
  ON_CALL(*mock_delegate, CreateMetricReportQueueMock(event_type, destination,
                                                      priority, IsNull(), _))
      .WillByDefault(Return(ByMove(std::move(metric_report_queue))));
  return metric_report_queue_ptr;
}

test::FakeMetricReportQueue* CreateMockRateLimitedMetricReportQueueHelper(
    ::testing::NiceMock<MockDelegate>* mock_delegate,
    EventType event_type,
    Destination destination,
    Priority priority,
    size_t total_size,
    base::TimeDelta time_window,
    size_t bucket_count) {
  auto rate_limiter_slide_window = std::make_unique<RateLimiterSlideWindow>(
      total_size, time_window, bucket_count);
  auto* const rate_limiter_slide_window_ptr = rate_limiter_slide_window.get();
  ON_CALL(*mock_delegate,
          CreateSlidingWindowRateLimiter(total_size, time_window, bucket_count))
      .WillByDefault(Return(ByMove(std::move(rate_limiter_slide_window))));

  auto metric_report_queue = std::make_unique<test::FakeMetricReportQueue>();
  auto* metric_report_queue_ptr = metric_report_queue.get();
  ON_CALL(*mock_delegate,
          CreateMetricReportQueueMock(event_type, destination, priority,
                                      rate_limiter_slide_window_ptr, _))
      .WillByDefault(Return(ByMove(std::move(metric_report_queue))));
  return metric_report_queue_ptr;
}

// Base class used to test scenarios for the `MetricReportingManager`. We extend
// `AppPlatformMetricsServiceTestBase` for relevant setup of the
// `AppPlatformMetrics` component.
class MetricReportingManagerTest
    : public ::apps::AppPlatformMetricsServiceTestBase,
      public ::testing::WithParamInterface<MetricReportingManagerTestCase> {
 protected:
  void SetUp() override {
    // Set up `AppServiceProxy` with the stubbed `AppPlatformMetricsService`.
    // Needed to ensure downstream components have the necessary setup for
    // initialization.
    ::apps::AppPlatformMetricsServiceTestBase::SetUp();
    ::apps::AppServiceProxyFactory::GetForProfile(profile())
        ->SetAppPlatformMetricsServiceForTesting(
            GetAppPlatformMetricsService());

    // Initialize fake session manager client. Needed for setting up downstream
    // app metric reporting components.
    ::ash::SessionManagerClient::InitializeFakeInMemory();
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
    app_event_queue_ptr_ = CreateMockRateLimitedMetricReportQueueHelper(
        mock_delegate_.get(), EventType::kUser, Destination::EVENT_METRIC,
        Priority::SLOW_BATCH, metrics::kAppEventsTotalSize,
        metrics::kAppEventsWindow, metrics::kAppEventsBucketCount);
    website_event_queue_ptr_ = CreateMockRateLimitedMetricReportQueueHelper(
        mock_delegate_.get(), EventType::kUser, Destination::EVENT_METRIC,
        Priority::SLOW_BATCH, metrics::kWebsiteEventsTotalSize,
        metrics::kWebsiteEventsWindow, metrics::kWebsiteEventsBucketCount);
    crash_event_queue_ptr_ = CreateMockMetricReportQueueHelper(
        mock_delegate_.get(), EventType::kDevice, Destination::CRASH_EVENTS,
        Priority::FAST_BATCH);
    chrome_crash_event_queue_ptr_ = CreateMockMetricReportQueueHelper(
        mock_delegate_.get(), EventType::kDevice,
        Destination::CHROME_CRASH_EVENTS, Priority::FAST_BATCH);

    auto telemetry_queue = std::make_unique<test::FakeMetricReportQueue>();
    telemetry_queue_ptr_ = telemetry_queue.get();
    // Only one periodic upload report queue should be created for .
    ON_CALL(*mock_delegate_,
            CreatePeriodicUploadReportQueue(EventType::kDevice,
                                            Destination::TELEMETRY_METRIC, _, _,
                                            _, _, _, _))
        .WillByDefault(Return(ByMove(std::move(telemetry_queue))));

    auto heartbeat_queue = std::make_unique<test::FakeMetricReportQueue>();
    heartbeat_queue_ptr_ = heartbeat_queue.get();
    // Only one periodic upload report queue should be created for
    // KIOSK_HEARTBEAT_EVENTS.
    ON_CALL(*mock_delegate_,
            CreatePeriodicUploadReportQueue(
                _, Destination::KIOSK_HEARTBEAT_EVENTS, _, _, _, _, _, _))
        .WillByDefault(Return(ByMove(std::move(heartbeat_queue))));
  }

  ::ash::SessionTerminationManager session_termination_manager_;
  raw_ptr<test::FakeMetricReportQueue, DanglingUntriaged> info_queue_ptr_;
  raw_ptr<test::FakeMetricReportQueue, DanglingUntriaged> telemetry_queue_ptr_;
  raw_ptr<test::FakeMetricReportQueue, DanglingUntriaged> event_queue_ptr_;
  raw_ptr<test::FakeMetricReportQueue, DanglingUntriaged> peripheral_queue_ptr_;
  raw_ptr<test::FakeMetricReportQueue, DanglingUntriaged>
      user_telemetry_queue_ptr_;
  raw_ptr<test::FakeMetricReportQueue, DanglingUntriaged> user_event_queue_ptr_;
  raw_ptr<test::FakeMetricReportQueue, DanglingUntriaged> app_event_queue_ptr_;
  raw_ptr<test::FakeMetricReportQueue, DanglingUntriaged>
      website_event_queue_ptr_;
  raw_ptr<test::FakeMetricReportQueue, DanglingUntriaged> heartbeat_queue_ptr_;
  raw_ptr<test::FakeMetricReportQueue, DanglingUntriaged>
      crash_event_queue_ptr_;
  raw_ptr<test::FakeMetricReportQueue, DanglingUntriaged>
      chrome_crash_event_queue_ptr_;

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
  ON_CALL(*mock_delegate_, IsUserAffiliated).WillByDefault(Return(true));
  ON_CALL(*mock_delegate_, IsAppServiceAvailableForProfile)
      .WillByDefault(Return(true));

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

  metric_reporting_manager->OnLogin(profile());

  EXPECT_EQ(one_shot_collector_count, 0);
  EXPECT_EQ(periodic_collector_count, 0);
  EXPECT_EQ(periodic_event_collector_count, 0);
  EXPECT_EQ(observer_manager_count, 0);
}

class MetricReportingManagerInfoTest : public MetricReportingManagerTest {};

TEST_P(MetricReportingManagerInfoTest, Default) {
  const MetricReportingManagerTestCase& test_case = GetParam();
  const base::TimeDelta init_delay = test_case.has_init_delay
                                         ? metrics::kInitialCollectionDelay
                                         : base::TimeDelta();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(test_case.enabled_features,
                                       test_case.disabled_features);

  auto* const mock_delegate_ptr = mock_delegate_.get();
  int collector_count = 0;
  ON_CALL(*mock_delegate_ptr, IsUserAffiliated)
      .WillByDefault(Return(test_case.is_affiliated));
  ON_CALL(*mock_delegate_ptr,
          CreateOneShotCollector(
              _, info_queue_ptr_.get(), _,
              test_case.setting_data.enable_setting_path,
              test_case.setting_data.setting_enabled_default_value, init_delay))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });

  // Mock app service unavailability to eliminate noise.
  ON_CALL(*mock_delegate_ptr, IsAppServiceAvailableForProfile)
      .WillByDefault(Return(false));

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

  EXPECT_EQ(collector_count, test_case.expected_count_before_login);

  metric_reporting_manager->OnLogin(profile());

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
                                         ? metrics::kInitialCollectionDelay
                                         : base::TimeDelta();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(test_case.enabled_features,
                                       test_case.disabled_features);

  auto fake_reporting_settings =
      std::make_unique<test::FakeReportingSettings>();
  auto* const mock_delegate_ptr = mock_delegate_.get();
  int observer_manager_count = 0;
  ON_CALL(*mock_delegate_ptr, IsUserAffiliated)
      .WillByDefault(Return(test_case.is_affiliated));
  ON_CALL(*mock_delegate_ptr, IsAppServiceAvailableForProfile)
      .WillByDefault(Return(true));
  ON_CALL(
      *mock_delegate_ptr,
      CreateEventObserverManager(
          _, event_queue_ptr_.get(), _,
          test_case.setting_data.enable_setting_path,
          test_case.setting_data.setting_enabled_default_value, _, init_delay))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });
  ON_CALL(
      *mock_delegate_ptr,
      CreateEventObserverManager(
          _, user_event_queue_ptr_.get(), _,
          test_case.setting_data.enable_setting_path,
          test_case.setting_data.setting_enabled_default_value, _, init_delay))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });
  ON_CALL(
      *mock_delegate_ptr,
      CreateEventObserverManager(
          _, app_event_queue_ptr_.get(), _,
          test_case.setting_data.enable_setting_path,
          test_case.setting_data.setting_enabled_default_value, _, init_delay))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });
  ON_CALL(
      *mock_delegate_ptr,
      CreateEventObserverManager(
          _, website_event_queue_ptr_.get(), _,
          test_case.setting_data.enable_setting_path,
          test_case.setting_data.setting_enabled_default_value, _, init_delay))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });
  ON_CALL(
      *mock_delegate_ptr,
      CreateEventObserverManager(
          _, crash_event_queue_ptr_.get(), _,
          test_case.setting_data.enable_setting_path,
          test_case.setting_data.setting_enabled_default_value, _, init_delay))
      .WillByDefault(
          // We expect `FatalCrashEventObserver` to be owned by
          // `MetricEventObserverManager`.
          WithArg<0>([&](std::unique_ptr<MetricEventObserver> event_observer) {
            return std::make_unique<FakeMetricEventObserverManager>(
                fake_reporting_settings.get(), &observer_manager_count,
                std::move(event_observer));
          }));
  ON_CALL(
      *mock_delegate_ptr,
      CreateEventObserverManager(
          _, chrome_crash_event_queue_ptr_.get(), _,
          test_case.setting_data.enable_setting_path,
          test_case.setting_data.setting_enabled_default_value, _, init_delay))
      .WillByDefault(
          // We expect `ChromeFatalCrashEventObserver` to be owned by
          // `MetricEventObserverManager`.
          WithArg<0>([&](std::unique_ptr<MetricEventObserver> event_observer) {
            return std::make_unique<FakeMetricEventObserverManager>(
                fake_reporting_settings.get(), &observer_manager_count,
                std::move(event_observer));
          }));

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

  EXPECT_EQ(observer_manager_count, test_case.expected_count_before_login);

  metric_reporting_manager->OnLogin(profile());

  EXPECT_EQ(observer_manager_count, test_case.expected_count_after_login);

  ON_CALL(*mock_delegate_ptr, IsDeprovisioned).WillByDefault(Return(true));
  metric_reporting_manager->DeviceSettingsUpdated();

  EXPECT_EQ(observer_manager_count, 0);
}

TEST_F(MetricReportingManagerEventTest,
       ShouldNotCreateAppEventObserverWhenAppServiceUnavailable) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableAppEventsObserver);

  // Setup appropriate mocks and stubs.
  auto fake_reporting_settings =
      std::make_unique<test::FakeReportingSettings>();
  auto* const mock_delegate_ptr = mock_delegate_.get();
  int observer_manager_count = 0;
  ON_CALL(*mock_delegate_ptr, IsUserAffiliated).WillByDefault(Return(true));
  ON_CALL(*mock_delegate_ptr, IsAppServiceAvailableForProfile)
      .WillByDefault(Return(false));
  ON_CALL(
      *mock_delegate_ptr,
      CreateEventObserverManager(
          _, event_queue_ptr_.get(), _, app_event_settings.enable_setting_path,
          app_event_settings.setting_enabled_default_value, _, _))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });
  ON_CALL(*mock_delegate_ptr,
          CreateEventObserverManager(
              _, user_event_queue_ptr_.get(), _,
              app_event_settings.enable_setting_path,
              app_event_settings.setting_enabled_default_value, _, _))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });
  ON_CALL(*mock_delegate_ptr,
          CreateEventObserverManager(
              _, app_event_queue_ptr_.get(), _,
              app_event_settings.enable_setting_path,
              app_event_settings.setting_enabled_default_value, _, _))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });
  ON_CALL(*mock_delegate_ptr,
          CreateEventObserverManager(
              _, website_event_queue_ptr_.get(), _,
              app_event_settings.enable_setting_path,
              app_event_settings.setting_enabled_default_value, _, _))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });

  // Create a metric reporting manager and ensure observer manager count is 0
  // before and after login.
  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);
  EXPECT_EQ(observer_manager_count, 0);
  metric_reporting_manager->OnLogin(profile());
  EXPECT_EQ(observer_manager_count, 0);

  ON_CALL(*mock_delegate_ptr, IsDeprovisioned).WillByDefault(Return(true));
  metric_reporting_manager->DeviceSettingsUpdated();
  EXPECT_EQ(observer_manager_count, 0);
}

TEST_F(MetricReportingManagerEventTest,
       ShouldNotCreateWebsiteEventObserverWhenAppServiceUnavailable) {
  // Setup appropriate mocks and stubs.
  auto fake_reporting_settings =
      std::make_unique<test::FakeReportingSettings>();
  auto* const mock_delegate_ptr = mock_delegate_.get();
  int observer_manager_count = 0;
  ON_CALL(*mock_delegate_ptr, IsUserAffiliated).WillByDefault(Return(true));
  ON_CALL(*mock_delegate_ptr, IsAppServiceAvailableForProfile)
      .WillByDefault(Return(false));
  ON_CALL(*mock_delegate_ptr,
          CreateEventObserverManager(
              _, event_queue_ptr_.get(), _,
              website_event_settings.enable_setting_path,
              website_event_settings.setting_enabled_default_value, _, _))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });
  ON_CALL(*mock_delegate_ptr,
          CreateEventObserverManager(
              _, user_event_queue_ptr_.get(), _,
              website_event_settings.enable_setting_path,
              website_event_settings.setting_enabled_default_value, _, _))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });

  // Create a metric reporting manager and ensure observer manager count is 0
  // before and after login.
  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);
  EXPECT_THAT(observer_manager_count, Eq(0));
  metric_reporting_manager->OnLogin(profile());
  EXPECT_THAT(observer_manager_count, Eq(0));

  ON_CALL(*mock_delegate_ptr, IsDeprovisioned).WillByDefault(Return(true));
  metric_reporting_manager->DeviceSettingsUpdated();
  EXPECT_THAT(observer_manager_count, Eq(0));
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
          /*is_affiliated=*/true, https_latency_event_settings,
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
          /*expected_count_after_login=*/1},
         {"AppEvents_FeatureFlagEnabled",
          /*enabled_features=*/{kEnableAppEventsObserver},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, app_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/1},
         {"AppEvents_FeatureFlagDisabled",
          /*enabled_features=*/{},
          /*disabled_features=*/{kEnableAppEventsObserver},
          /*is_affiliated=*/true, app_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"WebsiteEvents_Unaffiliated",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, website_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"WebsiteEvents_Default",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, website_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/1},
         {"FatalCrashEvents_Unaffiliated_FeatureUnchanged",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, fatal_crash_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"FatalCrashEvents_Unaffiliated_FeatureEnabled",
          /*enabled_features=*/{kEnableFatalCrashEventsObserver},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, fatal_crash_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/1,
          /*expected_count_after_login=*/1},
         {"FatalCrashEvents_Default_FeatureUnchanged",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, fatal_crash_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"FatalCrashEvents_Default_FeatureEnabled",
          /*enabled_features=*/{kEnableFatalCrashEventsObserver},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, fatal_crash_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/1,
          /*expected_count_after_login=*/1},
         {"ChromeFatalCrashEvents_Unaffiliated_FeatureUnchanged",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, chrome_fatal_crash_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"ChromeFatalCrashEvents_Unaffiliated_FeatureEnabled",
          /*enabled_features=*/{kEnableChromeFatalCrashEventsObserver},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, chrome_fatal_crash_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/1,
          /*expected_count_after_login=*/1},
         {"ChromeFatalCrashEvents_Default_FeatureUnchanged",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, chrome_fatal_crash_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"ChromeFatalCrashEvents_Default_FeatureEnabled",
          /*enabled_features=*/{kEnableChromeFatalCrashEventsObserver},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, chrome_fatal_crash_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/1,
          /*expected_count_after_login=*/1}}),
    [](const testing::TestParamInfo<MetricReportingManagerInfoTest::ParamType>&
           info) { return info.param.test_name; });

class MetricReportingManagerPeripheralTest : public MetricReportingManagerTest {
};

// These tests cover both peripheral telemetry and events since they share a
// queue.
TEST_P(MetricReportingManagerPeripheralTest, Default) {
  const MetricReportingManagerTestCase& test_case = GetParam();
  const base::TimeDelta init_delay = test_case.has_init_delay
                                         ? metrics::kInitialCollectionDelay
                                         : base::TimeDelta();

  auto fake_reporting_settings =
      std::make_unique<test::FakeReportingSettings>();
  auto* const mock_delegate_ptr = mock_delegate_.get();
  int observer_manager_count = 0;
  ON_CALL(*mock_delegate_ptr, IsUserAffiliated)
      .WillByDefault(Return(test_case.is_affiliated));

  // Mock app service unavailability to eliminate noise.
  ON_CALL(*mock_delegate_ptr, IsAppServiceAvailableForProfile)
      .WillByDefault(Return(false));
  ON_CALL(
      *mock_delegate_ptr,
      CreateEventObserverManager(
          _, peripheral_queue_ptr_.get(), _,
          test_case.setting_data.enable_setting_path,
          test_case.setting_data.setting_enabled_default_value, _, init_delay))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

  EXPECT_EQ(observer_manager_count, test_case.expected_count_before_login);

  metric_reporting_manager->OnLogin(profile());

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
          CreateOneShotCollector(_, telemetry_queue_ptr_.get(), _,
                                 ::ash::kReportDeviceBootMode, true,
                                 metrics::kInitialCollectionDelay))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

  EXPECT_EQ(collector_count, 1);

  task_environment_.FastForwardBy(upload_delay +
                                  metrics::kInitialCollectionDelay);

  EXPECT_EQ(telemetry_queue_ptr_->GetNumFlush(), 1);

  ON_CALL(*mock_delegate_ptr, IsDeprovisioned).WillByDefault(Return(true));
  metric_reporting_manager->DeviceSettingsUpdated();

  EXPECT_EQ(collector_count, 0);
}

TEST_P(MetricReportingManagerTelemetryTest, Default) {
  const MetricReportingManagerTestCase& test_case = GetParam();
  const base::TimeDelta init_delay = test_case.has_init_delay
                                         ? metrics::kInitialCollectionDelay
                                         : base::TimeDelta();

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(test_case.enabled_features,
                                       test_case.disabled_features);

  const auto upload_delay = mock_delegate_->GetInitialUploadDelay();
  auto* const mock_delegate_ptr = mock_delegate_.get();
  int collector_count = 0;
  ON_CALL(*mock_delegate_ptr, IsUserAffiliated)
      .WillByDefault(Return(test_case.is_affiliated));
  // Mock app service unavailability to eliminate noise.
  ON_CALL(*mock_delegate_ptr, IsAppServiceAvailableForProfile)
      .WillByDefault(Return(false));
  ON_CALL(*mock_delegate_ptr,
          CreatePeriodicCollector(
              _, telemetry_queue_ptr_.get(), _,
              test_case.setting_data.enable_setting_path,
              test_case.setting_data.setting_enabled_default_value,
              test_case.setting_data.rate_setting_path, _,
              test_case.setting_data.rate_unit_to_ms, init_delay))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });
  ON_CALL(*mock_delegate_ptr,
          CreatePeriodicCollector(
              _, user_telemetry_queue_ptr_.get(), _,
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
                                  metrics::kInitialCollectionDelay);

  EXPECT_EQ(telemetry_queue_ptr_->GetNumFlush(), 1);

  metric_reporting_manager->OnLogin(profile());

  EXPECT_EQ(collector_count, test_case.expected_count_after_login);

  const int expected_login_flush_count = test_case.is_affiliated ? 1 : 0;
  task_environment_.FastForwardBy(upload_delay +
                                  metrics::kInitialCollectionDelay);

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
          /*expected_count_after_login=*/1},
         {"DeviceActivityTelemetry_Unaffiliated", /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, device_activity_telemetry_settings,
          /*has_init_delay=*/true,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"DeviceActivityTelemetry_Default", /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, device_activity_telemetry_settings,
          /*has_init_delay=*/true,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/1},
         {"RuntimeCountersTelemetry_Unaffiliated_FeatureUnchanged",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, runtime_counters_telemetry_settings,
          /*has_init_delay=*/true,
          /*expected_count_before_login=*/1,
          /*expected_count_after_login=*/1},
         {"RuntimeCountersTelemetry_Unaffiliated_FeatureDisabled",
          /*enabled_features=*/{},
          /*disabled_features=*/{kEnableRuntimeCountersTelemetry},
          /*is_affiliated=*/false, runtime_counters_telemetry_settings,
          /*has_init_delay=*/true,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0},
         {"RuntimeCountersTelemetry_Default_FeatureUnchanged",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, runtime_counters_telemetry_settings,
          /*has_init_delay=*/true,
          /*expected_count_before_login=*/1,
          /*expected_count_after_login=*/1},
         {"RuntimeCountersTelemetry_Default_FeatureDisabled",
          /*enabled_features=*/{},
          /*disabled_features=*/{kEnableRuntimeCountersTelemetry},
          /*is_affiliated=*/true, runtime_counters_telemetry_settings,
          /*has_init_delay=*/true,
          /*expected_count_before_login=*/0,
          /*expected_count_after_login=*/0}}),
    [](const testing::TestParamInfo<
        MetricReportingManagerTelemetryTest::ParamType>& info) {
      return info.param.test_name;
    });

class KioskHeartbeatTelemetryTest : public MetricReportingManagerTest {
 protected:
  void SetUp() override {
    MetricReportingManagerTest::SetUp();
    collector_count_ = 0;
    auto* const mock_delegate_ptr = mock_delegate_.get();

    ON_CALL(*mock_delegate_ptr, IsUserAffiliated).WillByDefault(Return(true));
    // Mock app service unavailability to eliminate noise.
    ON_CALL(*mock_delegate_ptr, IsAppServiceAvailableForProfile)
        .WillByDefault(Return(false));
    ON_CALL(*mock_delegate_ptr,
            CreatePeriodicCollector(
                /*sampler=*/_,
                /*queue=*/heartbeat_queue_ptr_.get(),
                /*report_settings=*/_,
                /*enable_setting_path=*/::ash::kHeartbeatEnabled,
                /*setting_enabled_default_value=*/
                metrics::kHeartbeatTelemetryDefaultValue,
                /*rate_setting_path=*/::ash::kHeartbeatFrequency, _, 1,
                /*init_delay=*/base::TimeDelta()))
        .WillByDefault([&]() {
          return std::make_unique<FakeCollector>(&collector_count_);
        });
  }

  int collector_count_;
};

TEST_F(KioskHeartbeatTelemetryTest, Disabled) {
  auto* const mock_delegate_ptr = mock_delegate_.get();
  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

  // MetricReportQueue for KIOSK_HEARTBEAT_EVENTS must not be created for
  // disabled flag kKioskHeartbeatsViaERP
  EXPECT_CALL(*mock_delegate_ptr,
              CreatePeriodicUploadReportQueue(
                  _, Destination::KIOSK_HEARTBEAT_EVENTS, _, _, _, _, _, _))
      .Times(0);

  // Ignore any other call to CreatePeriodicCollector because it's irrelevant to
  // this test.
  EXPECT_CALL(
      *mock_delegate_ptr,
      CreatePeriodicCollector(_, Not(Pointer(heartbeat_queue_ptr_.get())), _, _,
                              _, _, _, _, _))
      .Times(AnyNumber());
  // PeriodicCollector should be not be created as disabled.
  EXPECT_CALL(
      *mock_delegate_ptr,
      CreatePeriodicCollector(
          /*sampler=*/_,
          /*queue=*/heartbeat_queue_ptr_.get(),
          /*report_settings=*/_,
          /*enable_setting_path=*/::ash::kHeartbeatEnabled,
          /*setting_enabled_default_value=*/
          metrics::kHeartbeatTelemetryDefaultValue,
          /*rate_setting_path=*/::ash::kHeartbeatFrequency, _, 1,
          /*init_delay=*/metrics::kDefaultHeartbeatTelemetryCollectionRate))
      .Times(0);

  metric_reporting_manager->OnLogin(profile());
}

TEST_F(KioskHeartbeatTelemetryTest, Init) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      chromeos::features::kKioskHeartbeatsViaERP);

  auto* const mock_delegate_ptr = mock_delegate_.get();
  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

  // MetricReportQueue for KIOSK_HEARTBEAT_EVENTS has to be created for feature
  // flag kKioskHeartbeatsViaERP
  EXPECT_CALL(*mock_delegate_ptr,
              CreatePeriodicUploadReportQueue(
                  _, Destination::KIOSK_HEARTBEAT_EVENTS, _, _, _, _, _, _))
      .Times(1);

  // Ignore any other call to CreatePeriodicCollector -> irrelevant. Should
  // be covered in other places.
  EXPECT_CALL(
      *mock_delegate_ptr,
      CreatePeriodicCollector(_, Not(Pointer(heartbeat_queue_ptr_.get())), _, _,
                              _, _, _, _, _))
      .Times(AnyNumber());
  // PeriodicCollector should be created here.
  EXPECT_CALL(*mock_delegate_ptr,
              CreatePeriodicCollector(
                  /*sampler=*/_,
                  /*queue=*/heartbeat_queue_ptr_.get(),
                  /*report_settings=*/_,
                  /*enable_setting_path=*/::ash::kHeartbeatEnabled,
                  /*setting_enabled_default_value=*/
                  metrics::kHeartbeatTelemetryDefaultValue,
                  /*rate_setting_path=*/::ash::kHeartbeatFrequency, _, 1,
                  /*init_delay=*/base::TimeDelta()))
      .Times(1);

  metric_reporting_manager->OnLogin(profile());
  EXPECT_EQ(collector_count_, 1);

  // Call Flush after initial delay
  task_environment_.FastForwardBy(mock_delegate_->GetInitialUploadDelay() +
                                  metrics::kInitialCollectionDelay);
  EXPECT_EQ(telemetry_queue_ptr_->GetNumFlush(), 1);

  // deprovision -> destruction
  ON_CALL(*mock_delegate_ptr, IsDeprovisioned).WillByDefault(Return(true));
  metric_reporting_manager->DeviceSettingsUpdated();

  EXPECT_EQ(collector_count_, 0);
}

class KioskVisionTelemetryTest : public MetricReportingManagerTest {
 protected:
  void SetUp() override {
    MetricReportingManagerTest::SetUp();
    auto* const mock_delegate_ptr = mock_delegate_.get();

    ON_CALL(*mock_delegate_ptr, IsUserAffiliated).WillByDefault(Return(true));
    // Mock app service unavailability to eliminate noise.
    ON_CALL(*mock_delegate_ptr, IsAppServiceAvailableForProfile)
        .WillByDefault(Return(false));
    ON_CALL(
        *mock_delegate_ptr,
        CreatePeriodicCollector(
            _, _, _,
            /*enable_setting_path=*/::ash::prefs::kKioskVisionTelemetryEnabled,
            _, _, _, _, _))
        .WillByDefault([&]() {
          return std::make_unique<FakeCollector>(&collector_count_);
        });
  }

  // Counts the number of `PeriodicCollector`s created for KioskVision
  // telemetry.
  int collector_count_{0};
};

TEST_F(KioskVisionTelemetryTest, Disabled) {
  auto* const mock_delegate_ptr = mock_delegate_.get();
  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

  // Ignore any call to `CreatePeriodicCollector()` because it's irrelevant to
  // this test.
  EXPECT_CALL(*mock_delegate_ptr,
              CreatePeriodicCollector(
                  _, _, _, Not(::ash::prefs::kKioskVisionTelemetryEnabled), _,
                  _, _, _, _))
      .Times(AnyNumber());
  // PeriodicCollector should be not be created as the feature is disabled.
  EXPECT_CALL(
      *mock_delegate_ptr,
      CreatePeriodicCollector(
          /*sampler=*/_,
          /*metric_report_queue=*/_,
          /*reporting_settings=*/_,
          /*enable_setting_path=*/::ash::prefs::kKioskVisionTelemetryEnabled,
          /*setting_enabled_default_value=*/
          _,
          /*rate_setting_path=*/_, _, _,
          /*init_delay=*/_))
      .Times(0);

  metric_reporting_manager->OnLogin(profile());
}

TEST_F(KioskVisionTelemetryTest, Init) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(kEnableKioskVisionTelemetry);

  auto* const mock_delegate_ptr = mock_delegate_.get();
  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

  // Calls to create other collectors are not relevant.
  EXPECT_CALL(*mock_delegate_ptr,
              CreatePeriodicCollector(
                  _, _, _, Not(::ash::prefs::kKioskVisionTelemetryEnabled), _,
                  _, _, _, _))
      .Times(AnyNumber());
  // PeriodicCollector should be created here.
  EXPECT_CALL(
      *mock_delegate_ptr,
      CreatePeriodicCollector(
          /*sampler=*/_,
          /*metric_report_queue=*/user_telemetry_queue_ptr_.get(),
          /*reporting_settings=*/_,
          /*enable_setting_path=*/::ash::prefs::kKioskVisionTelemetryEnabled,
          /*setting_enabled_default_value=*/
          metrics::kKioskVisionTelemetryDefaultValue,
          /*rate_setting_path=*/::ash::prefs::kKioskVisionTelemetryFrequency, _,
          1,
          /*init_delay=*/metrics::kInitialCollectionDelay))
      .Times(1);

  EXPECT_EQ(collector_count_, 0);
  metric_reporting_manager->OnLogin(profile());
  EXPECT_EQ(collector_count_, 1);

  // Call Flush after initial delay.
  task_environment_.FastForwardBy(mock_delegate_->GetInitialUploadDelay() +
                                  metrics::kInitialCollectionDelay);
  EXPECT_EQ(telemetry_queue_ptr_->GetNumFlush(), 1);

  // Deprovising the delegate should lead to the destruction of the collector.
  ON_CALL(*mock_delegate_ptr, IsDeprovisioned).WillByDefault(Return(true));
  metric_reporting_manager->DeviceSettingsUpdated();

  EXPECT_EQ(collector_count_, 0);
}

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
    // App event report queue.
    CreateMockRateLimitedMetricReportQueueHelper(
        mock_delegate_.get(), EventType::kUser, Destination::EVENT_METRIC,
        Priority::SLOW_BATCH, metrics::kAppEventsTotalSize,
        metrics::kAppEventsWindow, metrics::kAppEventsBucketCount);

    // Website event report queue.
    CreateMockRateLimitedMetricReportQueueHelper(
        mock_delegate_.get(), EventType::kUser, Destination::EVENT_METRIC,
        Priority::SLOW_BATCH, metrics::kWebsiteEventsTotalSize,
        metrics::kWebsiteEventsWindow, metrics::kWebsiteEventsBucketCount);

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

  content::BrowserTaskEnvironment task_environment_;

  raw_ptr<CollectorBase, DanglingUntriaged> https_latency_collector_ptr_;
  raw_ptr<CollectorBase, DanglingUntriaged> network_telemetry_collector_ptr_;

  std::unique_ptr<::testing::NiceMock<MockDelegate>> mock_delegate_;
  ::ash::ScopedTestingCrosSettings cros_settings_;
  ScopedTestingLocalState local_state_{TestingBrowserProcess::GetGlobal()};

  // Placeholder test profile needed for initializing downstream components.
  TestingProfile profile_;
};

TEST_P(EventDrivenTelemetryCollectorPoolTest,
       SettingBasedTelemetry_AffiliatedOnly) {
  EventDrivenTelemetryCollectorPoolTestCase test_case = GetParam();

  base::Value::List telemetry_list;
  telemetry_list.Append("invalid");
  telemetry_list.Append("network_telemetry");
  telemetry_list.Append("https_latency");
  telemetry_list.Append("https_latency");  // duplicate.
  telemetry_list.Append("invalid");

  cros_settings_.device_settings()->Set(test_case.setting_name,
                                        base::Value(std::move(telemetry_list)));

  ON_CALL(*mock_delegate_, IsDeprovisioned).WillByDefault(Return(false));
  ON_CALL(*mock_delegate_, IsUserAffiliated).WillByDefault(Return(true));
  ON_CALL(*mock_delegate_, IsAppServiceAvailableForProfile)
      .WillByDefault(Return(false));

  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(mock_delegate_), nullptr);

  std::vector<raw_ptr<CollectorBase, VectorExperimental>> event_telemetry =
      metric_reporting_manager->GetTelemetryCollectors(test_case.event_type);

  ASSERT_TRUE(event_telemetry.empty());

  metric_reporting_manager->OnLogin(&profile_);

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
        {{"SignalStrengthLow", MetricEventType::WIFI_SIGNAL_STRENGTH_LOW,
          ash::kReportDeviceSignalStrengthEventDrivenTelemetry},
         {"SignalStrengthRecovered",
          MetricEventType::WIFI_SIGNAL_STRENGTH_RECOVERED,
          ash::kReportDeviceSignalStrengthEventDrivenTelemetry}}),
    [](const testing::TestParamInfo<
        EventDrivenTelemetryCollectorPoolTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace reporting
