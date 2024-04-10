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
  using ActionableTabsResult = std::vector<resource_attribution::PageContext>;

  class StatusObserver : public base::CheckedObserver {
   public:
    // Called immediately with the current status when AddStatusObserver is
    // called, then again on changes (frequency determined by the backend).
    virtual void OnStatusChanged(ResourceType resource_type,
                                 HealthLevel health_level,
                                 bool actionable) {}
  };

  class ActionableTabsObserver : public base::CheckedObserver {
   public:
    // Called immediately with the current status when AddTabListObserver is
    // called, then again on changes (frequency determined by the backend).
    virtual void OnActionableTabListChanged(ResourceType resource_type,
                                            ActionableTabsResult tabs) {}
  };

  void AddStatusObserver(ResourceTypeSet resource_types,
                         StatusObserver* observer);
  void RemoveStatusObserver(StatusObserver* o);

  void AddActionableTabsObserver(ResourceTypeSet resource_types,
                                 ActionableTabsObserver* new_observer);
  void RemoveActionableTabsObserver(ActionableTabsObserver* o);

  // Returns whether a PerformanceDetectionManager was created and installed.
  // Should only return false in unit tests.
  static bool HasInstance();
  static PerformanceDetectionManager* GetInstance();

  ~PerformanceDetectionManager();

 private:
  using PageResourceMeasurements =
      base::flat_map<resource_attribution::PageContext, int>;
  using ActionableTabResultCallback =
      base::OnceCallback<void(ActionableTabsResult)>;

  friend class ::ChromeBrowserMainExtraPartsPerformanceManager;
  friend class PerformanceDetectionManagerTest;
  FRIEND_TEST_ALL_PREFIXES(PerformanceDetectionManagerTest,
                           RecordCpuAndUpdateHealthStatus);
  FRIEND_TEST_ALL_PREFIXES(PerformanceDetectionManagerTest, CpuStatusUpdates);
  FRIEND_TEST_ALL_PREFIXES(PerformanceDetectionManagerTest,
                           HealthyCpuUsageFromProbe);
  FRIEND_TEST_ALL_PREFIXES(PerformanceDetectionManagerTest,
                           GetPagesMeetMinimumCpuUsage);

  PerformanceDetectionManager();

  HealthLevel GetHealthLevelForTesting(ResourceType resource_type);

  void ProcessCpuProbeResult(std::optional<system_cpu::CpuSample> cpu_sample);

  // Keeps track of the health level associated with 'cpu_usage_percentage' and
  // updates the CPU health status if we see this health level for a certain
  // duration. Returns true if the cpu health status has changed.
  bool RecordAndUpdateCpuHealthStatus(int cpu_usage_percentage);

  // Processes 'results' and notifies observers if the health status changes
  void ProcessQueryResultMap(
      int system_cpu_usage_percentage,
      const resource_attribution::QueryResultMap& results);

  // Returns the associated health level for the given cpu utilization
  // percentage.
  HealthLevel GetCpuHealthStatus(int cpu_usage_percentage);

  // Filter 'page_cpu' for pages that meet the minimum CPU usage to be
  // actionable and returns the result as a map of page contexts with its
  // corresponding CPU usage percentage. CPU usage percentages are converted
  // to range from 0-100%.
  PageResourceMeasurements GetPagesMeetMinimumCpuUsage(
      std::map<resource_attribution::ResourceContext, double> page_cpu);

  // Takes in a map of page resource measurements and filters out which page
  // contexts are actionable. Runs 'on_results_callback' with the vector of
  // actionable tabs.
  void GetActionablePages(ResourceType resource_type,
                          const PageResourceMeasurements& page_measurements,
                          int overall_resource_usage,
                          ActionableTabResultCallback on_results_callback);

  // Notify all status observers of the current health status for
  // 'resource_type'.
  void NotifyStatusObservers(ResourceType resource_type, bool is_actionable);
  void NotifyActionableTabObservers(ResourceType resource_type,
                                    ActionableTabsResult tabs);

  // If the actionable tabs result list changes, then notify all observers that
  // the list changes otherwise only notify the provided observer. The provided
  // observer cannot be null.
  void MaybeNotifyAllActionableObservers(ResourceType resource_type,
                                         ActionableTabsObserver* new_observer,
                                         ActionableTabsResult result);

  // Notifies the status and actionable tab list observers if the
  // resource status changes and/or the actionable tab list changes.
  void MaybeNotifyStatusAndActionabilityChange(ResourceType resource_type,
                                               bool did_status_change,
                                               ActionableTabsResult tabs);

  std::map<ResourceType, base::ObserverList<StatusObserver>> status_observers_;
  std::map<ResourceType, base::ObserverList<ActionableTabsObserver>>
      actionable_tab_observers_;
  base::flat_map<ResourceType, std::vector<resource_attribution::PageContext>>
      actionable_tabs_;

  // Map containing all page contexts and their corresponding resource
  // measurements that are possibly actionable.
  base::flat_map<ResourceType, PageResourceMeasurements>
      possible_actionable_pages_;

  // Number of samples in a time window being used to consider the new health
  // status.
  const size_t cpu_health_sample_window_size_;

  // Recent resource measurements used to determine overall resource health.
  base::flat_map<ResourceType, base::circular_deque<int>>
      recent_resource_measurements_;

  resource_attribution::ScopedResourceUsageQuery scoped_cpu_query_;
  base::flat_map<ResourceType, HealthLevel> current_health_status_;
  base::RepeatingTimer cpu_probe_timer_;
  resource_attribution::CPUProportionTracker page_cpu_proportion_tracker_;
  base::WeakPtrFactory<PerformanceDetectionManager> weak_ptr_factory_{this};
};

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_PERFORMANCE_DETECTION_MANAGER_H_
