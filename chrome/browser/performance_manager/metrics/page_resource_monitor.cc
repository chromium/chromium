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
#include <optional>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/performance_manager/metrics/page_resource_cpu_monitor.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/resource_attribution/cpu_proportion_tracker.h"
#include "components/performance_manager/public/resource_attribution/resource_types.h"
#include "components/system_cpu/cpu_probe.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace performance_manager::metrics {

namespace {

using system_cpu::CpuProbe;
using system_cpu::CpuSample;
using PageMeasurementBackgroundState =
    PageResourceMonitor::PageMeasurementBackgroundState;

using MemorySummaryResult = resource_attribution::MemorySummaryResult;
using PageContext = resource_attribution::PageContext;
using QueryResultMap = resource_attribution::QueryResultMap;
using ResourceContext = resource_attribution::ResourceContext;
using ResourceType = resource_attribution::ResourceType;

// CPU usage metrics are provided as a double in the [0.0, number of cores *
// 100.0] range. The CPU usage is usually below 1%, so the UKM is
// reported out of 10,000 instead of out of 100 to make analyzing the data
// easier. This is the same scale factor used by the
// PerformanceMonitor.AverageCPU8 histograms recorded in
// chrome/browser/metrics/power/process_metrics_recorder_util.cc.
constexpr int kCPUUsageFactor = 100 * 100;

// The values for n when calculating the total CPU usage of the top n tabs.
constexpr std::array<size_t, 5> kTabCountSlices = {1, 2, 4, 8, 16};

// The time between calls to OnResourceUsageUpdated()
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

resource_attribution::QueryBuilder CPUQueryBuilder() {
  resource_attribution::QueryBuilder builder;
  builder.AddAllContextsOfType<PageContext>().AddResourceType(
      ResourceType::kCPUTime);
  return builder;
}

const PageNode* PageNodeFromContext(const ResourceContext& context) {
  // The query returned by CPUQueryBuilder() should only measure PageContexts.
  // AsContext() asserts that `context` is a PageContext.
  return resource_attribution::AsContext<PageContext>(context).GetPageNode();
}

bool ContextIsTab(const ResourceContext& context) {
  const PageNode* page_node = PageNodeFromContext(context);
  return page_node && page_node->GetType() == PageType::kTab;
}

bool IsCPUInterventionEvaluationLoggingEnabled() {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return base::FeatureList::IsEnabled(
      features::kCPUInterventionEvaluationLogging);
#endif
}

}  // namespace

class PageResourceMonitor::CPUResultConverter {
 public:
  // A callback that's invoked with the converted results.
  using ResultCallback = base::OnceCallback<void(const PageCPUUsageMap&,
                                                 std::optional<CpuSample>)>;

  explicit CPUResultConverter(std::unique_ptr<CpuProbe> system_cpu_probe);
  ~CPUResultConverter() = default;

  base::WeakPtr<CPUResultConverter> GetWeakPtr();

  bool HasSystemCPUProbe() const;

  // Invokes `result_callback_` with the converted `results`.
  void OnResourceUsageUpdated(ResultCallback result_callback,
                              const QueryResultMap& results);

 private:
  void StartFirstInterval(base::TimeTicks time, const QueryResultMap& results);
  void StartNextInterval(ResultCallback result_callback,
                         base::TimeTicks time,
                         const QueryResultMap& results,
                         std::optional<CpuSample> system_cpu);

  std::unique_ptr<CpuProbe> system_cpu_probe_;
  resource_attribution::CPUProportionTracker proportion_tracker_;
  base::WeakPtrFactory<CPUResultConverter> weak_factory_{this};
};

PageResourceMonitor::PageResourceMonitor(bool enable_system_cpu_probe)
    : resource_query_(CPUQueryBuilder()
                          .AddResourceType(ResourceType::kMemorySummary)
                          .CreateScopedQuery()) {
  query_observation_.Observe(&resource_query_);
  resource_query_.Start(kCollectionDelay);
  std::unique_ptr<CpuProbe> system_cpu_probe;
  if (enable_system_cpu_probe && IsCPUInterventionEvaluationLoggingEnabled()) {
    system_cpu_probe = CpuProbe::Create();
  }
  cpu_result_converter_ =
      std::make_unique<CPUResultConverter>(std::move(system_cpu_probe));
  if (base::FeatureList::IsEnabled(features::kResourceAttributionValidation)) {
    cpu_monitor_ = std::make_unique<PageResourceCPUMonitor>();
  }
}

PageResourceMonitor::~PageResourceMonitor() = default;

void PageResourceMonitor::OnResourceUsageUpdated(
    const QueryResultMap& results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cpu_result_converter_->OnResourceUsageUpdated(
      base::BindOnce(&PageResourceMonitor::OnPageResourceUsageResult,
                     weak_factory_.GetWeakPtr(), results),
      results);
}

void PageResourceMonitor::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!graph_);
  graph_ = graph;
  if (cpu_monitor_) {
    cpu_monitor_->StartMonitoring(graph);
  }
}

void PageResourceMonitor::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(graph_, graph);
  graph_ = nullptr;
  if (cpu_monitor_) {
    cpu_monitor_->StopMonitoring(graph);
  }
}

base::TimeDelta PageResourceMonitor::GetCollectionDelayForTesting() const {
  return kCollectionDelay;
}

base::TimeDelta PageResourceMonitor::GetDelayedMetricsTimeoutForTesting()
    const {
  return performance_manager::features::kDelayBeforeLogging.Get();
}

PageResourceCPUMonitor* PageResourceMonitor::GetCPUMonitorForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return cpu_monitor_.get();
}

void PageResourceMonitor::OnPageResourceUsageResult(
    const QueryResultMap& results,
    const PageCPUUsageMap& page_cpu_usage,
    std::optional<CpuSample> system_cpu) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Calculate the overall CPU usage.
  double total_cpu_usage = 0;
  for (const auto& [page_context, cpu_usage] : page_cpu_usage) {
    total_cpu_usage += cpu_usage;
  }

  // Contexts in `page_cpu_usage` are a subset of contexts in `results`.
  const auto now = base::TimeTicks::Now();
  for (const auto& [page_context, result] : results) {
    const PageNode* page_node = PageNodeFromContext(page_context);
    if (!page_node) {
      // Page was deleted while waiting for system CPU. Nothing to log.
      continue;
    }
    if (page_node->GetType() != PageType::kTab) {
      continue;
    }
    const ukm::SourceId source_id = page_node->GetUkmSourceID();
    auto ukm = ukm::builders::PerformanceManager_PageResourceUsage2(source_id);
    ukm.SetBackgroundState(
        static_cast<int64_t>(GetBackgroundStateForMeasurementPeriod(
            page_node, now - time_of_last_resource_usage_)));
    ukm.SetMeasurementAlgorithm(
        static_cast<int64_t>(PageMeasurementAlgorithm::kEvenSplitAndAggregate));
    // Add CPU usage, if this page included it.
    const auto it = page_cpu_usage.find(page_context);
    if (it != page_cpu_usage.end()) {
      ukm.SetRecentCPUUsage(kCPUUsageFactor * it->second);
      ukm.SetTotalRecentCPUUsageAllPages(kCPUUsageFactor * total_cpu_usage);
    }
    // Add memory summary, if this page included it.
    if (result.memory_summary_result.has_value()) {
      ukm.SetResidentSetSizeEstimate(
          result.memory_summary_result->resident_set_size_kb);
      ukm.SetPrivateFootprintEstimate(
          result.memory_summary_result->private_footprint_kb);
    }
    ukm.Record(ukm::UkmRecorder::Get());
  }

  if (base::FeatureList::IsEnabled(features::kResourceAttributionValidation)) {
    // Also record the legacy calculation.
    CHECK(cpu_monitor_);
    const PageResourceCPUMonitor::CPUUsageMap cpu_usage_map =
        cpu_monitor_->UpdateCPUMeasurements();

    CHECK(graph_);
    std::vector<std::pair<const PageNode*, double>> legacy_page_cpu_usage;
    double total_legacy_cpu_usage = 0;
    graph_->VisitAllPageNodes([&legacy_page_cpu_usage, &total_legacy_cpu_usage,
                               &cpu_usage_map](const PageNode* page_node) {
      if (page_node->GetType() == PageType::kTab) {
        const double cpu_usage = PageResourceCPUMonitor::EstimatePageCPUUsage(
            page_node, cpu_usage_map);
        total_legacy_cpu_usage += cpu_usage;
        legacy_page_cpu_usage.emplace_back(page_node, cpu_usage);
      }
      return true;
    });

    for (const auto& [page_node, cpu_usage] : legacy_page_cpu_usage) {
      const ukm::SourceId source_id = page_node->GetUkmSourceID();
      ukm::builders::PerformanceManager_PageResourceUsage2(source_id)
          .SetBackgroundState(
              static_cast<int64_t>(GetBackgroundStateForMeasurementPeriod(
                  page_node, now - time_of_last_resource_usage_)))
          .SetMeasurementAlgorithm(
              static_cast<int64_t>(PageMeasurementAlgorithm::kLegacy))
          .SetRecentCPUUsage(kCPUUsageFactor * cpu_usage)
          .SetTotalRecentCPUUsageAllPages(kCPUUsageFactor *
                                          total_legacy_cpu_usage)
          .SetResidentSetSizeEstimate(page_node->EstimateResidentSetSize())
          .SetPrivateFootprintEstimate(
              page_node->EstimatePrivateFootprintSize())
          .Record(ukm::UkmRecorder::Get());
    }
  }

  time_of_last_resource_usage_ = now;

  if (IsCPUInterventionEvaluationLoggingEnabled()) {
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
        CHECK(!delayed_cpu_result_converter_);
        delayed_cpu_result_converter_ = std::make_unique<CPUResultConverter>(
            cpu_result_converter_->HasSystemCPUProbe() ? CpuProbe::Create()
                                                       : nullptr);
        log_cpu_on_delay_timer_.Start(
            FROM_HERE, performance_manager::features::kDelayBeforeLogging.Get(),
            this, &PageResourceMonitor::CheckDelayedCPUInterventionMetrics);
      }
    } else if (!is_cpu_over_threshold) {
      base::UmaHistogramCustomTimes(
          "PerformanceManager.PerformanceInterventions.CPU."
          "DurationOverThreshold",
          now - time_of_last_cpu_threshold_exceeded_.value(), base::Minutes(2),
          base::Hours(24), 50);
      log_cpu_on_delay_timer_.AbandonAndStop();
      time_of_last_cpu_threshold_exceeded_ = std::nullopt;
      delayed_cpu_result_converter_.reset();
    }
  }
}

void PageResourceMonitor::CheckDelayedCPUInterventionMetrics() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(delayed_cpu_result_converter_);
  CPUQueryBuilder().QueryOnce(base::BindOnce(
      &CPUResultConverter::OnResourceUsageUpdated,
      delayed_cpu_result_converter_->GetWeakPtr(),
      base::BindOnce(
          &PageResourceMonitor::OnDelayedCPUInterventionMetricsResult,
          weak_factory_.GetWeakPtr())));
}

void PageResourceMonitor::OnDelayedCPUInterventionMetricsResult(
    const PageCPUUsageMap& page_cpu_usage,
    std::optional<CpuSample> system_cpu) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Now that results are received, stop the delayed CPU probe and proportion
  // tracking.
  delayed_cpu_result_converter_.reset();
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
    const PageCPUUsageMap& page_cpu_usage,
    const std::optional<CpuSample>& system_cpu,
    const base::TimeTicks now,
    CPUInterventionSuffix histogram_suffix) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<double> background_cpu_usage;
  double total_foreground_cpu_usage = 0;

  int foreground_tab_count = 0;
  int background_tab_count = 0;

  for (const auto& [page_context, cpu_usage] : page_cpu_usage) {
    const PageNode* page_node = PageNodeFromContext(page_context);
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
  } else if (cpu_result_converter_->HasSystemCPUProbe()) {
    // System CPU probing is available, so there must have been an error in
    // CpuProbe::RequestSample().
    //
    // For .Delayed histograms this can also include a failure to initialize the
    // CpuProbe in `delayed_cpu_result_converter_` when `cpu_result_converter_`
    // initialized successfully. Failure to even initialize
    // `cpu_result_converter_` isn't recorded because that means system CPU
    // isn't available at all on this system rather than a transient error.
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

PageResourceMonitor::CPUResultConverter::CPUResultConverter(
    std::unique_ptr<CpuProbe> system_cpu_probe)
    : system_cpu_probe_(std::move(system_cpu_probe)),
      // Only calculate results for tabs, not extensions.
      proportion_tracker_(base::BindRepeating(&ContextIsTab)) {
  CPUQueryBuilder().QueryOnce(
      base::BindOnce(&CPUResultConverter::StartFirstInterval, GetWeakPtr(),
                     base::TimeTicks::Now()));
  if (system_cpu_probe_) {
    system_cpu_probe_->StartSampling();
  }
}

base::WeakPtr<PageResourceMonitor::CPUResultConverter>
PageResourceMonitor::CPUResultConverter::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool PageResourceMonitor::CPUResultConverter::HasSystemCPUProbe() const {
  return static_cast<bool>(system_cpu_probe_);
}

void PageResourceMonitor::CPUResultConverter::OnResourceUsageUpdated(
    CPUResultConverter::ResultCallback result_callback,
    const QueryResultMap& results) {
  auto next_update_callback = base::BindOnce(
      &CPUResultConverter::StartNextInterval, GetWeakPtr(),
      std::move(result_callback), base::TimeTicks::Now(), results);
  if (system_cpu_probe_) {
    system_cpu_probe_->RequestSample(std::move(next_update_callback));
  } else {
    std::move(next_update_callback).Run(std::nullopt);
  }
}

void PageResourceMonitor::CPUResultConverter::StartFirstInterval(
    base::TimeTicks time,
    const QueryResultMap& results) {
  proportion_tracker_.StartFirstInterval(time, results);
}

void PageResourceMonitor::CPUResultConverter::StartNextInterval(
    CPUResultConverter::ResultCallback result_callback,
    base::TimeTicks time,
    const QueryResultMap& results,
    std::optional<CpuSample> system_cpu) {
  std::move(result_callback)
      .Run(proportion_tracker_.StartNextInterval(time, results),
           std::move(system_cpu));
}

}  // namespace performance_manager::metrics
