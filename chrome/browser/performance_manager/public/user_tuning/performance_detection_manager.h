// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_PERFORMANCE_DETECTION_MANAGER_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_PERFORMANCE_DETECTION_MANAGER_H_

#include <map>
#include <vector>

#include "base/containers/enum_set.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"

class ChromeBrowserMainExtraPartsPerformanceManager;

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

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class HealthLevel {
    kHealthy = 0,
    kMinValue = kHealthy,
    kDegraded = 1,
    kUnhealthy = 2,
    kMaxValue = kUnhealthy,
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

  // Discards all eligible pages in `tabs` and runs `post_discard_cb`
  // after the discard finishes. `post_discard_cb` must be valid to
  // run on the UI sequence.
  void DiscardTabs(ActionableTabsResult tabs,
                   base::OnceCallback<void(bool)> post_discard_cb =
                       base::OnceCallback<void(bool)>());

  void ForceTabCpuDataRefresh();

  void NotifyActionableTabObserversForTesting(ResourceType resource_type,
                                              const ActionableTabsResult& tabs);

  // Returns whether a PerformanceDetectionManager was created and installed.
  // Should only return false in unit tests.
  static bool HasInstance();
  static PerformanceDetectionManager* GetInstance();

  ~PerformanceDetectionManager();

 private:
  friend class ::ChromeBrowserMainExtraPartsPerformanceManager;
  friend class PerformanceDetectionManagerTest;
  friend class CpuHealthTrackerBrowserTest;

  PerformanceDetectionManager();

  // Notify all status observers of the current health status for
  // 'resource_type'.
  void NotifyStatusObservers(ResourceType resource_type,
                             HealthLevel new_level,
                             bool is_actionable);
  void NotifyActionableTabObservers(ResourceType resource_type,
                                    ActionableTabsResult tabs);

  void OnDiscardComplete();
  void RecordCpuHealthStatus(base::TimeDelta time_after_discard);

  std::map<ResourceType, base::ObserverList<StatusObserver>> status_observers_;
  std::map<ResourceType, base::ObserverList<ActionableTabsObserver>>
      actionable_tab_observers_;
  base::flat_map<ResourceType, ActionableTabsResult> actionable_tabs_;
  base::flat_map<ResourceType, HealthLevel> current_health_status_;

  base::OneShotTimer one_minute_discard_timer_;
  base::OneShotTimer two_minute_discard_timer_;
  base::OneShotTimer four_minute_discard_timer_;

  base::WeakPtrFactory<PerformanceDetectionManager> weak_ptr_factory_{this};
};

}  // namespace performance_manager::user_tuning

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_USER_TUNING_PERFORMANCE_DETECTION_MANAGER_H_
