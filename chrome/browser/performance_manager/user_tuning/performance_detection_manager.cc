// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
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
    StatusObserver* observer) {
  for (auto resource_type : resource_types) {
    status_observers_[resource_type].AddObserver(observer);
    observer->OnStatusChanged(resource_type,
                              current_health_status_[resource_type], false);
  }
}

void PerformanceDetectionManager::RemoveStatusObserver(StatusObserver* o) {
  for (auto& [resource_type, observer_list] : status_observers_) {
    observer_list.RemoveObserver(o);
  }
}

void PerformanceDetectionManager::AddActionableTabsObserver(
    ResourceTypeSet resource_types,
    ActionableTabsObserver* new_observer) {
  for (auto resource_type : resource_types) {
    actionable_tab_observers_[resource_type].AddObserver(new_observer);
    // Need to go through possible_actionable_pages_ again to filter for
    // actionable tabs because the list of actionable tabs may have changed
    // since the previous list of actionable tabs was generated
    GetActionablePages(
        resource_type, possible_actionable_pages_[resource_type],
        recent_resource_measurements_[resource_type].back(),
        base::BindOnce(
            &PerformanceDetectionManager::MaybeNotifyAllActionableObservers,
            weak_ptr_factory_.GetWeakPtr(), resource_type, new_observer));
  }
}

void PerformanceDetectionManager::RemoveActionableTabsObserver(
    ActionableTabsObserver* o) {
  for (auto& [resource_type, observer_list] : actionable_tab_observers_) {
    observer_list.RemoveObserver(o);
  }
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

  actionable_tabs_ =
      base::MakeFlatMap<ResourceType,
                        std::vector<resource_attribution::PageContext>>(
          resource_types, {}, [](ResourceType type) {
            return std::make_pair(
                type, std::vector<resource_attribution::PageContext>());
          });

  possible_actionable_pages_ =
      base::MakeFlatMap<ResourceType, PageResourceMeasurements>(
          resource_types, {}, [](ResourceType type) {
            return std::make_pair(type, PageResourceMeasurements());
          });

  recent_resource_measurements_ =
      base::MakeFlatMap<ResourceType, base::circular_deque<int>>(
          resource_types, {},
          [window_size = cpu_health_sample_window_size_](ResourceType type) {
            return std::make_pair(type,
                                  base::circular_deque<int>(window_size, 0));
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
    // Notify observers that the health level became healthy.
    // We don't need to query for tab data because nothing needs to be
    // actionable when CPU is healthy.
    possible_actionable_pages_[ResourceType::kCpu] = {};
    NotifyStatusObservers(ResourceType::kCpu, false);
    if (!actionable_tabs_[ResourceType::kCpu].empty()) {
      NotifyActionableTabObservers(ResourceType::kCpu, {});
    }
  }
}

bool PerformanceDetectionManager::RecordAndUpdateCpuHealthStatus(
    int cpu_usage_percentage) {
  base::circular_deque<int>& recent_cpu_measurements =
      recent_resource_measurements_[ResourceType::kCpu];
  CHECK_EQ(recent_cpu_measurements.size(), cpu_health_sample_window_size_);

  // Remove the oldest health measurement and add the updated measurement
  recent_cpu_measurements.pop_front();
  recent_cpu_measurements.push_back(cpu_usage_percentage);

  const HealthLevel new_level = GetCpuHealthStatus(*std::min_element(
      recent_cpu_measurements.begin(), recent_cpu_measurements.end()));
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
    std::map<resource_attribution::ResourceContext, double> page_cpu =
        page_cpu_proportion_tracker_.StartNextInterval(measurement_time,
                                                       results);
    possible_actionable_pages_[ResourceType::kCpu] =
        GetPagesMeetMinimumCpuUsage(page_cpu);

    GetActionablePages(
        ResourceType::kCpu, possible_actionable_pages_[ResourceType::kCpu],
        system_cpu_usage_percentage,
        base::BindOnce(&PerformanceDetectionManager::
                           MaybeNotifyStatusAndActionabilityChange,
                       weak_ptr_factory_.GetWeakPtr(), ResourceType::kCpu,
                       did_status_change));
  }
}

PerformanceDetectionManager::HealthLevel
PerformanceDetectionManager::GetCpuHealthStatus(int cpu_usage_percentage) {
  if (cpu_usage_percentage >
      performance_manager::features::kCPUUnhealthyPercentageThreshold.Get()) {
    return PerformanceDetectionManager::HealthLevel::kUnhealthy;
  } else if (cpu_usage_percentage >
             performance_manager::features::
                 kCPUDegradedHealthPercentageThreshold.Get()) {
    return PerformanceDetectionManager::HealthLevel::kDegraded;
  }
  return PerformanceDetectionManager::HealthLevel::kHealthy;
}

PerformanceDetectionManager::PageResourceMeasurements
PerformanceDetectionManager::GetPagesMeetMinimumCpuUsage(
    std::map<resource_attribution::ResourceContext, double> page_cpu) {
  std::vector<std::pair<resource_attribution::PageContext, int>> eligible_pages;

  for (auto& it : page_cpu) {
    const int cpu_usage_percentage =
        it.second * 100 / base::SysInfo::NumberOfProcessors();
    if (cpu_usage_percentage >=
        features::kMinimumActionableTabCPUPercentage.Get()) {
      resource_attribution::ResourceContext context = it.first;
      eligible_pages.emplace_back(
          resource_attribution::AsContext<resource_attribution::PageContext>(
              context),
          cpu_usage_percentage);
    }
  }

  return base::MakeFlatMap<resource_attribution::PageContext, int>(
      eligible_pages);
}

void PerformanceDetectionManager::GetActionablePages(
    ResourceType resource_type,
    const PageResourceMeasurements& page_measurements,
    int overall_cpu_usage,
    ActionableTabResultCallback on_results_callback) {
  std::move(on_results_callback).Run({});

  // TODO(crbug.com/324261765): determine which page contexts are actionable
  // and run the 'on_results_callback' with the list of actionable pages
}

void PerformanceDetectionManager::NotifyStatusObservers(
    ResourceType resource_type,
    bool is_actionable) {
  for (auto& obs : status_observers_[resource_type]) {
    obs.OnStatusChanged(resource_type, current_health_status_[resource_type],
                        is_actionable);
  }
}

void PerformanceDetectionManager::NotifyActionableTabObservers(
    ResourceType resource_type,
    std::vector<resource_attribution::PageContext> tabs) {
  // Record the vector of actionable page contexts that we notify to observers
  actionable_tabs_[resource_type] = tabs;
  for (auto& obs : actionable_tab_observers_[resource_type]) {
    obs.OnActionableTabListChanged(resource_type, tabs);
  }
}

void PerformanceDetectionManager::MaybeNotifyAllActionableObservers(
    ResourceType resource_type,
    ActionableTabsObserver* new_observer,
    ActionableTabsResult result) {
  if (result != actionable_tabs_[resource_type]) {
    NotifyActionableTabObservers(resource_type, result);
  } else {
    new_observer->OnActionableTabListChanged(resource_type, result);
  }
}

void PerformanceDetectionManager::MaybeNotifyStatusAndActionabilityChange(
    ResourceType resource_type,
    bool did_status_change,
    ActionableTabsResult measurements) {
  if (did_status_change) {
    NotifyStatusObservers(resource_type, !measurements.empty());
  }

  if (actionable_tabs_[resource_type] != measurements) {
    NotifyActionableTabObservers(resource_type, measurements);
  }
}

}  // namespace performance_manager::user_tuning
