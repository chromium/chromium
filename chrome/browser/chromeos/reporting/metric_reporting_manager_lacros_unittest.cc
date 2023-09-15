// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/metric_reporting_manager_lacros.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy_lacros.h"
#include "chrome/browser/apps/app_service/metrics/website_metrics_service_lacros.h"
#include "chrome/browser/chromeos/reporting/device_reporting_settings_lacros.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "components/policy/policy_constants.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/fakes/fake_metric_event_observer.h"
#include "components/reporting/metrics/fakes/fake_metric_report_queue.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/metrics/metric_event_observer_manager.h"
#include "components/reporting/metrics/reporting_settings.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/util/rate_limiter_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::_;
using ::testing::Address;
using ::testing::AllOf;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Optional;
using ::testing::Property;
using ::testing::Return;

namespace reporting {
namespace {

constexpr char kUserId[] = "123";

// Fake metric event observer manager implementation used by tests to monitor
// event observer manager instantiation.
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
  explicit FakeCollector(int* collector_count)
      : CollectorBase(nullptr), collector_count_(collector_count) {
    ++(*collector_count_);
  }

  FakeCollector(const FakeCollector& other) = delete;
  FakeCollector& operator=(const FakeCollector& other) = delete;

  ~FakeCollector() override { --(*collector_count_); }

 protected:
  // CollectorBase:
  void OnMetricDataCollected(bool is_event_driven,
                             absl::optional<MetricData> metric_data) override {}
  bool CanCollect() const override { return true; }

 private:
  raw_ptr<int> collector_count_;
};

class MockDelegate : public metrics::MetricReportingManagerLacros::Delegate {
 public:
  MockDelegate() = default;
  MockDelegate(const MockDelegate& other) = delete;
  MockDelegate& operator=(const MockDelegate& other) = delete;
  ~MockDelegate() override = default;

  MOCK_METHOD(bool, IsUserAffiliated, (Profile & profile), (const, override));

  MOCK_METHOD(
      void,
      CheckDeviceDeprovisioned,
      (crosapi::mojom::DeviceSettingsService::IsDeviceDeprovisionedCallback
           callback),
      (override));

  MOCK_METHOD(std::unique_ptr<DeviceReportingSettingsLacros>,
              CreateDeviceReportingSettings,
              (),
              (override));

  MOCK_METHOD(void,
              RegisterObserverWithCrosApiClient,
              (metrics::MetricReportingManagerLacros* const),
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

  MOCK_METHOD(std::unique_ptr<MetricReportQueue>,
              CreatePeriodicUploadReportQueue,
              (EventType event_type,
               Destination destination,
               Priority priority,
               ReportingSettings* reporting_settings,
               const std::string& rate_setting_path,
               base::TimeDelta default_rate,
               int rate_unit_to_ms,
               absl::optional<SourceInfo> source_info),
              (override));

  MOCK_METHOD(std::unique_ptr<MetricReportQueue>,
              CreateMetricReportQueue,
              (EventType event_type,
               Destination destination,
               Priority priority,
               std::unique_ptr<RateLimiterInterface> rate_limiter,
               absl::optional<SourceInfo> source_info),
              (override));
};

struct MetricReportingSettingData {
  std::string enable_setting_path;
  bool setting_enabled_default_value;
  std::string rate_setting_path;
  int rate_unit_to_ms;
};

const MetricReportingSettingData network_telemetry_settings = {
    ::policy::key::kReportDeviceNetworkStatus,
    metrics::kReportDeviceNetworkStatusDefaultValue,
    ::policy::key::kReportDeviceNetworkTelemetryCollectionRateMs, 1};
const MetricReportingSettingData website_event_settings = {
    kReportWebsiteActivityAllowlist,
    metrics::kReportWebsiteActivityEnabledDefaultValue, "", 1};

struct TestCase {
  std::string test_name;
  // Is the user affiliated.
  bool is_affiliated;
  MetricReportingSettingData setting_data;
  // Count of initialized components.
  int expected_count;
};

class MetricReportingManagerLacrosTest
    : public ::testing::TestWithParam<TestCase> {
 protected:
  MetricReportingManagerLacrosTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    profile_ = profile_manager_.CreateTestingProfile(kUserId);
    delegate_ = std::make_unique<NiceMock<MockDelegate>>();

    auto telemetry_queue = std::make_unique<test::FakeMetricReportQueue>();
    telemetry_queue_ptr_ = telemetry_queue.get();
    ON_CALL(*delegate_,
            CreatePeriodicUploadReportQueue(
                EventType::kUser, Destination::TELEMETRY_METRIC,
                Priority::MANUAL_BATCH_LACROS, _, _, _, 1,
                Optional(AllOf(
                    Property(&SourceInfo::source, Eq(SourceInfo::LACROS)),
                    Property(&SourceInfo::source_version, Not(IsEmpty()))))))
        .WillByDefault(Return(ByMove(std::move(telemetry_queue))));

    auto event_queue = std::make_unique<test::FakeMetricReportQueue>();
    event_queue_ptr_ = event_queue.get();
    ON_CALL(*delegate_,
            CreateMetricReportQueue(EventType::kUser, Destination::EVENT_METRIC,
                                    Priority::SLOW_BATCH, IsNull(), _))
        .WillByDefault(Return(ByMove(std::move(event_queue))));

    ON_CALL(*delegate_, RegisterObserverWithCrosApiClient)
        .WillByDefault([]() { /** Do nothing **/ });

    ON_CALL(*delegate_, CreateDeviceReportingSettings)
        .WillByDefault(Return(
            ByMove(std::unique_ptr<DeviceReportingSettingsLacros>(nullptr))));

    ON_CALL(*delegate_, CheckDeviceDeprovisioned(_))
        .WillByDefault([](crosapi::mojom::DeviceSettingsService::
                              IsDeviceDeprovisionedCallback callback) {
          std::move(callback).Run(false);
        });

    // The app service proxy does not initialize website metrics service for
    // test profiles by default, so we set up website metrics service to
    // simplify testing.
    auto website_metrics_service =
        std::make_unique<::apps::WebsiteMetricsServiceLacros>(profile_);
    ::apps::AppServiceProxyFactory::GetForProfile(profile_)
        ->SetWebsiteMetricsServiceForTesting(
            std::move(website_metrics_service));
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<MockDelegate> delegate_;
  raw_ptr<test::FakeMetricReportQueue> telemetry_queue_ptr_;
  raw_ptr<test::FakeMetricReportQueue> event_queue_ptr_;
};

TEST_F(MetricReportingManagerLacrosTest, InitiallyDeprovisioned) {
  int periodic_collector_count = 0;
  int observer_manager_count = 0;
  auto fake_reporting_settings =
      std::make_unique<test::FakeReportingSettings>();

  ON_CALL(*delegate_, IsUserAffiliated(Address(profile_)))
      .WillByDefault(Return(true));
  ON_CALL(*delegate_, CheckDeviceDeprovisioned(_))
      .WillByDefault([](crosapi::mojom::DeviceSettingsService::
                            IsDeviceDeprovisionedCallback callback) {
        std::move(callback).Run(true);
      });
  ON_CALL(*delegate_, CreatePeriodicCollector)
      .WillByDefault([&periodic_collector_count]() {
        return std::make_unique<FakeCollector>(&periodic_collector_count);
      });
  ON_CALL(*delegate_, CreateEventObserverManager).WillByDefault([&]() {
    return std::make_unique<FakeMetricEventObserverManager>(
        fake_reporting_settings.get(), &observer_manager_count);
  });

  auto* const delegate_ptr = delegate_.get();
  metrics::MetricReportingManagerLacros metric_reporting_manager(
      profile_.get(), std::move(delegate_));

  task_environment_.FastForwardBy(delegate_ptr->GetInitDelay());
  task_environment_.RunUntilIdle();
  EXPECT_THAT(periodic_collector_count, Eq(0));
  EXPECT_THAT(observer_manager_count, Eq(0));
}

TEST_F(MetricReportingManagerLacrosTest, SecondaryUserProfiles) {
  auto* const secondary_profile =
      profile_manager_.CreateTestingProfile("secondary_profile");
  const auto* const metric_reporting_manager =
      metrics::MetricReportingManagerLacros::GetForProfile(secondary_profile);
  EXPECT_THAT(metric_reporting_manager, IsNull());
}

class MetricReportingManagerLacrosTelemetryTest
    : public MetricReportingManagerLacrosTest {};

TEST_P(MetricReportingManagerLacrosTelemetryTest, Default) {
  const TestCase& test_case = GetParam();
  int periodic_collector_count = 0;
  ON_CALL(*delegate_, CreatePeriodicCollector(
                          _, telemetry_queue_ptr_.get(), _,
                          test_case.setting_data.enable_setting_path,
                          test_case.setting_data.setting_enabled_default_value,
                          test_case.setting_data.rate_setting_path, _,
                          test_case.setting_data.rate_unit_to_ms, _))
      .WillByDefault([&periodic_collector_count]() {
        return std::make_unique<FakeCollector>(&periodic_collector_count);
      });
  ON_CALL(*delegate_, IsUserAffiliated(Address(profile_)))
      .WillByDefault(Return(test_case.is_affiliated));

  auto* const delegate_ptr = delegate_.get();
  metrics::MetricReportingManagerLacros metric_reporting_manager(
      profile_.get(), std::move(delegate_));

  task_environment_.FastForwardBy(delegate_ptr->GetInitDelay());
  task_environment_.RunUntilIdle();
  ASSERT_THAT(periodic_collector_count, Eq(test_case.expected_count));

  // Simulate device deprovisioning and verify collectors are destroyed.
  EXPECT_CALL(*delegate_ptr, CheckDeviceDeprovisioned(_))
      .WillRepeatedly([](crosapi::mojom::DeviceSettingsService::
                             IsDeviceDeprovisionedCallback callback) {
        std::move(callback).Run(true);
      });
  metric_reporting_manager.OnDeviceSettingsUpdated();
  task_environment_.RunUntilIdle();
  EXPECT_THAT(periodic_collector_count, Eq(0));
}

INSTANTIATE_TEST_SUITE_P(
    MetricReportingManagerLacrosTelemetryTests,
    MetricReportingManagerLacrosTelemetryTest,
    ::testing::ValuesIn<TestCase>({
        {"NetworkTelemetry_Unaffiliated",
         /*is_affiliated=*/false, network_telemetry_settings,
         /*expected_count=*/0},
        {"NetworkTelemetry_Affiliated",
         /*is_affiliated=*/true, network_telemetry_settings,
         /*expected_count=*/1},
    }),
    [](const ::testing::TestParamInfo<
        MetricReportingManagerLacrosTest::ParamType>& info) {
      return info.param.test_name;
    });

class MetricReportingManagerLacrosEventTest
    : public MetricReportingManagerLacrosTest {};

TEST_P(MetricReportingManagerLacrosEventTest, Default) {
  const TestCase& test_case = GetParam();
  auto fake_reporting_settings =
      std::make_unique<test::FakeReportingSettings>();
  int observer_manager_count = 0;
  ON_CALL(*delegate_, IsUserAffiliated(Address(profile_)))
      .WillByDefault(Return(test_case.is_affiliated));
  ON_CALL(*delegate_, CreateEventObserverManager(
                          _, event_queue_ptr_.get(), _,
                          test_case.setting_data.enable_setting_path,
                          test_case.setting_data.setting_enabled_default_value,
                          _, base::TimeDelta()))
      .WillByDefault([&]() {
        return std::make_unique<FakeMetricEventObserverManager>(
            fake_reporting_settings.get(), &observer_manager_count);
      });

  auto* const delegate_ptr = delegate_.get();
  metrics::MetricReportingManagerLacros metric_reporting_manager(
      profile_.get(), std::move(delegate_));
  task_environment_.RunUntilIdle();
  EXPECT_THAT(observer_manager_count, Eq(test_case.expected_count));

  // Simulate device deprovisioning and verify event observer managers are
  // destroyed.
  EXPECT_CALL(*delegate_ptr, CheckDeviceDeprovisioned(_))
      .WillRepeatedly([](crosapi::mojom::DeviceSettingsService::
                             IsDeviceDeprovisionedCallback callback) {
        std::move(callback).Run(true);
      });
  metric_reporting_manager.OnDeviceSettingsUpdated();
  EXPECT_THAT(observer_manager_count, Eq(0));
}

INSTANTIATE_TEST_SUITE_P(
    MetricReportingManagerLacrosEventTests,
    MetricReportingManagerLacrosEventTest,
    ::testing::ValuesIn<TestCase>({
        {"WebsiteEvents_Unaffiliated",
         /*is_affiliated=*/false, website_event_settings,
         /*expected_count=*/0},
        {"WebsiteEvents_Affiliated",
         /*is_affiliated=*/true, website_event_settings,
         /*expected_count=*/1},
    }),
    [](const ::testing::TestParamInfo<
        MetricReportingManagerLacrosTest::ParamType>& info) {
      return info.param.test_name;
    });
}  // namespace
}  // namespace reporting
