// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/metric_reporting_manager_lacros.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/reporting/device_reporting_settings_lacros.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "components/policy/policy_constants.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/fakes/fake_metric_report_queue.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::IsNull;
using ::testing::NiceMock;
using ::testing::Return;

namespace reporting {
namespace {

constexpr char kUserId[] = "123";

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

  MOCK_METHOD(bool, IsAffiliated, (Profile * profile), (const, override));

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

struct TestCase {
  std::string test_name;
  bool is_affiliated;
  MetricReportingSettingData setting_data;
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
    telemetry_queue_ = std::make_unique<test::FakeMetricReportQueue>();

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
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfileManager profile_manager_;
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<MockDelegate> delegate_;
  std::unique_ptr<test::FakeMetricReportQueue> telemetry_queue_;
};

TEST_F(MetricReportingManagerLacrosTest, InitiallyDeprovisioned) {
  int periodic_collector_count = 0;

  ON_CALL(*delegate_, CreatePeriodicUploadReportQueue(
                          EventType::kUser, Destination::TELEMETRY_METRIC,
                          Priority::MANUAL_BATCH_LACROS, _, _, _, 1))
      .WillByDefault(Return(ByMove(std::move(telemetry_queue_))));

  ON_CALL(*delegate_, IsAffiliated(profile_.get())).WillByDefault(Return(true));

  ON_CALL(*delegate_, CheckDeviceDeprovisioned(_))
      .WillByDefault([](crosapi::mojom::DeviceSettingsService::
                            IsDeviceDeprovisionedCallback callback) {
        std::move(callback).Run(true);
      });

  ON_CALL(*delegate_, CreatePeriodicCollector)
      .WillByDefault([&periodic_collector_count]() {
        return std::make_unique<FakeCollector>(&periodic_collector_count);
      });

  auto* const delegate_ptr = delegate_.get();
  metrics::MetricReportingManagerLacros metric_reporting_manager(
      profile_.get(), std::move(delegate_));

  task_environment_.FastForwardBy(delegate_ptr->GetInitDelay());
  task_environment_.RunUntilIdle();
  EXPECT_EQ(periodic_collector_count, 0);
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
  auto* const telemetry_queue_ptr = telemetry_queue_.get();
  int periodic_collector_count = 0;

  ON_CALL(*delegate_, CreatePeriodicUploadReportQueue(
                          EventType::kUser, Destination::TELEMETRY_METRIC,
                          Priority::MANUAL_BATCH_LACROS, _, _, _, 1))
      .WillByDefault(Return(ByMove(std::move(telemetry_queue_))));

  ON_CALL(*delegate_, CreatePeriodicCollector(
                          _, telemetry_queue_ptr, _,
                          test_case.setting_data.enable_setting_path,
                          test_case.setting_data.setting_enabled_default_value,
                          test_case.setting_data.rate_setting_path, _,
                          test_case.setting_data.rate_unit_to_ms, _))
      .WillByDefault([&periodic_collector_count]() {
        return std::make_unique<FakeCollector>(&periodic_collector_count);
      });

  ON_CALL(*delegate_, IsAffiliated(profile_.get()))
      .WillByDefault(Return(test_case.is_affiliated));

  auto* const delegate_ptr = delegate_.get();
  metrics::MetricReportingManagerLacros metric_reporting_manager(
      profile_.get(), std::move(delegate_));

  task_environment_.FastForwardBy(delegate_ptr->GetInitDelay());
  task_environment_.RunUntilIdle();
  ASSERT_EQ(periodic_collector_count, test_case.expected_count);

  // Simulate device deprovisioning and verify collectors are destroyed.
  EXPECT_CALL(*delegate_ptr, CheckDeviceDeprovisioned(_))
      .WillRepeatedly([](crosapi::mojom::DeviceSettingsService::
                             IsDeviceDeprovisionedCallback callback) {
        std::move(callback).Run(true);
      });
  metric_reporting_manager.OnDeviceSettingsUpdated();
  task_environment_.RunUntilIdle();
  EXPECT_EQ(periodic_collector_count, 0);
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
}  // namespace
}  // namespace reporting
