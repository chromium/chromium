// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/metrics/page_resource_monitor.h"

#include <stdint.h>
#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/resource_attribution/cpu_measurement_delegate.h"
#include "components/system_cpu/cpu_probe.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager::metrics {

namespace {

using system_cpu::CpuProbe;
using system_cpu::PressureSample;
using PageMeasurementBackgroundState =
    PageResourceMonitor::PageMeasurementBackgroundState;

// CPU usage metrics are provided as a double in the [0.0, number of cores *
// 100.0] range. The CPU usage is usually below 1%, so the UKM is
// reported out of 10,000 instead of out of 100 to make analyzing the data
// easier. This is the same scale factor used by the
// PerformanceMonitor.AverageCPU8 histograms recorded in
// chrome/browser/metrics/power/process_metrics_recorder_util.cc.
constexpr int kCPUUsageFactor = 100 * 100;

// The values for n when calculating the total CPU usage of the top n tabs.
constexpr std::array<size_t, 5> kTabCountSlices = {1, 2, 4, 8, 16};

// The time between calls to CollectPageResourceUsage()
constexpr base::TimeDelta kCollectionDelay = base::Minutes(2);

PageMeasurementBackgroundState GetBackgroundStateForMeasurementPeriod(
    const PageNode* page_node,
    base::TimeDelta time_since_last_measurement) {
  if (page_node->GetTimeSinceLastVisibilityChange() <
      time_since_last_measurement) {
    return PageMeasurementBackgroundState::kMixedForegroundBackground;
  }
  if (page_node->IsVisible()) {
    return PageMeasurementBackgroundState::kForeground;
  }
  // Check if the page was audible for the entire measurement period.
  if (page_node->GetTimeSinceLastAudibleChange().value_or(
          base::TimeDelta::Max()) < time_since_last_measurement) {
    return PageMeasurementBackgroundState::kBackgroundMixedAudible;
  }
  if (page_node->IsAudible()) {
    return PageMeasurementBackgroundState::kAudibleInBackground;
  }
  return PageMeasurementBackgroundState::kBackground;
}

}  // namespace

PageResourceMonitor::PageResourceMonitor(bool enable_system_cpu_probe)
    : system_cpu_probe_(enable_system_cpu_probe ? CpuProbe::Create()
                                                : nullptr) {
  collect_page_resource_usage_timer_.Start(
      FROM_HERE, kCollectionDelay,
      base::BindRepeating(&PageResourceMonitor::CollectPageResourceUsage,
                          weak_factory_.GetWeakPtr()));
  if (system_cpu_probe_) {
    system_cpu_probe_->StartSampling();
  }
}

PageResourceMonitor::~PageResourceMonitor() = default;

void PageResourceMonitor::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph_ = graph;
  cpu_monitor_.StartMonitoring(graph_);
}

void PageResourceMonitor::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cpu_monitor_.StopMonitoring(graph_);
  graph_ = nullptr;
}

base::TimeDelta PageResourceMonitor::GetCollectionDelayForTesting() const {
  return kCollectionDelay;
}

base::TimeDelta PageResourceMonitor::GetDelayedMetricsTimeoutForTesting()
    const {
  return performance_manager::features::kDelayBeforeLogging.Get();
}

void PageResourceMonitor::SetCPUMeasurementDelegateFactoryForTesting(
    Graph* graph,
    PageResourceCPUMonitor::CPUMeasurementDelegate::Factory* factory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Callback should be installed before `cpu_monitor_` starts
  // measuring the graph.
  CHECK(graph);
  CHECK(!graph_);
  cpu_monitor_.SetCPUMeasurementDelegateFactoryForTesting(  // IN-TEST
      graph, factory);
}

void PageResourceMonitor::CollectPageResourceUsage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CalculatePageCPUUsage(
      /*use_delayed_system_cpu_probe=*/false,
      base::BindOnce(&PageResourceMonitor::OnPageResourceUsageResult,
                     weak_factory_.GetWeakPtr()));
}

void PageResourceMonitor::OnPageResourceUsageResult(
    const PageCPUUsageVector& page_cpu_usage,
    absl::optional<PressureSample> system_cpu) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Calculate the overall CPU usage.
  double total_cpu_usage = 0;
  for (const auto& [page_context, cpu_usage] : page_cpu_usage) {
    total_cpu_usage += cpu_usage;
  }

  const auto now = base::TimeTicks::Now();
  for (const auto& [page_context, cpu_usage] : page_cpu_usage) {
    const PageNode* page_node = page_context.GetPageNode();
    if (!page_node) {
      // Page was deleted while waiting for system CPU. Nothing to log.
      continue;
    }
    const ukm::SourceId source_id = page_node->GetUkmSourceID();
    ukm::builders::PerformanceManager_PageResourceUsage2(source_id)
        .SetResidentSetSizeEstimate(page_node->EstimateResidentSetSize())
        .SetPrivateFootprintEstimate(page_node->EstimatePrivateFootprintSize())
        .SetRecentCPUUsage(kCPUUsageFactor * cpu_usage)
        .SetTotalRecentCPUUsageAllPages(kCPUUsageFactor * total_cpu_usage)
        .SetBackgroundState(
            static_cast<int64_t>(GetBackgroundStateForMeasurementPeriod(
                page_node, now - time_of_last_resource_usage_)))
        .Record(ukm::UkmRecorder::Get());
  }
  time_of_last_resource_usage_ = now;

#if !BUILDFLAG(IS_ANDROID)
  if (base::FeatureList::IsEnabled(
          performance_manager::features::kCPUInterventionEvaluationLogging)) {
    LogCPUInterventionMetrics(page_cpu_usage, system_cpu, now,
                              CPUInterventionSuffix::kBaseline);
    bool is_cpu_over_threshold =
        (100 * total_cpu_usage / base::SysInfo::NumberOfProcessors() >
         performance_manager::features::kThresholdChromeCPUPercent.Get());
    if (!time_of_last_cpu_threshold_exceeded_.has_value()) {
      CHECK(!log_cpu_on_delay_timer_.IsRunning());
      if (is_cpu_over_threshold) {
        time_of_last_cpu_threshold_exceeded_ = now;
        LogCPUInterventionMetrics(page_cpu_usage, system_cpu, now,
                                  CPUInterventionSuffix::kImmediate);

        // Only logged delayed metrics when using the new CPU monitor.
        if (performance_manager::features::kUseResourceAttributionCPUMonitor
                .Get()) {
          if (system_cpu_probe_) {
            // `system_cpu_probe_` needs to be called at fixed intervals, so
            // start a second probe  to measure the CPU until the delay timer
            // fires.
            CHECK(!delayed_system_cpu_probe_);
            delayed_system_cpu_probe_ = CpuProbe::Create();
            delayed_system_cpu_probe_->StartSampling();
          }
          log_cpu_on_delay_timer_.Start(
              FROM_HERE,
              performance_manager::features::kDelayBeforeLogging.Get(), this,
              &PageResourceMonitor::CheckDelayedCPUInterventionMetrics);
        }
      }
    } else if (!is_cpu_over_threshold) {
      base::UmaHistogramCustomTimes(
          "PerformanceManager.PerformanceInterventions.CPU."
          "DurationOverThreshold",
          now - time_of_last_cpu_threshold_exceeded_.value(), base::Minutes(2),
          base::Hours(24), 50);
      log_cpu_on_delay_timer_.AbandonAndStop();
      time_of_last_cpu_threshold_exceeded_ = absl::nullopt;
      delayed_system_cpu_probe_.reset();
    }
  }
#endif
}

void PageResourceMonitor::CheckDelayedCPUInterventionMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(performance_manager::features::kUseResourceAttributionCPUMonitor.Get());
  CalculatePageCPUUsage(
      /*use_delayed_system_cpu_probe=*/true,
      base::BindOnce(
          &PageResourceMonitor::OnDelayedCPUInterventionMetricsResult,
          weak_factory_.GetWeakPtr()));
}

void PageResourceMonitor::OnDelayedCPUInterventionMetricsResult(
    const PageCPUUsageVector& page_cpu_usage,
    absl::optional<PressureSample> system_cpu) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(performance_manager::features::kUseResourceAttributionCPUMonitor.Get());
  // Now that `system_cpu` is received, stop the delayed CPU probe. This is a
  // no-op if it was already nullptr.
  delayed_system_cpu_probe_.reset();
  double total_cpu_usage = 0;
  for (const auto& [page_context, cpu_usage] : page_cpu_usage) {
    total_cpu_usage += cpu_usage;
  }

  if (100 * total_cpu_usage / base::SysInfo::NumberOfProcessors() >
      performance_manager::features::kThresholdChromeCPUPercent.Get()) {
    // Still over the threshold so we should log .Delayed UMA metrics.
    LogCPUInterventionMetrics(page_cpu_usage, system_cpu,
                              base::TimeTicks::Now(),
                              CPUInterventionSuffix::kDelayed);
  }
}

void PageResourceMonitor::LogCPUInterventionMetrics(
    const PageCPUUsageVector& page_cpu_usage,
    const absl::optional<PressureSample>& system_cpu,
    const base::TimeTicks now,
    CPUInterventionSuffix histogram_suffix) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<double> background_cpu_usage;
  double total_foreground_cpu_usage = 0;

  int foreground_tab_count = 0;
  int background_tab_count = 0;

  for (const auto& [page_context, cpu_usage] : page_cpu_usage) {
    const PageNode* page_node = page_context.GetPageNode();
    if (!page_node) {
      // Page was deleted while waiting for system CPU.
      continue;
    }
    if (GetBackgroundStateForMeasurementPeriod(
            page_node, now - time_of_last_resource_usage_) !=
        PageMeasurementBackgroundState::kForeground) {
      background_cpu_usage.emplace_back(cpu_usage);
      background_tab_count++;
    } else {
      total_foreground_cpu_usage += cpu_usage;
      foreground_tab_count++;
    }
  }

  double total_background_cpu_usage = std::accumulate(
      background_cpu_usage.begin(), background_cpu_usage.end(), 0.0);

  // Log basic background UMA metrics.
  const char* suffix = nullptr;
  switch (histogram_suffix) {
    case CPUInterventionSuffix::kBaseline:
      suffix = "Baseline";
      break;
    case CPUInterventionSuffix::kImmediate:
      suffix = "Immediate";
      break;
    case CPUInterventionSuffix::kDelayed:
      suffix = "Delayed";
      break;
  }
  CHECK(suffix);

  const int total_background_cpu_percent =
      total_background_cpu_usage * 100 / base::SysInfo::NumberOfProcessors();
  base::UmaHistogramPercentage(
      base::StrCat({"PerformanceManager.PerformanceInterventions.CPU."
                    "TotalBackgroundCPU.",
                    suffix}),
      total_background_cpu_percent);
  base::UmaHistogramCounts1000(
      base::StrCat({"PerformanceManager.PerformanceInterventions.CPU."
                    "TotalBackgroundTabCount.",
                    suffix}),
      background_tab_count);
  if (background_tab_count) {
    base::UmaHistogramPercentage(
        base::StrCat({"PerformanceManager.PerformanceInterventions.CPU."
                      "AverageBackgroundCPU.",
                      suffix}),
        total_background_cpu_percent / background_tab_count);
  }

  // Log basic foreground UMA metrics.
  const int total_foreground_cpu_percent =
      total_foreground_cpu_usage * 100 / base::SysInfo::NumberOfProcessors();
  base::UmaHistogramPercentage(
      base::StrCat({"PerformanceManager.PerformanceInterventions.CPU."
                    "TotalForegroundCPU.",
                    suffix}),
      total_foreground_cpu_percent);
  base::UmaHistogramCounts1000(
      base::StrCat({"PerformanceManager.PerformanceInterventions.CPU."
                    "TotalForegroundTabCount.",
                    suffix}),
      foreground_tab_count);
  if (foreground_tab_count) {
    base::UmaHistogramPercentage(
        base::StrCat({"PerformanceManager.PerformanceInterventions.CPU."
                      "AverageForegroundCPU.",
                      suffix}),
        total_foreground_cpu_percent / foreground_tab_count);
  }

  // Log comparisons with system CPU.
  if (system_cpu.has_value()) {
    const int system_cpu_percent = system_cpu->cpu_utilization * 100;
    base::UmaHistogramPercentage(
        base::StrCat({"PerformanceManager.PerformanceInterventions.CPU."
                      "System.",
                      suffix}),
        system_cpu_percent);
    base::UmaHistogramPercentage(
        base::StrCat({"PerformanceManager.PerformanceInterventions.CPU."
                      "NonChrome.",
                      suffix}),
        std::max(system_cpu_percent - total_background_cpu_percent -
                     total_foreground_cpu_percent,
                 0));
  } else if (system_cpu_probe_) {
    // System CPU probing is available, so there must have been an error in
    // CpuProbe::RequestSample().
    //
    // For .Delayed histograms this can also include a failure to initialize
    // `delayed_system_cpu_probe_` when `system_cpu_probe_` initialized
    // successfully. Failure to even initialize `system_cpu_probe_` isn't
    // recorded because that means system CPU isn't available at all on this
    // system rather than a transient error.
    base::UmaHistogramBoolean(
        base::StrCat({"PerformanceManager.PerformanceInterventions.CPU."
                      "SystemCPUError.",
                      suffix}),
        true);
  }

  // Log derived background UMA metrics.
  if (histogram_suffix == CPUInterventionSuffix::kBaseline) {
    return;
  }
  std::sort(background_cpu_usage.begin(), background_cpu_usage.end(),
            std::greater<double>());

  int tabs_to_get_under_threshold = 0;
  const double kThreshold =
      performance_manager::features::kThresholdChromeCPUPercent.Get() *
      (base::SysInfo::NumberOfProcessors() / 100.0);

  double cpu_to_get_under_threshold =
      total_foreground_cpu_usage + total_background_cpu_usage - kThreshold;
  if (total_background_cpu_usage < cpu_to_get_under_threshold) {
    // Use max int to represent when closing all background tabs won't be
    // enough.
    tabs_to_get_under_threshold = std::numeric_limits<int>::max();
  } else {
    for (double cpu_usage : background_cpu_usage) {
      cpu_to_get_under_threshold -= cpu_usage;
      tabs_to_get_under_threshold++;
      if (cpu_to_get_under_threshold <= 0) {
        break;
      }
    }
  }

  base::UmaHistogramCounts1000(
      base::StrCat({"PerformanceManager.PerformanceInterventions.CPU."
                    "BackgroundTabsToGetUnderCPUThreshold.",
                    suffix}),
      tabs_to_get_under_threshold);

  for (auto& n : kTabCountSlices) {
    // Accumulate memory from the top CPU usage tab through the nth or the last
    // tab, whichever is first.
    const auto nth_iter = std::next(background_cpu_usage.begin(),
                                    std::min(n, background_cpu_usage.size()));
    double top_n_cpu =
        std::accumulate(background_cpu_usage.begin(), nth_iter, 0.0);

    base::UmaHistogramPercentage(
        base::StrCat({"PerformanceManager.PerformanceInterventions.CPU."
                      "TopNBackgroundCPU.",
                      base::NumberToString(n), ".", suffix}),
        top_n_cpu * 100 / base::SysInfo::NumberOfProcessors());
  }
}

void PageResourceMonitor::CalculatePageCPUUsage(
    bool use_delayed_system_cpu_probe,
    base::OnceCallback<void(const PageCPUUsageVector&,
                            absl::optional<PressureSample>)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cpu_monitor_.UpdateCPUMeasurements(base::BindOnce(
      &PageResourceMonitor::OnPageCPUUsageResult, weak_factory_.GetWeakPtr(),
      use_delayed_system_cpu_probe, std::move(callback)));
}

void PageResourceMonitor::OnPageCPUUsageResult(
    bool use_delayed_system_cpu_probe,
    base::OnceCallback<void(const PageCPUUsageVector&,
                            absl::optional<PressureSample>)> callback,
    const PageResourceCPUMonitor::CPUUsageMap& cpu_usage_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(graph_);

  // Calculate the overall CPU usage.
  PageCPUUsageVector page_cpu_usage;
  graph_->VisitAllPageNodes([&page_cpu_usage,
                             &cpu_usage_map](const PageNode* page_node) {
    if (page_node->GetType() == PageType::kTab) {
      page_cpu_usage.emplace_back(page_node->GetResourceContext(),
                                  PageResourceCPUMonitor::EstimatePageCPUUsage(
                                      page_node, cpu_usage_map));
    }
    return true;
  });

  // Now fetch the system CPU usage if available.
  CpuProbe* cpu_probe = use_delayed_system_cpu_probe
                            ? delayed_system_cpu_probe_.get()
                            : system_cpu_probe_.get();
  if (cpu_probe) {
    cpu_probe->RequestSample(
        base::BindOnce(std::move(callback), page_cpu_usage));
  } else {
    std::move(callback).Run(page_cpu_usage, absl::nullopt);
  }
}

}  // namespace performance_manager::metrics
