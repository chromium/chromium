// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/system_monitor.h"

#include <algorithm>
#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/performance_monitor/metric_evaluator_helper_win.h"
#elif BUILDFLAG(IS_POSIX)
#include "chrome/browser/performance_monitor/metric_evaluator_helper_posix.h"
#endif

namespace performance_monitor {

namespace {

using MetricRefreshFrequencies =
    SystemMonitor::SystemObserver::MetricRefreshFrequencies;

// The global instance.
SystemMonitor* g_system_metrics_monitor = nullptr;

// The default interval at which the metrics are refreshed.
constexpr base::TimeDelta kDefaultRefreshInterval = base::Seconds(2);

}  // namespace

SystemMonitor::SystemMonitor(
    std::unique_ptr<MetricEvaluatorsHelper> metric_evaluators_helper)
    : blocking_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(),
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN})),
      metric_evaluators_helper_(
          metric_evaluators_helper.release(),
          base::OnTaskRunnerDeleter(blocking_task_runner_)),
      metric_evaluators_metadata_(CreateMetricMetadataArray()) {
  DCHECK(!g_system_metrics_monitor);
  g_system_metrics_monitor = this;
}

SystemMonitor::~SystemMonitor() {
  DCHECK_EQ(this, g_system_metrics_monitor);
  g_system_metrics_monitor = nullptr;
}

// static
std::unique_ptr<SystemMonitor> SystemMonitor::Create() {
  DCHECK(!g_system_metrics_monitor);
  return base::WrapUnique(new SystemMonitor(CreateMetricEvaluatorsHelper()));
}

// static
std::unique_ptr<SystemMonitor> SystemMonitor::CreateForTesting(
    std::unique_ptr<MetricEvaluatorsHelper> helper) {
  DCHECK(!g_system_metrics_monitor);
  return base::WrapUnique(new SystemMonitor(std::move(helper)));
}

// static
SystemMonitor* SystemMonitor::Get() {
  return g_system_metrics_monitor;
}

MetricRefreshFrequencies::Builder&
MetricRefreshFrequencies::Builder::SetFreePhysMemoryMbFrequency(
    SamplingFrequency freq) {
  metrics_and_frequencies_.free_phys_memory_mb_frequency = freq;
  return *this;
}

MetricRefreshFrequencies::Builder&
MetricRefreshFrequencies::Builder::SetSystemMetricsSamplingFrequency(
    SamplingFrequency freq) {
  metrics_and_frequencies_.system_metrics_sampling_frequency = freq;
  return *this;
}

MetricRefreshFrequencies MetricRefreshFrequencies::Builder::Build() {
  return metrics_and_frequencies_;
}

SystemMonitor::SystemObserver::~SystemObserver() {
  if (g_system_metrics_monitor) {
    // This is a no-op if the observer has already been removed.
    g_system_metrics_monitor->RemoveObserver(this);
  }
}

void SystemMonitor::SystemObserver::OnFreePhysicalMemoryMbSample(
    int free_phys_memory_mb) {
  NOTREACHED_IN_MIGRATION();
}

void SystemMonitor::SystemObserver::OnSystemMetricsStruct(
    const base::SystemMetrics& system_metrics) {
  NOTREACHED_IN_MIGRATION();
}

void SystemMonitor::AddOrUpdateObserver(
    SystemMonitor::SystemObserver* observer,
    MetricRefreshFrequencies metrics_and_frequencies) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!observers_.HasObserver(observer))
    observers_.AddObserver(observer);
  observer_metrics_[observer] = std::move(metrics_and_frequencies);
  UpdateObservedMetrics();
}

void SystemMonitor::RemoveObserver(SystemMonitor::SystemObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
  if (observer_metrics_.erase(observer))
    UpdateObservedMetrics();
}

SystemMonitor::MetricVector SystemMonitor::GetMetricsToEvaluate() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  SystemMonitor::MetricVector metrics_to_evaluate;

  for (size_t i = 0; i < metrics_refresh_frequencies_.size(); ++i) {
    if (metrics_refresh_frequencies_[i] == SamplingFrequency::kNoSampling)
      continue;
    metrics_to_evaluate.emplace_back(
        metric_evaluators_metadata_[i].create_metric_evaluator_function(
            metric_evaluators_helper_.get()));
  }
  return metrics_to_evaluate;
}

// static
SystemMonitor::MetricVector SystemMonitor::EvaluateMetrics(
    SystemMonitor::MetricVector metrics_to_evaluate) {
  for (auto& metric : metrics_to_evaluate)
    metric->Evaluate();

  return metrics_to_evaluate;
}

SystemMonitor::MetricMetadataArray SystemMonitor::CreateMetricMetadataArray() {
#define CREATE_METRIC_METADATA(metric_type, value_type, helper_function, \
                               notify_function, metric_freq_field)       \
  MetricMetadata(                                                        \
      [](MetricEvaluatorsHelper* helper) {                               \
        std::unique_ptr<MetricEvaluator> metric =                        \
            base::WrapUnique(new MetricEvaluatorImpl<value_type>(        \
                MetricEvaluator::Type::metric_type,                      \
                base::BindOnce(&MetricEvaluatorsHelper::helper_function, \
                               base::Unretained(helper)),                \
                &SystemObserver::notify_function));                      \
        return metric;                                                   \
      },                                                                 \
      [](const SystemObserver::MetricRefreshFrequencies&                 \
             metric_refresh_frequencies) {                               \
        return metric_refresh_frequencies.metric_freq_field;             \
      })

  return {
      CREATE_METRIC_METADATA(kFreeMemoryMb, int, GetFreePhysicalMemoryMb,
                             OnFreePhysicalMemoryMbSample,
                             free_phys_memory_mb_frequency),
      CREATE_METRIC_METADATA(kSystemMetricsStruct, base::SystemMetrics,
                             GetSystemMetricsStruct, OnSystemMetricsStruct,
                             system_metrics_sampling_frequency),
  };

#undef CREATE_METRIC_METADATA
}  // namespace performance_monitor

void SystemMonitor::UpdateObservedMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::TimeDelta refresh_interval = base::TimeDelta::Max();
  // Iterates over the |observer_metrics_| list to find the highest refresh
  // frequency for each metric.
  for (size_t i = 0; i < metrics_refresh_frequencies_.size(); ++i) {
    metrics_refresh_frequencies_[i] = SamplingFrequency::kNoSampling;
    for (const auto& obs_iter : observer_metrics_) {
      metrics_refresh_frequencies_[i] = std::max(
          metrics_refresh_frequencies_[i],
          metric_evaluators_metadata_[i].get_refresh_frequency_field_function(
              obs_iter.second));
    }
    if (metrics_refresh_frequencies_[i] != SamplingFrequency::kNoSampling)
      refresh_interval = kDefaultRefreshInterval;
  }

  if (refresh_interval.is_max()) {
    refresh_timer_.Stop();
  } else if (!refresh_timer_.IsRunning() ||
             refresh_interval != refresh_timer_.GetCurrentDelay()) {
    refresh_timer_.Start(FROM_HERE, refresh_interval,
                         base::BindOnce(&SystemMonitor::RefreshCallback,
                                        base::Unretained(this)));
  }
}

void SystemMonitor::RefreshCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  blocking_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&SystemMonitor::EvaluateMetrics, GetMetricsToEvaluate()),
      base::BindOnce(&SystemMonitor::NotifyObservers,
                     weak_factory_.GetWeakPtr()));

  refresh_timer_.Start(
      FROM_HERE, refresh_timer_.GetCurrentDelay(),
      base::BindOnce(&SystemMonitor::RefreshCallback, base::Unretained(this)));
}

void SystemMonitor::NotifyObservers(SystemMonitor::MetricVector metrics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Iterate over the observers and notify them if a metric has been refreshed
  // at the requested frequency.
  for (const auto& metric : metrics) {
    if (!metric->has_value())
      continue;
    for (auto& observer : observers_) {
      const auto& iter = observer_metrics_.find(&observer);
      CHECK(iter != observer_metrics_.end(), base::NotFatalUntil::M130);
      if (metric_evaluators_metadata_[static_cast<size_t>(metric->type())]
              .get_refresh_frequency_field_function(iter->second) !=
          SystemMonitor::SamplingFrequency::kNoSampling) {
        metric->NotifyObserver(iter->first);
      }
    }
  }
}

// static
std::unique_ptr<MetricEvaluatorsHelper>
SystemMonitor::CreateMetricEvaluatorsHelper() {
#if BUILDFLAG(IS_WIN)
  return base::WrapUnique(new MetricEvaluatorsHelperWin());
#elif BUILDFLAG(IS_POSIX)
  return std::make_unique<MetricEvaluatorsHelperPosix>();
#else
#error Unsupported platform
#endif
}

SystemMonitor::MetricEvaluator::MetricEvaluator(Type type) : type_(type) {}
SystemMonitor::MetricEvaluator::~MetricEvaluator() = default;

template <typename T>
SystemMonitor::MetricEvaluatorImpl<T>::MetricEvaluatorImpl(
    Type type,
    base::OnceCallback<std::optional<T>()> evaluate_function,
    void (SystemObserver::*notify_function)(ObserverArgType))
    : MetricEvaluator(type),
      evaluate_function_(std::move(evaluate_function)),
      notify_function_(notify_function) {}

template <typename T>
SystemMonitor::MetricEvaluatorImpl<T>::~MetricEvaluatorImpl() = default;

SystemMonitor::MetricMetadata::MetricMetadata(
    std::unique_ptr<MetricEvaluator> (*create_function)(
        MetricEvaluatorsHelper* helper),
    SamplingFrequency (*get_refresh_field_function)(
        const SystemMonitor::SystemObserver::MetricRefreshFrequencies&))
    : create_metric_evaluator_function(create_function),
      get_refresh_frequency_field_function(get_refresh_field_function) {}

template <typename T>
void SystemMonitor::MetricEvaluatorImpl<T>::NotifyObserver(
    SystemObserver* observer) {
  DCHECK(value());
  (observer->*notify_function_)(value().value());
}

template <typename T>
void SystemMonitor::MetricEvaluatorImpl<T>::Evaluate() {
  DCHECK(evaluate_function_);
  value_ = std::move(evaluate_function_).Run();
}

std::optional<base::SystemMetrics>
MetricEvaluatorsHelper::GetSystemMetricsStruct() {
  return base::SystemMetrics::Sample();
}

}  // namespace performance_monitor
