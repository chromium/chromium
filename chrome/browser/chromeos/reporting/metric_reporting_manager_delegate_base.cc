// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/metric_reporting_manager_delegate_base.h"

#include "base/time/time.h"
#include "chrome/browser/chromeos/reporting/metric_default_utils.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "components/reporting/client/report_queue_factory.h"
#include "components/reporting/metrics/collector_base.h"
#include "components/reporting/metrics/event_driven_telemetry_collector_pool.h"
#include "components/reporting/metrics/metric_event_observer.h"
#include "components/reporting/metrics/metric_report_queue.h"
#include "components/reporting/metrics/one_shot_collector.h"
#include "components/reporting/metrics/periodic_collector.h"

namespace reporting::metrics {
namespace {

std::unique_ptr<::reporting::ReportQueue, base::OnTaskRunnerDeleter>
CreateReportQueue(EventType event_type, Destination destination) {
  return ReportQueueFactory::CreateSpeculativeReportQueue(event_type,
                                                          destination);
}

}  // namespace

std::unique_ptr<MetricReportQueue>
MetricReportingManagerDelegateBase::CreateMetricReportQueue(
    EventType event_type,
    Destination destination,
    Priority priority) {
  std::unique_ptr<MetricReportQueue> metric_report_queue;
  auto report_queue = CreateReportQueue(event_type, destination);
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
    int rate_unit_to_ms) {
  std::unique_ptr<MetricReportQueue> metric_report_queue;
  auto report_queue = CreateReportQueue(event_type, destination);
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

bool MetricReportingManagerDelegateBase::IsAffiliated(Profile* profile) const {
  return ::chrome::enterprise_util::IsProfileAffiliated(profile);
}

base::TimeDelta MetricReportingManagerDelegateBase::GetInitDelay() const {
  return InitDelayParam::Get();
}

base::TimeDelta MetricReportingManagerDelegateBase::GetInitialUploadDelay()
    const {
  return kInitialUploadDelay;
}

}  // namespace reporting::metrics
