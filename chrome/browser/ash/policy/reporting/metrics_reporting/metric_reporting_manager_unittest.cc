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
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/browser/profiles/profile.h"
#include "components/reporting/metrics/fake_metric_report_queue.h"
#include "components/reporting/metrics/fake_sampler.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace reporting {

class FakeDelegate : public MetricReportingManager::Delegate {
 public:
  FakeDelegate() = default;

  FakeDelegate(const FakeDelegate& other) = delete;
  FakeDelegate& operator=(const FakeDelegate& other) = delete;

  ~FakeDelegate() override = default;

  std::unique_ptr<MetricReportQueue> CreateInfoReportQueue() override {
    auto report_queue = std::make_unique<test::FakeMetricReportQueue>();
    info_queue_ = report_queue.get();
    return report_queue;
  }

  std::unique_ptr<MetricReportQueue> CreateEventReportQueue() override {
    auto report_queue = std::make_unique<test::FakeMetricReportQueue>();
    event_queue_ = report_queue.get();
    return report_queue;
  }

  std::unique_ptr<MetricReportQueue> CreateTelemetryReportQueue(
      ReportingSettings* reporting_settings,
      const std::string& rate_setting_path,
      base::TimeDelta default_rate) override {
    auto report_queue = std::make_unique<test::FakeMetricReportQueue>(
        Priority::MANUAL_BATCH, reporting_settings, rate_setting_path,
        default_rate);
    telemetry_queue_ = report_queue.get();
    return report_queue;
  }

  Sampler* AddSampler(std::unique_ptr<Sampler>) override {
    return Delegate::AddSampler(std::make_unique<test::FakeSampler>());
  }

  bool IsAffiliated(Profile* profile) override { return is_affiliated_; }

  void SetIsAffiliated(bool is_affiliated) { is_affiliated_ = is_affiliated; }

  test::FakeMetricReportQueue* GetInfoQueue() { return info_queue_; }

  test::FakeMetricReportQueue* GetEventQueue() { return event_queue_; }

  test::FakeMetricReportQueue* GetTelemetryQueue() { return telemetry_queue_; }

 private:
  bool is_affiliated_ = true;

  test::FakeMetricReportQueue* info_queue_;
  test::FakeMetricReportQueue* event_queue_;
  test::FakeMetricReportQueue* telemetry_queue_;
};

void NetworkCollectorsTestHelper(
    bool is_affiliated,
    const std::vector<base::Feature>& enabled_features,
    const std::vector<base::Feature>& disabled_features,
    bool telemetry_policy_enabled,
    int telemetry_collection_rate_ms,
    const base::TimeDelta time_forward,
    size_t expected_telemetry_reports_count) {
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(enabled_features, disabled_features);

  ash::ScopedTestingCrosSettings scoped_testing_cros_settings;
  scoped_testing_cros_settings.device_settings()->SetInteger(
      ash::kReportUploadFrequency, time_forward.InMilliseconds());
  scoped_testing_cros_settings.device_settings()->SetBoolean(
      ash::kReportDeviceNetworkStatus, telemetry_policy_enabled);
  scoped_testing_cros_settings.device_settings()->SetInteger(
      ash::kReportDeviceNetworkTelemetryCollectionRateMs,
      telemetry_collection_rate_ms);

  auto fake_delegate = std::make_unique<FakeDelegate>();
  fake_delegate->SetIsAffiliated(is_affiliated);
  auto* const fake_delegate_ptr = fake_delegate.get();
  auto metric_reporting_manager = MetricReportingManager::CreateForTesting(
      std::move(fake_delegate), nullptr);

  task_environment.FastForwardBy(time_forward);
  EXPECT_TRUE(
      fake_delegate_ptr->GetTelemetryQueue()->GetMetricDataReported().empty());

  metric_reporting_manager->OnLogin(nullptr);

  task_environment.FastForwardBy(time_forward);

  EXPECT_EQ(
      fake_delegate_ptr->GetTelemetryQueue()->GetMetricDataReported().size(),
      expected_telemetry_reports_count);
  EXPECT_EQ(fake_delegate_ptr->GetTelemetryQueue()->GetNumFlush(), 2);
}

TEST(MetricReportingManagerTest, NetworkCollectors_FeatureDisabled) {
  NetworkCollectorsTestHelper(
      /*is_affiliated=*/true,
      /*enabled_features=*/{},
      /*disabled_features=*/
      {MetricReportingManager::kEnableNetworkTelemetryReporting},
      /*telemetry_policy_enabled=*/true,
      /*telemetry_collection_rate_ms=*/60000,
      /*time_forward=*/base::Milliseconds(120000),
      /*expected_telemetry_reports_count=*/0ul);
}

TEST(MetricReportingManagerTest, NetworkCollectors_PolicyDisabled) {
  NetworkCollectorsTestHelper(
      /*is_affiliated=*/true,
      /*enabled_features=*/
      {MetricReportingManager::kEnableNetworkTelemetryReporting},
      /*disabled_features=*/{},
      /*telemetry_policy_enabled=*/false,
      /*telemetry_collection_rate_ms=*/60000,
      /*time_forward=*/base::Milliseconds(120000),
      /*expected_telemetry_reports_count=*/0ul);
}

TEST(MetricReportingManagerTest, NetworkCollectors_NotAffiliated) {
  NetworkCollectorsTestHelper(
      /*is_affiliated=*/false,
      /*enabled_features=*/
      {MetricReportingManager::kEnableNetworkTelemetryReporting},
      /*disabled_features=*/{},
      /*telemetry_policy_enabled=*/true,
      /*telemetry_collection_rate_ms=*/60000,
      /*time_forward=*/base::Milliseconds(120000),
      /*expected_telemetry_reports_count=*/0ul);
}

TEST(MetricReportingManagerTest, NetworkCollectors_Default) {
  NetworkCollectorsTestHelper(
      /*is_affiliated=*/true,
      /*enabled_features=*/
      {MetricReportingManager::kEnableNetworkTelemetryReporting},
      /*disabled_features=*/{},
      /*telemetry_policy_enabled=*/true,
      /*telemetry_collection_rate_ms=*/60000,
      /*time_forward=*/base::Milliseconds(120000),
      /*expected_telemetry_reports_count=*/2ul);
}
}  // namespace reporting
