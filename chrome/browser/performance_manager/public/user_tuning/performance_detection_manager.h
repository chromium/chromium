// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_PERFORMANCE_DETECTION_MANAGER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_PERFORMANCE_DETECTION_MANAGER_H_

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/enum_set.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/resource_attribution/cpu_proportion_tracker.h"
#include "components/performance_manager/public/resource_attribution/queries.h"
#include "components/system_cpu/cpu_probe.h"

class ChromeBrowserMainExtraPartsPerformanceManager;

namespace resource_attribution {
class PageContext;
}  // namespace resource_attribution

namespace system_cpu {
struct CpuSample;
}  // namespace system_cpu

namespace performance_manager::user_tuning {

class PerformanceDetectionManager {
 public:
  enum class ResourceType {
    kMemory = 0,
    kMinValue = kMemory,
    kCpu = 1,
    kNetwork = 2,
    kMaxValue = kNetwork,
  };

  enum class HealthLevel {
    kHealthy = 0,
    kDegraded = 1,
    kUnhealthy = 2,
  };

  using ResourceTypeSet = base::
      EnumSet<ResourceType, ResourceType::kMinValue, ResourceType::kMaxValue>;

  class StatusObserver : public base::CheckedObserver {
   public:
    // Called immediately with the current status when AddStatusObserver is
    // called, then again on changes (frequency determined by the backend).
    // RequestStatus() requests an OOB update with most recent status.
    virtual void OnStatusChanged(ResourceType resource_type,
                                 HealthLevel health_level,
                                 bool actionable) {}
  };

  class ActionableTabsObserver : public base::CheckedObserver {
   public:
    // Called immediately with the current status when AddTabListObserver is
    // called, then again on changes (frequency determined by the backend).
    // RequestTabList() requests an OOB update with most recent status.
    virtual void OnActionableTabListChanged(
        ResourceType resource_type,
        std::vector<resource_attribution::PageContext> tabs) {}
  };

  void AddStatusObserver(ResourceTypeSet resource_types, StatusObserver* o);
  void RemoveStatusObserver(StatusObserver* o);

  void AddActionableTabsObserver(ResourceTypeSet resource_types,
                                 ActionableTabsObserver* o);
  void RemoveActionableTabsObserver(ActionableTabsObserver* o);

  // Returns whether a PerformanceDetectionManager was created and installed.
  // Should only return false in unit tests.
  static bool HasInstance();
  static PerformanceDetectionManager* GetInstance();

  ~PerformanceDetectionManager();

 private:
  friend class ::ChromeBrowserMainExtraPartsPerformanceManager;
  friend class PerformanceDetectionManagerTest;
  FRIEND_TEST_ALL_PREFIXES(PerformanceDetectionManagerTest,
                           RecordCpuAndUpdateHealthStatus);
  FRIEND_TEST_ALL_PREFIXES(PerformanceDetectionManagerTest, CpuStatusUpdates);
  FRIEND_TEST_ALL_PREFIXES(PerformanceDetectionManagerTest,
                           HealthyCpuUsageFromProbe);

  PerformanceDetectionManager();

  HealthLevel GetHealthLevelForTesting(ResourceType resource_type);

  void ProcessCpuProbeResult(std::optional<system_cpu::CpuSample> cpu_sample);

  // Returns the associated health level for the given cpu utilization
  // percentage.
  HealthLevel GetCpuHealthStatus(int cpu_usage_percentage);

  // Keeps track of the health level associated with 'cpu_usage_percentage' and
  // updates the CPU health status if we see this health level for a certain
  // duration. Returns true if the cpu health status has changed.
  bool RecordAndUpdateCpuHealthStatus(int cpu_usage_percentage);

  // Processes 'results' and notifies observers if the health status changes
  void ProcessQueryResultMap(
      int system_cpu_usage_percentage,
      const resource_attribution::QueryResultMap& results);

  // Notify all status observers of the current health status for
  // 'resource_type'.
  void NotifyStatusObservers(ResourceType resource_type, bool is_actionable);

  std::map<ResourceType, base::ObserverList<StatusObserver>> status_observers_;
  // Number of samples in a time window being used to consider the new health
  // status
  const size_t cpu_health_sample_window_size_;
  base::circular_deque<HealthLevel> recent_cpu_health_levels_;
  resource_attribution::ScopedResourceUsageQuery scoped_cpu_query_;
  base::flat_map<ResourceType, HealthLevel> current_health_status_;
  base::RepeatingTimer cpu_probe_timer_;
  resource_attribution::CPUProportionTracker page_cpu_proportion_tracker_;
  base::WeakPtrFactory<PerformanceDetectionManager> weak_ptr_factory_{this};
};

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_PERFORMANCE_DETECTION_MANAGER_H_
