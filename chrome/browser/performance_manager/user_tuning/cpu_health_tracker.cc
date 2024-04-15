// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/user_tuning/cpu_health_tracker.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "components/performance_manager/public/features.h"
#include "content/public/browser/browser_thread.h"

namespace performance_manager::user_tuning {

CpuHealthTracker::CpuHealthTracker(
    StatusChangeCallback on_status_change_cb,
    ActionableTabResultCallback on_actionability_change_cb)
    : status_change_cb_(std::move(on_status_change_cb)),
      actionable_tabs_cb_(std::move(on_actionability_change_cb)),
      cpu_health_sample_window_size_(
          performance_manager::features::kCPUTimeOverThreshold.Get() /
          performance_manager::features::kCPUSampleFrequency.Get()),
      recent_resource_measurements_(cpu_health_sample_window_size_, 0),
      // scoped_cpu_query_ is initialized to monitor CPU usage. Actual queries
      // are being sent from ProcessCpuProbeResult().
      scoped_cpu_query_(
          resource_attribution::QueryBuilder()
              .AddAllContextsOfType<resource_attribution::PageContext>()
              .AddResourceType(resource_attribution::ResourceType::kCPUTime)
              .CreateScopedQuery()) {
  std::unique_ptr<system_cpu::CpuProbe> cpu_probe =
      system_cpu::CpuProbe::Create();
  cpu_probe->StartSampling();
  cpu_probe_timer_.Start(
      FROM_HERE, performance_manager::features::kCPUSampleFrequency.Get(),
      base::BindRepeating(
          &system_cpu::CpuProbe::RequestSample, std::move(cpu_probe),
          base::BindRepeating(&CpuHealthTracker::ProcessCpuProbeResult,
                              base::Unretained(this))));
  // base::Unretained(this) is safe here because the CPU probe is owned by the
  // callback, which is owned by the timer. The timer is owned by this, so the
  // callback will not be invoked after this is destroyed
}

CpuHealthTracker::~CpuHealthTracker() = default;

CpuHealthTracker::HealthLevel CpuHealthTracker::GetHealthLevelForTesting() {
  return current_health_status_;
}

void CpuHealthTracker::OnPassedToGraph(performance_manager::Graph* graph) {
  graph->RegisterObject(this);
}

void CpuHealthTracker::OnTakenFromGraph(performance_manager::Graph* graph) {
  graph->UnregisterObject(this);
}

base::OnceCallback<void(CpuHealthTracker::ActionableTabsResult)>
CpuHealthTracker::GetStatusAndActionabilityCallback(
    bool did_status_change,
    CpuHealthTracker::HealthLevel health_level) {
  return base::BindOnce(
      [](StatusChangeCallback status_change,
         ActionableTabResultCallback actionability_change,
         bool did_status_change, HealthLevel health_level,
         ActionableTabsResult previously_actionable,
         ActionableTabsResult actionable_tabs) {
        if (did_status_change) {
          status_change.Run(ResourceType::kCpu, health_level,
                            !actionable_tabs.empty());
        }

        if (previously_actionable != actionable_tabs) {
          actionability_change.Run(ResourceType::kCpu, actionable_tabs);
        }
      },
      status_change_cb_, actionable_tabs_cb_, did_status_change, health_level,
      actionable_tabs_);
}

CpuHealthTracker::HealthLevel CpuHealthTracker::GetHealthLevelForMeasurement(
    int measurement) {
  if (measurement >
      performance_manager::features::kCPUUnhealthyPercentageThreshold.Get()) {
    return HealthLevel::kUnhealthy;
  }

  if (measurement >
      performance_manager::features::kCPUDegradedHealthPercentageThreshold
          .Get()) {
    return HealthLevel::kDegraded;
  }

  return HealthLevel::kHealthy;
}

void CpuHealthTracker::GetFilteredActionableTabs(
    PageResourceMeasurements unfiltered_measurements,
    int recent_measurement,
    base::OnceCallback<void(ActionableTabsResult)> callback) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), ActionableTabsResult()));

  // TODO(crbug.com/324261765): determine which page contexts are actionable
  // and run the 'on_results_callback' with the list of actionable pages
}

bool CpuHealthTracker::RecordAndUpdateHealthStatus(int measurement) {
  CHECK_EQ(recent_resource_measurements_.size(),
           cpu_health_sample_window_size_);

  // Remove the oldest health measurement and add the updated measurement
  recent_resource_measurements_.pop_front();
  recent_resource_measurements_.push_back(measurement);

  const HealthLevel new_level = GetHealthLevelForMeasurement(
      *std::min_element(recent_resource_measurements_.begin(),
                        recent_resource_measurements_.end()));

  const HealthLevel old_level = current_health_status_;
  current_health_status_ = new_level;
  return new_level != old_level;
}

void CpuHealthTracker::ProcessCpuProbeResult(
    std::optional<system_cpu::CpuSample> cpu_sample) {
  if (!cpu_sample.has_value()) {
    return;
  }

  const int total_system_cpu_usage = cpu_sample.value().cpu_utilization * 100;
  if (GetHealthLevelForMeasurement(total_system_cpu_usage) !=
      HealthLevel::kHealthy) {
    // Query for tab CPU usage to determine actionability
    resource_attribution::QueryBuilder()
        .AddResourceType(resource_attribution::ResourceType::kCPUTime)
        .AddAllContextsOfType<resource_attribution::PageContext>()
        .QueryOnce(base::BindOnce(&CpuHealthTracker::ProcessQueryResultMap,
                                  weak_ptr_factory_.GetWeakPtr(),
                                  total_system_cpu_usage));
    // We delay recording total_system_cpu_usage for not healthy CPU usage until
    // we get results from the query to ensure that the recorded CPU and
    // resulting health status stays consistent with tab actionability
  } else if (RecordAndUpdateHealthStatus(total_system_cpu_usage)) {
    // Notify observers that the health level became healthy.
    // We don't need to query for tab data because nothing needs to be
    // actionable when CPU is healthy.

    base::OnceClosure notify_healthy_status = base::BindOnce(
        GetStatusAndActionabilityCallback(true, HealthLevel::kHealthy),
        ActionableTabsResult());

    if (!actionable_tabs_.empty()) {
      actionable_tabs_ = {};
      possible_actionable_pages_ = {};
    }

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, std::move(notify_healthy_status));
  }
}

void CpuHealthTracker::ProcessQueryResultMap(
    int system_cpu_usage_percentage,
    const resource_attribution::QueryResultMap& results) {
  const base::TimeTicks measurement_time = base::TimeTicks::Now();
  const bool did_status_change =
      RecordAndUpdateHealthStatus(system_cpu_usage_percentage);

  if (!page_cpu_proportion_tracker_.IsTracking()) {
    page_cpu_proportion_tracker_.StartFirstInterval(measurement_time, results);
  } else {
    // Determine cpu usage for each page context
    std::map<resource_attribution::ResourceContext, double> page_cpu =
        page_cpu_proportion_tracker_.StartNextInterval(measurement_time,
                                                       results);
    possible_actionable_pages_ = GetPagesMeetMinimumCpuUsage(page_cpu);

    GetFilteredActionableTabs(possible_actionable_pages_,
                              system_cpu_usage_percentage,
                              GetStatusAndActionabilityCallback(
                                  did_status_change, current_health_status_));
  }
}

CpuHealthTracker::PageResourceMeasurements
CpuHealthTracker::GetPagesMeetMinimumCpuUsage(
    std::map<resource_attribution::ResourceContext, double> page_cpu) {
  std::vector<std::pair<resource_attribution::PageContext, int>> eligible_pages;

  for (auto& it : page_cpu) {
    const int cpu_usage_percentage =
        it.second * 100 / base::SysInfo::NumberOfProcessors();
    if (cpu_usage_percentage >=
        performance_manager::features::kMinimumActionableTabCPUPercentage
            .Get()) {
      eligible_pages.emplace_back(
          resource_attribution::AsContext<resource_attribution::PageContext>(
              it.first),
          cpu_usage_percentage);
    }
  }

  return base::MakeFlatMap<resource_attribution::PageContext, int>(
      eligible_pages);
}
}  // namespace performance_manager::user_tuning
