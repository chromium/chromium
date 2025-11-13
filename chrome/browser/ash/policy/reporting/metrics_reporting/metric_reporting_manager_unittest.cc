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
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service_test_base.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager_for_test.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_prefs.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/fakes/fake_metric_event_observer.h"
#include "components/reporting/metrics/fakes/fake_metric_report_queue.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/metrics/fakes/fake_sampler.h"
#include "components/reporting/metrics/metric_event_observer.h"
#include "components/reporting/metrics/metric_event_observer_manager.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
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
using testing::Return;
using testing::SizeIs;
using testing::StrEq;
using testing::StrNe;
using testing::WithArg;
using testing::WithArgs;

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

// Deprovising the delegate (assuming it is MockDelegate).
void DeprovisionDelegate(MetricReportingManager& manager) {
  ON_CALL(*static_cast<test::MockDelegate*>(manager.delegate()),
          IsDeprovisioned)
      .WillByDefault(Return(true));
  manager.DeviceSettingsUpdated();
}

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

    // Prepare delegate.
    mock_delegate_ =
        std::make_unique<::testing::NiceMock<test::MockDelegate>>();
    ON_CALL(*mock_delegate_, CreateMetricReportQueue(_, _, _, _, _))
        .WillByDefault(WithArg<2>([](Priority priority) {
          return std::make_unique<test::FakeMetricReportQueue>(priority);
        }));
    ON_CALL(*mock_delegate_,
            CreatePeriodicUploadReportQueue(_, _, _, _, _, _, _, _))
        .WillByDefault(WithArgs<2, 3, 4, 5, 6>(
            [](Priority priority, ReportingSettings* reporting_settings,
               const std::string& rate_setting_path,
               base::TimeDelta default_rate, int rate_unit_to_ms) {
              return std::make_unique<test::FakeMetricReportQueue>(
                  priority, reporting_settings, rate_setting_path, default_rate,
                  rate_unit_to_ms);
            }));
  }

  ::ash::SessionTerminationManager session_termination_manager_;
  std::unique_ptr<::testing::NiceMock<test::MockDelegate>> mock_delegate_;
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

  // Create a metric reporting manager.
  const auto metric_reporting_manager =
      test::MetricReportingManagerForTest::Create(std::move(mock_delegate_),
                                                  nullptr);

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

  int collector_count = 0;
  ON_CALL(*mock_delegate_, IsUserAffiliated)
      .WillByDefault(Return(test_case.is_affiliated));

  // Mock app service unavailability to eliminate noise.
  ON_CALL(*mock_delegate_, IsAppServiceAvailableForProfile)
      .WillByDefault(Return(false));

  ON_CALL(*mock_delegate_,
          CreateOneShotCollector(
              _, _,  // info_queue
              _, test_case.setting_data.enable_setting_path,
              test_case.setting_data.setting_enabled_default_value, init_delay))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });

  // Create a metric reporting manager.
  const auto metric_reporting_manager =
      test::MetricReportingManagerForTest::Create(std::move(mock_delegate_),
                                                  nullptr);

  EXPECT_EQ(collector_count, test_case.expected_count_before_login);

  metric_reporting_manager->OnLogin(profile());

  EXPECT_EQ(collector_count, test_case.expected_count_after_login);

  DeprovisionDelegate(*metric_reporting_manager);

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
  int observer_manager_count = 0;
  ON_CALL(*mock_delegate_, IsUserAffiliated)
      .WillByDefault(Return(test_case.is_affiliated));
  ON_CALL(*mock_delegate_, IsAppServiceAvailableForProfile)
      .WillByDefault(Return(true));

  ON_CALL(
      *mock_delegate_,
      CreateEventObserverManager(
          _, _,  // event_queue
          _, test_case.setting_data.enable_setting_path,
          test_case.setting_data.setting_enabled_default_value, _, init_delay))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });
  ON_CALL(
      *mock_delegate_,
      CreateEventObserverManager(
          _, _,  // user_event_queue
          _, test_case.setting_data.enable_setting_path,
          test_case.setting_data.setting_enabled_default_value, _, init_delay))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });
  ON_CALL(
      *mock_delegate_,
      CreateEventObserverManager(
          _, _,  // app_event_queue,
          _, test_case.setting_data.enable_setting_path,
          test_case.setting_data.setting_enabled_default_value, _, init_delay))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });
  ON_CALL(
      *mock_delegate_,
      CreateEventObserverManager(
          _, _,  // website_event_queue
          _, test_case.setting_data.enable_setting_path,
          test_case.setting_data.setting_enabled_default_value, _, init_delay))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });
  ON_CALL(
      *mock_delegate_,
      CreateEventObserverManager(
          _, _,  // crash_event_queue
          _, test_case.setting_data.enable_setting_path,
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
      *mock_delegate_,
      CreateEventObserverManager(
          _, _,  // chrome_crash_event_queue
          _, test_case.setting_data.enable_setting_path,
          test_case.setting_data.setting_enabled_default_value, _, init_delay))
      .WillByDefault(
          // We expect `ChromeFatalCrashEventObserver` to be owned by
          // `MetricEventObserverManager`.
          WithArg<0>([&](std::unique_ptr<MetricEventObserver> event_observer) {
            return std::make_unique<FakeMetricEventObserverManager>(
                fake_reporting_settings.get(), &observer_manager_count,
                std::move(event_observer));
          }));

  // Create a metric reporting manager.
  const auto metric_reporting_manager =
      test::MetricReportingManagerForTest::Create(std::move(mock_delegate_),
                                                  nullptr);
  EXPECT_EQ(observer_manager_count, test_case.expected_count_before_login);

  metric_reporting_manager->OnLogin(profile());

  EXPECT_EQ(observer_manager_count, test_case.expected_count_after_login);

  DeprovisionDelegate(*metric_reporting_manager);

  EXPECT_EQ(observer_manager_count, 0);
}

TEST_F(MetricReportingManagerEventTest,
       ShouldNotCreateAppEventObserverWhenAppServiceUnavailable) {
  // Setup appropriate mocks and stubs.
  auto fake_reporting_settings =
      std::make_unique<test::FakeReportingSettings>();
  int observer_manager_count = 0;
  ON_CALL(*mock_delegate_, IsUserAffiliated).WillByDefault(Return(true));
  ON_CALL(*mock_delegate_, IsAppServiceAvailableForProfile)
      .WillByDefault(Return(false));

  ON_CALL(*mock_delegate_,
          CreateEventObserverManager(
              _, _,  // event_queue,
              _, app_event_settings.enable_setting_path,
              app_event_settings.setting_enabled_default_value, _, _))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });
  ON_CALL(*mock_delegate_,
          CreateEventObserverManager(
              _, _,  // user_event_queue,
              _, app_event_settings.enable_setting_path,
              app_event_settings.setting_enabled_default_value, _, _))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });
  ON_CALL(*mock_delegate_,
          CreateEventObserverManager(
              _, _,  // app_event_queue,
              _, app_event_settings.enable_setting_path,
              app_event_settings.setting_enabled_default_value, _, _))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });
  ON_CALL(*mock_delegate_,
          CreateEventObserverManager(
              _, _,  // website_event_queue,
              _, app_event_settings.enable_setting_path,
              app_event_settings.setting_enabled_default_value, _, _))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });

  // Create a metric reporting manager.
  const auto metric_reporting_manager =
      test::MetricReportingManagerForTest::Create(std::move(mock_delegate_),
                                                  nullptr);

  // Ensure observer manager count is 0 before and after login.
  EXPECT_EQ(observer_manager_count, 0);
  metric_reporting_manager->OnLogin(profile());
  EXPECT_EQ(observer_manager_count, 0);

  DeprovisionDelegate(*metric_reporting_manager);

  EXPECT_EQ(observer_manager_count, 0);
}

TEST_F(MetricReportingManagerEventTest,
       ShouldNotCreateWebsiteEventObserverWhenAppServiceUnavailable) {
  // Setup appropriate mocks and stubs.
  auto fake_reporting_settings =
      std::make_unique<test::FakeReportingSettings>();
  int observer_manager_count = 0;
  ON_CALL(*mock_delegate_, IsUserAffiliated).WillByDefault(Return(true));
  ON_CALL(*mock_delegate_, IsAppServiceAvailableForProfile)
      .WillByDefault(Return(false));

  ON_CALL(*mock_delegate_,
          CreateEventObserverManager(
              _, _,  // event_queue,
              _, website_event_settings.enable_setting_path,
              website_event_settings.setting_enabled_default_value, _, _))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });
  ON_CALL(*mock_delegate_,
          CreateEventObserverManager(
              _, _,  // user_event_queue,
              _, website_event_settings.enable_setting_path,
              website_event_settings.setting_enabled_default_value, _, _))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });

  // Create a metric reporting manager.
  const auto metric_reporting_manager =
      test::MetricReportingManagerForTest::Create(std::move(mock_delegate_),
                                                  nullptr);

  // Ensure observer manager count is 0 before and after login.
  EXPECT_THAT(observer_manager_count, Eq(0));
  metric_reporting_manager->OnLogin(profile());
  EXPECT_THAT(observer_manager_count, Eq(0));

  DeprovisionDelegate(*metric_reporting_manager);

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
         {"FatalCrashEvents_Unaffiliated_FeatureEnabled",
          /*enabled_features=*/{kEnableFatalCrashEventsObserver},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, fatal_crash_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/2,
          /*expected_count_after_login=*/2},
         {"FatalCrashEvents_Default_FeatureEnabled",
          /*enabled_features=*/{kEnableFatalCrashEventsObserver},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, fatal_crash_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/2,
          /*expected_count_after_login=*/2},
         {"ChromeFatalCrashEvents_Unaffiliated_FeatureEnabled",
          /*enabled_features=*/{kEnableChromeFatalCrashEventsObserver},
          /*disabled_features=*/{},
          /*is_affiliated=*/false, chrome_fatal_crash_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/2,
          /*expected_count_after_login=*/2},
         {"ChromeFatalCrashEvents_Default_FeatureEnabled",
          /*enabled_features=*/{kEnableChromeFatalCrashEventsObserver},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, chrome_fatal_crash_event_settings,
          /*has_init_delay=*/false,
          /*expected_count_before_login=*/2,
          /*expected_count_after_login=*/2}}),
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
  int observer_manager_count = 0;
  ON_CALL(*mock_delegate_, IsUserAffiliated)
      .WillByDefault(Return(test_case.is_affiliated));

  // Mock app service unavailability to eliminate noise.
  ON_CALL(*mock_delegate_, IsAppServiceAvailableForProfile)
      .WillByDefault(Return(false));

  ON_CALL(
      *mock_delegate_,
      CreateEventObserverManager(
          _,
          _,  // user_peripheral_events_and_telemetry_queue
          _, test_case.setting_data.enable_setting_path,
          test_case.setting_data.setting_enabled_default_value, _, init_delay))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });

  // Create a metric reporting manager.
  const auto metric_reporting_manager =
      test::MetricReportingManagerForTest::Create(std::move(mock_delegate_),
                                                  nullptr);

  EXPECT_EQ(observer_manager_count, test_case.expected_count_before_login);

  metric_reporting_manager->OnLogin(profile());

  EXPECT_EQ(observer_manager_count, test_case.expected_count_after_login);

  DeprovisionDelegate(*metric_reporting_manager);

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
  int collector_count = 0;

  ON_CALL(*mock_delegate_,
          CreateOneShotCollector(_, _,  // telemetry_queue
                                 _, ::ash::kReportDeviceBootMode, true,
                                 metrics::kInitialCollectionDelay))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });

  // Create a metric reporting manager.
  const auto metric_reporting_manager =
      test::MetricReportingManagerForTest::Create(std::move(mock_delegate_),
                                                  nullptr);

  EXPECT_EQ(collector_count, 1);

  task_environment_.FastForwardBy(upload_delay +
                                  metrics::kInitialCollectionDelay);

  EXPECT_EQ(metric_reporting_manager->telemetry_queue()->GetNumFlush(), 1);

  DeprovisionDelegate(*metric_reporting_manager);

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
  int collector_count = 0;
  ON_CALL(*mock_delegate_, IsUserAffiliated)
      .WillByDefault(Return(test_case.is_affiliated));
  // Mock app service unavailability to eliminate noise.
  ON_CALL(*mock_delegate_, IsAppServiceAvailableForProfile)
      .WillByDefault(Return(false));

  ON_CALL(*mock_delegate_,
          CreatePeriodicCollector(
              _, _,  // telemetry_queue,
              _, test_case.setting_data.enable_setting_path,
              test_case.setting_data.setting_enabled_default_value,
              test_case.setting_data.rate_setting_path, _,
              test_case.setting_data.rate_unit_to_ms, init_delay))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });
  ON_CALL(*mock_delegate_,
          CreatePeriodicCollector(
              _, _,  // user_telemetry_queue,
              _, test_case.setting_data.enable_setting_path,
              test_case.setting_data.setting_enabled_default_value,
              test_case.setting_data.rate_setting_path, _,
              test_case.setting_data.rate_unit_to_ms, init_delay))
      .WillByDefault(
          [&]() { return std::make_unique<FakeCollector>(&collector_count); });

  // Create a metric reporting manager.
  const auto metric_reporting_manager =
      test::MetricReportingManagerForTest::Create(std::move(mock_delegate_),
                                                  nullptr);

  EXPECT_EQ(collector_count, test_case.expected_count_before_login);

  task_environment_.FastForwardBy(upload_delay +
                                  metrics::kInitialCollectionDelay);

  EXPECT_EQ(metric_reporting_manager->telemetry_queue()->GetNumFlush(), 1);

  metric_reporting_manager->OnLogin(profile());

  EXPECT_EQ(collector_count, test_case.expected_count_after_login);

  const int expected_login_flush_count = test_case.is_affiliated ? 1 : 0;
  task_environment_.FastForwardBy(upload_delay +
                                  metrics::kInitialCollectionDelay);

  EXPECT_EQ(metric_reporting_manager->telemetry_queue()->GetNumFlush(),
            1 + expected_login_flush_count);

  DeprovisionDelegate(*metric_reporting_manager);

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
         {"RuntimeCountersTelemetry_Default_FeatureUnchanged",
          /*enabled_features=*/{},
          /*disabled_features=*/{},
          /*is_affiliated=*/true, runtime_counters_telemetry_settings,
          /*has_init_delay=*/true,
          /*expected_count_before_login=*/1,
          /*expected_count_after_login=*/1}}),
    [](const testing::TestParamInfo<
        MetricReportingManagerTelemetryTest::ParamType>& info) {
      return info.param.test_name;
    });

class KioskHeartbeatTelemetryTest : public MetricReportingManagerTest {
 protected:
  void SetUp() override {
    MetricReportingManagerTest::SetUp();
    collector_count_ = 0;

    ON_CALL(*mock_delegate_, IsUserAffiliated).WillByDefault(Return(true));
    // Mock app service unavailability to eliminate noise.
    ON_CALL(*mock_delegate_, IsAppServiceAvailableForProfile)
        .WillByDefault(Return(false));
  }

  int collector_count_;
};

TEST_F(KioskHeartbeatTelemetryTest, Disabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      chromeos::features::kKioskHeartbeatsViaERP);

  // PeriodicCollector should be not be created as disabled.
  EXPECT_CALL(*mock_delegate_,
              CreatePeriodicCollector(
                  /*sampler=*/_,
                  /*metric_report_queue=*/_,
                  /*reporting_settings=*/_,
                  /*enable_setting_path=*/StrEq(::ash::kHeartbeatEnabled),
                  /*setting_enabled_default_value=*/
                  metrics::kHeartbeatTelemetryDefaultValue,
                  /*rate_setting_path=*/::ash::kHeartbeatFrequency, _, 1,
                  /*init_delay=*/base::TimeDelta()))
      .Times(0);

  // Ignore any other call to CreatePeriodicCollector because it's irrelevant to
  // this test.
  EXPECT_CALL(*mock_delegate_,
              CreatePeriodicCollector(_, _, _, StrNe(::ash::kHeartbeatEnabled),
                                      _, _, _, _, _))
      .Times(AnyNumber());

  // Create a metric reporting manager.
  const auto metric_reporting_manager =
      test::MetricReportingManagerForTest::Create(std::move(mock_delegate_),
                                                  nullptr);

  metric_reporting_manager->OnLogin(profile());
  EXPECT_FALSE(metric_reporting_manager->kiosk_heartbeat_telemetry_queue());
}

TEST_F(KioskHeartbeatTelemetryTest, Init) {
  const auto upload_delay = mock_delegate_->GetInitialUploadDelay();

  EXPECT_CALL(*mock_delegate_,
              CreatePeriodicCollector(
                  /*sampler=*/_,
                  /*metric_report_queue=*/_,
                  /*reporting_settings=*/_,
                  /*enable_setting_path=*/StrEq(::ash::kHeartbeatEnabled),
                  /*setting_enabled_default_value=*/
                  metrics::kHeartbeatTelemetryDefaultValue,
                  /*rate_setting_path=*/::ash::kHeartbeatFrequency, _, 1,
                  /*init_delay=*/base::TimeDelta()))
      .WillOnce(
          [&]() { return std::make_unique<FakeCollector>(&collector_count_); });

  // Ignore any other call to CreatePeriodicCollector -> irrelevant. Should
  // be covered in other places.
  EXPECT_CALL(*mock_delegate_,
              CreatePeriodicCollector(_, _, _, StrNe(::ash::kHeartbeatEnabled),
                                      _, _, _, _, _))
      .Times(AnyNumber());

  // Create a metric reporting manager.
  const auto metric_reporting_manager =
      test::MetricReportingManagerForTest::Create(std::move(mock_delegate_),
                                                  nullptr);

  metric_reporting_manager->OnLogin(profile());
  EXPECT_TRUE(metric_reporting_manager->kiosk_heartbeat_telemetry_queue());
  EXPECT_EQ(collector_count_, 1);

  // Call Flush after initial delay
  task_environment_.FastForwardBy(upload_delay +
                                  metrics::kInitialCollectionDelay);
  EXPECT_EQ(metric_reporting_manager->telemetry_queue()->GetNumFlush(), 1);

  // deprovision -> destruction
  DeprovisionDelegate(*metric_reporting_manager);

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

    mock_delegate_ =
        std::make_unique<::testing::NiceMock<test::MockDelegate>>();

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

  ::ash::SessionTerminationManager session_termination_manager_;
  std::unique_ptr<::testing::NiceMock<test::MockDelegate>> mock_delegate_;

  ::ash::ScopedStubInstallAttributes install_attributes_;
  ::ash::ScopedTestingCrosSettings cros_settings_;

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

  auto https_latency_collector = std::make_unique<FakeCollector>();
  https_latency_collector_ptr_ = https_latency_collector.get();
  auto network_telemetry_collector = std::make_unique<FakeCollector>();
  network_telemetry_collector_ptr_ = network_telemetry_collector.get();

  auto https_latency_sampler = std::make_unique<test::FakeSampler>();
  auto* const https_latency_sampler_ptr = https_latency_sampler.get();
  auto network_telemetry_sampler = std::make_unique<test::FakeSampler>();
  auto* const network_telemetry_sampler_ptr = network_telemetry_sampler.get();

  // Telemetry queue.
  ON_CALL(*mock_delegate_, CreatePeriodicUploadReportQueue).WillByDefault([] {
    return std::make_unique<test::FakeMetricReportQueue>();
  });

  ON_CALL(*mock_delegate_, GetHttpsLatencySampler)
      .WillByDefault(Return(ByMove(std::move(https_latency_sampler))));
  ON_CALL(*mock_delegate_, GetNetworkTelemetrySampler)
      .WillByDefault(Return(ByMove(std::move(network_telemetry_sampler))));
  ON_CALL(*mock_delegate_,
          CreatePeriodicCollector(network_telemetry_sampler_ptr, _, _, _, _, _,
                                  _, _, _))
      .WillByDefault(Return(ByMove(std::move(network_telemetry_collector))));
  ON_CALL(*mock_delegate_, CreatePeriodicCollector(https_latency_sampler_ptr, _,
                                                   _, _, _, _, _, _, _))
      .WillByDefault(Return(ByMove(std::move(https_latency_collector))));

  // Create a metric reporting manager.
  const auto metric_reporting_manager =
      test::MetricReportingManagerForTest::Create(std::move(mock_delegate_),
                                                  nullptr);

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
