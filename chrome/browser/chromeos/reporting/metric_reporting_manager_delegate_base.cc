// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/metric_reporting_manager_delegate_base.h"

#include <memory>

#include "base/logging.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "components/reporting/client/report_queue_factory.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/event_driven_telemetry_collector_pool.h"
#include "components/reporting/metrics/manual_collector.h"
#include "components/reporting/metrics/metric_event_observer.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/one_shot_collector.h"
#include "components/reporting/metrics/periodic_collector.h"
#include "components/reporting/util/rate_limiter_interface.h"
#include "components/reporting/util/rate_limiter_slide_window.h"

namespace reporting::metrics {
namespace {

std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
CreateReportQueue(EventType event_type,
                  Destination destination,
                  std::unique_ptr<RateLimiterInterface> rate_limiter,
                  std::optional<SourceInfo> source_info) {
  return ReportQueueFactory::CreateSpeculativeReportQueue(
      ReportQueueConfiguration::Create(
          {.event_type = event_type, .destination = destination})
          .SetRateLimiter(std::move(rate_limiter))
          .SetSourceInfo(std::move(source_info)));
}

}  // namespace

std::unique_ptr<MetricReportQueue>
MetricReportingManagerDelegateBase::CreateMetricReportQueue(
    EventType event_type,
    Destination destination,
    Priority priority,
    std::unique_ptr<RateLimiterInterface> rate_limiter,
    std::optional<SourceInfo> source_info) {
  std::unique_ptr<MetricReportQueue> metric_report_queue;
  auto report_queue = CreateReportQueue(
      event_type, destination, std::move(rate_limiter), std::move(source_info));
  if (report_queue) {
    metric_report_queue =
        std::make_unique<MetricReportQueue>(std::move(report_queue), priority);
  } else {
    LOG(ERROR) << "Cannot create metric report queue, report queue is null";
  }
  return metric_report_queue;
}

std::unique_ptr<MetricReportQueue>
MetricReportingManagerDelegateBase::CreatePeriodicUploadReportQueue(
    EventType event_type,
    Destination destination,
    Priority priority,
    ReportingSettings* reporting_settings,
    const std::string& rate_setting_path,
    base::TimeDelta default_rate,
    int rate_unit_to_ms,
    std::optional<SourceInfo> source_info) {
  std::unique_ptr<MetricReportQueue> metric_report_queue;
  auto report_queue =
      CreateReportQueue(event_type, destination, /*rate_limiter=*/nullptr,
                        std::move(source_info));
  if (report_queue) {
    metric_report_queue = std::make_unique<MetricReportQueue>(
        std::move(report_queue), priority, reporting_settings,
        rate_setting_path, default_rate, rate_unit_to_ms);
  } else {
    LOG(ERROR)
        << "Cannot create periodic upload report queue, report queue is null";
  }
  return metric_report_queue;
}

std::unique_ptr<CollectorBase>
MetricReportingManagerDelegateBase::CreatePeriodicCollector(
    Sampler* sampler,
    MetricReportQueue* metric_report_queue,
    ReportingSettings* reporting_settings,
    const std::string& enable_setting_path,
    bool setting_enabled_default_value,
    const std::string& rate_setting_path,
    base::TimeDelta default_rate,
    int rate_unit_to_ms,
    base::TimeDelta init_delay) {
  return std::make_unique<PeriodicCollector>(
      sampler, metric_report_queue, reporting_settings, enable_setting_path,
      setting_enabled_default_value, rate_setting_path, default_rate,
      rate_unit_to_ms, init_delay);
}

std::unique_ptr<CollectorBase>
MetricReportingManagerDelegateBase::CreateOneShotCollector(
    Sampler* sampler,
    MetricReportQueue* metric_report_queue,
    ReportingSettings* reporting_settings,
    const std::string& enable_setting_path,
    bool setting_enabled_default_value,
    base::TimeDelta init_delay) {
  return std::make_unique<OneShotCollector>(
      sampler, metric_report_queue, reporting_settings, enable_setting_path,
      setting_enabled_default_value, init_delay);
}

std::unique_ptr<CollectorBase>
MetricReportingManagerDelegateBase::CreateManualCollector(
    Sampler* sampler,
    MetricReportQueue* metric_report_queue,
    ReportingSettings* reporting_settings,
    const std::string& enable_setting_path,
    bool setting_enabled_default_value) {
  return std::make_unique<ManualCollector>(
      sampler, metric_report_queue, reporting_settings, enable_setting_path,
      setting_enabled_default_value);
}

std::unique_ptr<MetricEventObserverManager>
MetricReportingManagerDelegateBase::CreateEventObserverManager(
    std::unique_ptr<MetricEventObserver> event_observer,
    MetricReportQueue* metric_report_queue,
    ReportingSettings* reporting_settings,
    const std::string& enable_setting_path,
    bool setting_enabled_default_value,
    EventDrivenTelemetryCollectorPool* collector_pool,
    base::TimeDelta init_delay) {
  return std::make_unique<MetricEventObserverManager>(
      std::move(event_observer), metric_report_queue, reporting_settings,
      enable_setting_path, setting_enabled_default_value, collector_pool,
      init_delay);
}

std::unique_ptr<RateLimiterSlideWindow>
MetricReportingManagerDelegateBase::CreateSlidingWindowRateLimiter(
    size_t total_size,
    base::TimeDelta time_window,
    size_t bucket_count) {
  return std::make_unique<RateLimiterSlideWindow>(total_size, time_window,
                                                  bucket_count);
}

bool MetricReportingManagerDelegateBase::IsUserAffiliated(
    Profile& profile) const {
  return ::enterprise_util::IsProfileAffiliated(&profile);
}

base::TimeDelta MetricReportingManagerDelegateBase::GetInitDelay() const {
  return kInitialCollectionDelay;
}

base::TimeDelta MetricReportingManagerDelegateBase::GetInitialUploadDelay()
    const {
  return kInitialUploadDelay;
}

}  // namespace reporting::metrics
