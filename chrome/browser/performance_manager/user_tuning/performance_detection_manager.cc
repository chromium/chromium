// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/observer_list.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "components/performance_manager/public/resource_attribution/process_context.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/performance_manager/public/resource_attribution/resource_contexts.h"
#include "components/system_cpu/cpu_sample.h"

namespace performance_manager::user_tuning {

namespace {
PerformanceDetectionManager* g_performance_detection_manager = nullptr;
}  // namespace

void PerformanceDetectionManager::AddStatusObserver(
    ResourceTypeSet resource_types,
    StatusObserver* o) {
  for (auto resource_type : resource_types) {
    status_observers_[resource_type].AddObserver(o);
    o->OnStatusChanged(resource_type, current_health_status_[resource_type],
                       false);
  }
}

void PerformanceDetectionManager::RemoveStatusObserver(StatusObserver* o) {
  for (auto& [resource_type, observer_list] : status_observers_) {
    observer_list.RemoveObserver(o);
  }
}

void PerformanceDetectionManager::AddActionableTabsObserver(
    ResourceTypeSet resource_types,
    ActionableTabsObserver* o) {
  // TODO(crbug.com/324261765): Implement method
  for (auto resource_type : resource_types) {
    o->OnActionableTabListChanged(resource_type, {});
  }
}

void PerformanceDetectionManager::RemoveActionableTabsObserver(
    ActionableTabsObserver* o) {
  // TODO(crbug.com/324261765): Implement method
}

// static
bool PerformanceDetectionManager::HasInstance() {
  return g_performance_detection_manager;
}

// static
PerformanceDetectionManager* PerformanceDetectionManager::GetInstance() {
  CHECK(g_performance_detection_manager);
  return g_performance_detection_manager;
}

PerformanceDetectionManager::~PerformanceDetectionManager() {
  CHECK_EQ(this, g_performance_detection_manager);
  g_performance_detection_manager = nullptr;
}

PerformanceDetectionManager::PerformanceDetectionManager()
    : cpu_health_sample_window_size_(
          performance_manager::features::kCPUTimeOverThreshold.Get() /
          performance_manager::features::kCPUSampleFrequency.Get()),
      recent_cpu_health_levels_(cpu_health_sample_window_size_,
                                HealthLevel::kHealthy),
      // scoped_cpu_query_ is initialized to monitor CPU usage. Actual queries
      // are being sent from ProcessCpuProbeResult().
      scoped_cpu_query_(
          resource_attribution::QueryBuilder()
              .AddAllContextsOfType<resource_attribution::PageContext>()
              .AddResourceType(resource_attribution::ResourceType::kCPUTime)
              .CreateScopedQuery()) {
  CHECK(!g_performance_detection_manager);
  g_performance_detection_manager = this;

  const std::vector<ResourceType> resource_types = {ResourceType::kCpu,
                                                    ResourceType::kMemory};
  current_health_status_ = base::MakeFlatMap<ResourceType, HealthLevel>(
      resource_types, {}, [](ResourceType type) {
        return std::make_pair(type, HealthLevel::kHealthy);
      });

  std::unique_ptr<system_cpu::CpuProbe> cpu_probe =
      system_cpu::CpuProbe::Create();
  cpu_probe->StartSampling();
  cpu_probe_timer_.Start(
      FROM_HERE, performance_manager::features::kCPUSampleFrequency.Get(),
      base::BindRepeating(
          &system_cpu::CpuProbe::RequestSample, std::move(cpu_probe),
          base::BindRepeating(
              &PerformanceDetectionManager::ProcessCpuProbeResult,
              base::Unretained(this))));
  // base::Unretained(this) is safe here because the CPU probe is owned by the
  // callback, which is owned by the timer. The timer is owned by this, so the
  // callback will not be invoked after this is destroyed
}

PerformanceDetectionManager::HealthLevel
PerformanceDetectionManager::GetHealthLevelForTesting(
    ResourceType resource_type) {
  const auto it = current_health_status_.find(resource_type);
  CHECK(it != current_health_status_.end());
  return it->second;
}

void PerformanceDetectionManager::ProcessCpuProbeResult(
    std::optional<system_cpu::CpuSample> cpu_sample) {
  if (!cpu_sample.has_value()) {
    return;
  }

  const int total_system_cpu_usage = cpu_sample.value().cpu_utilization * 100;
  if (GetCpuHealthStatus(total_system_cpu_usage) != HealthLevel::kHealthy) {
    // Query for tab CPU usage to determine actionability
    resource_attribution::QueryBuilder()
        .AddResourceType(resource_attribution::ResourceType::kCPUTime)
        .AddAllContextsOfType<resource_attribution::PageContext>()
        .QueryOnce(base::BindOnce(
            &PerformanceDetectionManager::ProcessQueryResultMap,
            weak_ptr_factory_.GetWeakPtr(), total_system_cpu_usage));
    // We delay recording total_system_cpu_usage for not healthy CPU usage until
    // we get results from the query to ensure that the recorded CPU and
    // resulting health status stays consistent with tab actionability
  } else if (RecordAndUpdateCpuHealthStatus(total_system_cpu_usage)) {
    // Notify observers that the health level became healthy
    // We don't need to query for tab data because nothing needs to be
    // actionable when CPU is healthy
    NotifyStatusObservers(ResourceType::kCpu, false);
  }
}

PerformanceDetectionManager::HealthLevel
PerformanceDetectionManager::GetCpuHealthStatus(int cpu_usage_percentage) {
  if (cpu_usage_percentage >
      performance_manager::features::kCPUUnhealthyPercentageThreshold.Get()) {
    return HealthLevel::kUnhealthy;
  } else if (cpu_usage_percentage >
             performance_manager::features::
                 kCPUDegradedHealthPercentageThreshold.Get()) {
    return HealthLevel::kDegraded;
  }
  return HealthLevel::kHealthy;
}

bool PerformanceDetectionManager::RecordAndUpdateCpuHealthStatus(
    int cpu_usage_percentage) {
  CHECK_EQ(recent_cpu_health_levels_.size(), cpu_health_sample_window_size_);

  // Remove the oldest health level and add the updated status
  recent_cpu_health_levels_.pop_front();
  recent_cpu_health_levels_.push_back(GetCpuHealthStatus(cpu_usage_percentage));

  const HealthLevel new_level = *std::min_element(
      recent_cpu_health_levels_.begin(), recent_cpu_health_levels_.end());
  const auto it = current_health_status_.find(ResourceType::kCpu);
  const HealthLevel old_level = it->second;
  it->second = new_level;

  return new_level != old_level;
}

void PerformanceDetectionManager::ProcessQueryResultMap(
    int system_cpu_usage_percentage,
    const resource_attribution::QueryResultMap& results) {
  const base::TimeTicks measurement_time = base::TimeTicks::Now();
  const bool did_status_change =
      RecordAndUpdateCpuHealthStatus(system_cpu_usage_percentage);

  if (!page_cpu_proportion_tracker_.IsTracking()) {
    page_cpu_proportion_tracker_.StartFirstInterval(measurement_time, results);
  } else {
    // Determine cpu usage for each page context
    page_cpu_proportion_tracker_.StartNextInterval(measurement_time, results);

    // Notify observers that the status changes
    // We use the system cpu if it is available, otherwise we need to sum up the
    // process cpu to get Chrome usage cpu
    if (did_status_change) {
      NotifyStatusObservers(ResourceType::kCpu, false);
    }
  }
}

void PerformanceDetectionManager::NotifyStatusObservers(
    ResourceType resource_type,
    bool is_actionable) {
  for (auto& obs : status_observers_[resource_type]) {
    obs.OnStatusChanged(resource_type, current_health_status_[resource_type],
                        is_actionable);
  }
}
}  // namespace performance_manager::user_tuning
