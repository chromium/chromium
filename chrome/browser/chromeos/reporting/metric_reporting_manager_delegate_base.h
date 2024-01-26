// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_MANAGER_DELEGATE_BASE_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_MANAGER_DELEGATE_BASE_H_

#include <memory>
#include <optional>

#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "components/reporting/client/report_queue_configuration.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/event_driven_telemetry_collector_pool.h"
#include "components/reporting/metrics/metric_event_observer.h"
#include "components/reporting/metrics/metric_event_observer_manager.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/util/rate_limiter_interface.h"
#include "components/reporting/util/rate_limiter_slide_window.h"

namespace reporting::metrics {

// Base class for the delegate used by the `MetricReportingManager` to
// initialize metric related components.
class MetricReportingManagerDelegateBase {
 public:
  MetricReportingManagerDelegateBase() = default;
  MetricReportingManagerDelegateBase(
      const MetricReportingManagerDelegateBase& other) = delete;
  MetricReportingManagerDelegateBase& operator=(
      const MetricReportingManagerDelegateBase& other) = delete;
  virtual ~MetricReportingManagerDelegateBase() = default;

  // Creates a new `MetricReportQueue` that can be used towards metrics
  // reporting. Specify a `RateLimiterInterface` implementation to enforce rate
  // limiting.
  virtual std::unique_ptr<MetricReportQueue> CreateMetricReportQueue(
      EventType event_type,
      Destination destination,
      Priority priority,
      std::unique_ptr<RateLimiterInterface> rate_limiter = nullptr,
      std::optional<SourceInfo> source_info = std::nullopt);

  // Creates a new `MetricReportQueue` for periodic uploads. The rate is
  // controlled by the specified setting and we fall back to the defaults
  // specified if none set by policy.
  virtual std::unique_ptr<MetricReportQueue> CreatePeriodicUploadReportQueue(
      EventType event_type,
      Destination destination,
      Priority priority,
      ReportingSettings* reporting_settings,
      const std::string& rate_setting_path,
      base::TimeDelta default_rate,
      int rate_unit_to_ms = 1,
      std::optional<SourceInfo> source_info = std::nullopt);

  // Creates a new collector for periodic metric collection. The rate is
  // controlled by the specified setting and we fall back to the defaults
  // specified if none set by policy.
  virtual std::unique_ptr<CollectorBase> CreatePeriodicCollector(
      Sampler* sampler,
      MetricReportQueue* metric_report_queue,
      ReportingSettings* reporting_settings,
      const std::string& enable_setting_path,
      bool setting_enabled_default_value,
      const std::string& rate_setting_path,
      base::TimeDelta default_rate,
      int rate_unit_to_ms,
      base::TimeDelta init_delay = base::TimeDelta());

  // Creates a new collector for one shot metric collection. The rate is
  // controlled by the specified setting and we fall back to the defaults
  // specified if none set by policy.
  virtual std::unique_ptr<CollectorBase> CreateOneShotCollector(
      Sampler* sampler,
      MetricReportQueue* metric_report_queue,
      ReportingSettings* reporting_settings,
      const std::string& enable_setting_path,
      bool setting_enabled_default_value,
      base::TimeDelta init_delay = base::TimeDelta());

  // Creates a new collector for manual collection. Does not automatically
  // collect data upon construction or on any time period. Only collects if the
  // appropriate settings are enabled when manual collection happens.
  virtual std::unique_ptr<CollectorBase> CreateManualCollector(
      Sampler* sampler,
      MetricReportQueue* metric_report_queue,
      ReportingSettings* reporting_settings,
      const std::string& enable_setting_path,
      bool setting_enabled_default_value);

  // Creates a new event observer manager to manage events reporting. The rate
  // is controlled by the specified setting and we fall back to the defaults
  // specified if none set by policy.
  virtual std::unique_ptr<MetricEventObserverManager>
  CreateEventObserverManager(
      std::unique_ptr<MetricEventObserver> event_observer,
      MetricReportQueue* metric_report_queue,
      ReportingSettings* reporting_settings,
      const std::string& enable_setting_path,
      bool setting_enabled_default_value,
      EventDrivenTelemetryCollectorPool* collector_pool,
      base::TimeDelta init_delay = base::TimeDelta());

  // Creates a new instance of the sliding window rate limiter with the
  // specified total size, time window and bucket count.
  virtual std::unique_ptr<RateLimiterSlideWindow>
  CreateSlidingWindowRateLimiter(size_t total_size,
                                 base::TimeDelta time_window,
                                 size_t bucket_count);

  // Checks for profile affiliation and returns true if affiliated. False
  // otherwise.
  virtual bool IsUserAffiliated(Profile& profile) const;

  // Returns the delay interval used with `MetricReportingManager`
  // initialization.
  base::TimeDelta GetInitDelay() const;

  // Returns the delay interval used with initial record uploads.
  base::TimeDelta GetInitialUploadDelay() const;
};

}  // namespace reporting::metrics

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_METRIC_REPORTING_MANAGER_DELEGATE_BASE_H_
