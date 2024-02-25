// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/websites/website_usage_telemetry_periodic_collector_lacros.h"

#include <memory>
#include <string_view>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/reporting/device_reporting_settings_lacros.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_manager_lacros.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_manager_lacros_factory.h"
#include "chrome/browser/chromeos/reporting/metric_reporting_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/reporting/metrics/fakes/fake_metric_report_queue.h"
#include "components/reporting/metrics/fakes/fake_reporting_settings.h"
#include "components/reporting/metrics/fakes/fake_sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Eq;

namespace reporting {
namespace {

constexpr std::string_view kTestUserId = "123";
constexpr base::TimeDelta kCollectionInterval = base::Minutes(30);

// Fake delegate implementation for the `MetricReportingManagerLacros`
// component. Used with the `MetricReportingManagerLacrosFactory` to block
// initialization of downstream components and simplify testing.
class FakeDelegate : public metrics::MetricReportingManagerLacros::Delegate {
 public:
  FakeDelegate() = default;
  FakeDelegate(const FakeDelegate& other) = delete;
  FakeDelegate& operator=(const FakeDelegate& other) = delete;
  ~FakeDelegate() override = default;

  bool IsUserAffiliated(Profile& profile) const override { return false; }

  std::unique_ptr<DeviceReportingSettingsLacros> CreateDeviceReportingSettings()
      override {
    return std::unique_ptr<DeviceReportingSettingsLacros>(nullptr);
  }
};

class WebsiteUsageTelemetryPeriodicCollectorLacrosTest
    : public ::testing::Test {
 protected:
  WebsiteUsageTelemetryPeriodicCollectorLacrosTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    dependency_manager_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &WebsiteUsageTelemetryPeriodicCollectorLacrosTest::
                    SetTestingFactory,
                base::Unretained(this)));

    // Set up main user profile. Used to monitor `MetricReportingManagerLacros`
    // component shutdown.
    profile_ = profile_manager_.CreateTestingProfile(std::string{kTestUserId});
    profile_->SetIsMainProfile(true);
  }

  void SetTestingFactory(::content::BrowserContext* context) {
    metrics::MetricReportingManagerLacrosFactory::GetInstance()
        ->SetTestingFactory(
            context, base::BindRepeating(
                         &WebsiteUsageTelemetryPeriodicCollectorLacrosTest::
                             CreateMetricReportingManager,
                         base::Unretained(this)));
  }

  std::unique_ptr<KeyedService> CreateMetricReportingManager(
      ::content::BrowserContext* context) {
    auto fake_delegate = std::make_unique<FakeDelegate>();
    return std::make_unique<metrics::MetricReportingManagerLacros>(
        static_cast<Profile*>(context), std::move(fake_delegate));
  }

  void AssertDataIsReported() {
    ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(1));
    const auto enqueued_metric_data =
        metric_report_queue_.GetMetricDataReported();
    EXPECT_TRUE(enqueued_metric_data.has_timestamp_ms());
    EXPECT_TRUE(enqueued_metric_data.has_telemetry_data());
  }

  ::content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingProfileManager profile_manager_;
  base::CallbackListSubscription dependency_manager_subscription_;
  raw_ptr<TestingProfile> profile_;
  test::FakeReportingSettings reporting_settings_;
  test::FakeSampler sampler_;
  test::FakeMetricReportQueue metric_report_queue_;
};

TEST_F(WebsiteUsageTelemetryPeriodicCollectorLacrosTest,
       CollectMetricDataWithDefaultRate) {
  MetricData metric_data;
  metric_data.mutable_telemetry_data();
  sampler_.SetMetricData(std::move(metric_data));

  const WebsiteUsageTelemetryPeriodicCollectorLacros
      website_usage_telemetry_periodic_collector(profile_.get(), &sampler_,
                                                 &metric_report_queue_,
                                                 &reporting_settings_);
  ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(0));

  // Advance timer and verify data is collected.
  task_environment_.FastForwardBy(
      metrics::kDefaultWebsiteTelemetryCollectionRate);
  AssertDataIsReported();
}

TEST_F(WebsiteUsageTelemetryPeriodicCollectorLacrosTest,
       CollectMetricDataWithRateSetting) {
  reporting_settings_.SetInteger(kReportWebsiteTelemetryCollectionRateMs,
                                 kCollectionInterval.InMilliseconds());
  MetricData metric_data;
  metric_data.mutable_telemetry_data();
  sampler_.SetMetricData(std::move(metric_data));

  const WebsiteUsageTelemetryPeriodicCollectorLacros
      website_usage_telemetry_periodic_collector(profile_.get(), &sampler_,
                                                 &metric_report_queue_,
                                                 &reporting_settings_);
  ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(0));

  // Advance timer and verify data is collected.
  task_environment_.FastForwardBy(kCollectionInterval);
  AssertDataIsReported();
}

TEST_F(WebsiteUsageTelemetryPeriodicCollectorLacrosTest, NoMetricData) {
  reporting_settings_.SetInteger(kReportWebsiteTelemetryCollectionRateMs,
                                 kCollectionInterval.InMilliseconds());
  const WebsiteUsageTelemetryPeriodicCollectorLacros
      website_usage_telemetry_periodic_collector(profile_.get(), &sampler_,
                                                 &metric_report_queue_,
                                                 &reporting_settings_);
  ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(0));

  // Advance timer and verify no data is reported.
  task_environment_.FastForwardBy(kCollectionInterval);
  ASSERT_TRUE(metric_report_queue_.IsEmpty());
}

TEST_F(WebsiteUsageTelemetryPeriodicCollectorLacrosTest,
       OnCollectorDestruction) {
  reporting_settings_.SetInteger(kReportWebsiteTelemetryCollectionRateMs,
                                 kCollectionInterval.InMilliseconds());
  MetricData metric_data;
  metric_data.mutable_telemetry_data();
  sampler_.SetMetricData(std::move(metric_data));

  // Set up periodic collector and destroy collector before triggering
  // collection.
  auto website_usage_telemetry_periodic_collector =
      std::make_unique<WebsiteUsageTelemetryPeriodicCollectorLacros>(
          profile_.get(), &sampler_, &metric_report_queue_,
          &reporting_settings_);
  website_usage_telemetry_periodic_collector.reset();

  // Advance timer and verify no data is collected.
  task_environment_.FastForwardBy(kCollectionInterval);
  ASSERT_THAT(sampler_.GetNumCollectCalls(), Eq(0));
  ASSERT_TRUE(metric_report_queue_.IsEmpty());
}

TEST_F(WebsiteUsageTelemetryPeriodicCollectorLacrosTest,
       OnMetricReportingManagerShutdown) {
  reporting_settings_.SetInteger(kReportWebsiteTelemetryCollectionRateMs,
                                 kCollectionInterval.InMilliseconds());
  MetricData metric_data;
  metric_data.mutable_telemetry_data();
  sampler_.SetMetricData(std::move(metric_data));
  const WebsiteUsageTelemetryPeriodicCollectorLacros
      website_usage_telemetry_periodic_collector(profile_.get(), &sampler_,
                                                 &metric_report_queue_,
                                                 &reporting_settings_);

  // Delete profile to trigger metric reporting manager shutdown and verify data
  // is collected.
  profile_ = nullptr;
  profile_manager_.DeleteAllTestingProfiles();
  AssertDataIsReported();
}

}  // namespace
}  // namespace reporting
