// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_USER_TUNING_CPU_HEALTH_TRACKER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_USER_TUNING_CPU_HEALTH_TRACKER_H_

#include <map>
#include <optional>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/types/strong_alias.h"
#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/resource_attribution/cpu_proportion_tracker.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "components/performance_manager/public/resource_attribution/queries.h"
#include "components/performance_manager/public/resource_attribution/query_results.h"
#include "components/system_cpu/cpu_probe.h"
#include "content/public/browser/resource_context.h"

namespace performance_manager::user_tuning {

class CpuHealthTracker
    : public performance_manager::GraphOwnedAndRegistered<CpuHealthTracker> {
 public:
  using ResourceType = PerformanceDetectionManager::ResourceType;
  using HealthLevel = PerformanceDetectionManager::HealthLevel;
  using ActionableTabsResult =
      PerformanceDetectionManager::ActionableTabsResult;
  using StatusChangeCallback =
      base::RepeatingCallback<void(ResourceType, HealthLevel, bool)>;
  using ActionableTabResultCallback =
      base::RepeatingCallback<void(ResourceType, ActionableTabsResult)>;
  using CpuPercent = base::StrongAlias<class CpuPercentTag, int>;

  CpuHealthTracker(StatusChangeCallback on_status_change_cb,
                   ActionableTabResultCallback on_actionability_change_cb);
  ~CpuHealthTracker() override;

  HealthLevel GetCurrentHealthLevel();

  int GetTotalCpuPercentUsage(ActionableTabsResult tabs);

  // Queries and process tab CPU data. This data is recorded and may invoke the
  // status change and actionability change callback if the processed tab CPU
  // data meets the criteria to be actionable.
  void QueryAndProcessTabActionability(
      std::optional<CpuPercent> system_cpu_usage_percentage);

 private:
  friend class CpuHealthTrackerTestHelper;
  friend class CpuHealthTrackerTest;
  friend class CpuHealthTrackerBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(CpuHealthTrackerTest,
                           RecordCpuAndUpdateHealthStatus);
  FRIEND_TEST_ALL_PREFIXES(CpuHealthTrackerTest, HealthyCpuUsageFromProbe);
  FRIEND_TEST_ALL_PREFIXES(CpuHealthTrackerBrowserTest,
                           PagesMeetMinimumCpuUsage);

  using PageResourceMeasurements =
      base::flat_map<resource_attribution::PageContext, CpuPercent>;

  base::OnceCallback<void(ActionableTabsResult)>
  GetStatusAndActionabilityCallback(bool did_status_change,
                                    HealthLevel health_level);

  // Returns the health level associated with the measurement
  HealthLevel GetHealthLevelForMeasurement(CpuPercent measurement);

  // Calls `callback` with a vector of actionable tabs that is determined from
  // 'unfiltered_measurements'.
  void GetFilteredActionableTabs(
      PageResourceMeasurements unfiltered_measurements,
      CpuPercent recent_measurement,
      base::OnceCallback<void(ActionableTabsResult)> callback);

  // Keeps track of the given measurement and potentially update the health
  // level if the tracker consistently records that level
  bool RecordAndUpdateHealthStatus(CpuPercent measurement);

  void ProcessCpuProbeResult(std::optional<system_cpu::CpuSample> cpu_sample);

  // Processes 'results' and notifies observers if the health status changes
  void ProcessQueryResultMap(
      CpuPercent system_cpu_usage_percentage,
      const resource_attribution::QueryResultMap& results);

  // Filter 'page_cpu' for pages that meet the minimum CPU usage to be
  // actionable and returns the result as a map of page contexts with its
  // corresponding CPU usage percentage. CPU usage percentages are converted
  // to range from 0-100%.
  PageResourceMeasurements FilterForPossibleActionablePages(
      std::map<resource_attribution::ResourceContext, double> page_cpu);

  bool CanDiscardPage(resource_attribution::PageContext context);

  // Health tracker automatically call these callbacks after CPU metrics
  // refreshes and the status or tab actionability changes.
  StatusChangeCallback status_change_cb_;
  ActionableTabResultCallback actionable_tabs_cb_;

  ActionableTabsResult actionable_tabs_;

  // Map containing all non-off record tab page contexts and their
  // corresponding resource measurements since the last measurement interval.
  // Some tabs are not actionable since their CPU usage may be lower than
  // the minimum to be considered as actionable.
  PageResourceMeasurements tab_page_measurements_;

  // Number of samples in a time window being used to consider the new health
  // status.
  const size_t cpu_health_sample_window_size_;

  const bool is_demo_mode_;

  // Recent resource measurements used to determine overall resource health.
  base::circular_deque<CpuPercent> recent_resource_measurements_;
  CpuPercent min_resource_measurement_ = CpuPercent(0);

  resource_attribution::ScopedResourceUsageQuery scoped_cpu_query_;
  HealthLevel current_health_status_ = HealthLevel::kHealthy;
  base::RepeatingTimer cpu_probe_timer_;
  resource_attribution::CPUProportionTracker page_cpu_proportion_tracker_;
  base::WeakPtrFactory<CpuHealthTracker> weak_ptr_factory_{this};
};

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_USER_TUNING_CPU_HEALTH_TRACKER_H_
