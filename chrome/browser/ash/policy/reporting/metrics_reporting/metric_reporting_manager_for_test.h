// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_MANAGER_FOR_TEST_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_MANAGER_FOR_TEST_H_

#include <memory>

#include "chrome/browser/ash/policy/reporting/metrics_reporting/metric_reporting_manager.h"
#include "components/reporting/metrics/fakes/fake_metric_report_queue.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace reporting::test {

// Report manager mock delegate
class MockDelegate : public MetricReportingManager::Delegate {
 public:
  MockDelegate();

  MockDelegate(const MockDelegate& other) = delete;
  MockDelegate& operator=(const MockDelegate& other) = delete;

  ~MockDelegate() override;

  MOCK_METHOD(bool, IsUserAffiliated, (Profile & profile), (const, override));

  MOCK_METHOD(bool, IsDeprovisioned, (), (const, override));

  MOCK_METHOD(std::unique_ptr<MetricReportQueue>,
              CreateMetricReportQueue,
              (EventType event_type,
               Destination destination,
               Priority priority,
               std::unique_ptr<RateLimiterInterface> rate_limiter,
               std::optional<SourceInfo> source_info),
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

// Manager class extension for testing purposes.
class MetricReportingManagerForTest : public MetricReportingManager {
 public:
  static std::unique_ptr<MetricReportingManagerForTest> Create(
      std::unique_ptr<Delegate> delegate,
      policy::ManagedSessionService* managed_session_service);

  ~MetricReportingManagerForTest() override;

  FakeMetricReportQueue* info_queue() const;
  FakeMetricReportQueue* telemetry_queue() const;
  FakeMetricReportQueue* user_telemetry_queue() const;
  FakeMetricReportQueue* event_queue() const;
  FakeMetricReportQueue* crash_event_queue() const;
  FakeMetricReportQueue* chrome_crash_event_queue() const;
  FakeMetricReportQueue* user_event_queue() const;
  FakeMetricReportQueue* app_event_queue() const;
  FakeMetricReportQueue* website_event_queue() const;
  FakeMetricReportQueue* user_peripheral_events_and_telemetry_queue() const;
  FakeMetricReportQueue* kiosk_heartbeat_telemetry_queue() const;

 private:
  explicit MetricReportingManagerForTest(std::unique_ptr<Delegate> delegate);
};
}  // namespace reporting::test

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_METRIC_REPORTING_MANAGER_FOR_TEST_H_
