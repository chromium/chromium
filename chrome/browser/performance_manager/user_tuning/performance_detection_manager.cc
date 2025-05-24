// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/bind_post_task.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/performance_manager/user_tuning/cpu_health_tracker.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
#include "chrome/browser/ui/performance_controls/performance_controls_metrics.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/resource_attribution/page_context.h"
#include "content/public/browser/browser_thread.h"

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
                              current_health_status_[resource_type],
                              !actionable_tabs_[resource_type].empty());
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
    new_observer->OnActionableTabListChanged(resource_type,
                                             actionable_tabs_[resource_type]);
  }
}

void PerformanceDetectionManager::RemoveActionableTabsObserver(
    ActionableTabsObserver* o) {
  for (auto& [resource_type, observer_list] : actionable_tab_observers_) {
    observer_list.RemoveObserver(o);
  }
}

bool PerformanceDetectionManager::DiscardTabs(
    std::vector<resource_attribution::PageContext> tabs) {
  Graph* graph = PerformanceManager::GetGraph();

  std::vector<const PageNode*> eligible_nodes;
  std::vector<resource_attribution::PageContext> eligible_page_contexts;
  for (const resource_attribution::PageContext& context : tabs) {
    const PageNode* page_node = context.GetPageNode();
    if (page_node) {
      eligible_nodes.emplace_back(page_node);
      eligible_page_contexts.emplace_back(context);
    }
  }

  performance_manager::user_tuning::CpuHealthTracker* const health_tracker =
      performance_manager::user_tuning::CpuHealthTracker::GetFromGraph(graph);

  RecordCpuUsageBeforeDiscard(
      health_tracker->GetTotalCpuPercentUsage(eligible_page_contexts));
  health_level_before_discard_ = health_tracker->GetCurrentHealthLevel();

  policies::PageDiscardingHelper* const helper =
      policies::PageDiscardingHelper::GetFromGraph(graph);
  CHECK(helper);
  const bool had_successful_discard = helper->ImmediatelyDiscardMultiplePages(
      eligible_nodes, ::mojom::LifecycleUnitDiscardReason::SUGGESTED);

  OnDiscardComplete();
  return had_successful_discard;
}

void PerformanceDetectionManager::ForceTabCpuDataRefresh() {
  Graph* graph = PerformanceManager::GetGraph();
  CpuHealthTracker* const health_tracker =
      CpuHealthTracker::GetFromGraph(graph);
  health_tracker->QueryAndProcessTabActionability(std::nullopt);
}

void PerformanceDetectionManager::OnDiscardComplete() {
  // base::Unretained(this) is safe here because the timers are owned by the
  // PerformanceDetectionManager so the callback will not be invoked after this
  // is destroyed.
  // If a timer is still running and another discard occurs, we will restart the
  // timer instead to record the health status after the most recent discard.
  // This may cause different counts for the one, two, and four minute timers.
  discard_timer_.Start(
      FROM_HERE, base::Minutes(1),
      base::BindRepeating(&PerformanceDetectionManager::RecordCpuHealthStatus,
                          base::Unretained(this), base::Minutes(1)));
}

void PerformanceDetectionManager::RecordCpuHealthStatus(
    base::TimeDelta time_after_discard) {
  if (time_after_discard == base::Minutes(1)) {
    // Start the timer for another minute to record health status 2 minutes
    // after discard
    discard_timer_.Start(
        FROM_HERE, base::Minutes(1),
        base::BindRepeating(&PerformanceDetectionManager::RecordCpuHealthStatus,
                            base::Unretained(this), base::Minutes(2)));
  } else if (time_after_discard == base::Minutes(2)) {
    // Start the timer for another 2 minutes to record health status 4 minutes
    // after discard
    discard_timer_.Start(
        FROM_HERE, base::Minutes(2),
        base::BindRepeating(&PerformanceDetectionManager::RecordCpuHealthStatus,
                            base::Unretained(this), base::Minutes(4)));
  }

  Graph* const graph = PerformanceManager::GetGraph();
  CpuHealthTracker* const health_tracker =
      performance_manager::user_tuning::CpuHealthTracker::GetFromGraph(graph);
  PerformanceDetectionManager::HealthLevel current_health_level =
      health_tracker->GetCurrentHealthLevel();

  CHECK(health_level_before_discard_.has_value());
  RecordCpuHealthStatusChange(time_after_discard,
                              health_level_before_discard_.value(),
                              current_health_level);
  RecordCpuHealthStatusAfterDiscard(time_after_discard, current_health_level);
}

void PerformanceDetectionManager::NotifyActionableTabObserversForTesting(
    ResourceType resource_type,
    const ActionableTabsResult& tabs) {
  NotifyActionableTabObservers(resource_type, tabs);
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

PerformanceDetectionManager::PerformanceDetectionManager() {
  CHECK(!g_performance_detection_manager);
  g_performance_detection_manager = this;

  const auto resource_types = ResourceTypeSet::All();
  current_health_status_ = base::MakeFlatMap<ResourceType, HealthLevel>(
      resource_types, {}, [](ResourceType type) {
        return std::make_pair(type, HealthLevel::kHealthy);
      });

  actionable_tabs_ = base::MakeFlatMap<ResourceType, ActionableTabsResult>(
      resource_types, {}, [](ResourceType type) {
        return std::make_pair(type, ActionableTabsResult());
      });

  CpuHealthTracker::StatusChangeCallback on_status_change = base::BindPostTask(
      content::GetUIThreadTaskRunner({}),
      base::BindRepeating(&PerformanceDetectionManager::NotifyStatusObservers,
                          weak_ptr_factory_.GetWeakPtr()));

  CpuHealthTracker::ActionableTabResultCallback on_actionable_list_change =
      base::BindPostTask(
          content::GetUIThreadTaskRunner({}),
          base::BindRepeating(
              &PerformanceDetectionManager::NotifyActionableTabObservers,
              weak_ptr_factory_.GetWeakPtr()));

  Graph* graph = PerformanceManager::GetGraph();

  std::unique_ptr<CpuHealthTracker> cpu_health_tracker =
      std::make_unique<CpuHealthTracker>(std::move(on_status_change),
                                         std::move(on_actionable_list_change));
  graph->PassToGraph(std::move(cpu_health_tracker));
}

void PerformanceDetectionManager::NotifyStatusObservers(
    ResourceType resource_type,
    HealthLevel new_level,
    bool is_actionable) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto& current_health = current_health_status_[resource_type];
  CHECK_NE(current_health, new_level);
  current_health = new_level;
  for (auto& obs : status_observers_[resource_type]) {
    obs.OnStatusChanged(resource_type, current_health_status_[resource_type],
                        is_actionable);
  }
}

void PerformanceDetectionManager::NotifyActionableTabObservers(
    ResourceType resource_type,
    ActionableTabsResult tabs) {
  CHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto& actionable_tabs = actionable_tabs_[resource_type];

  // It is possible for the same actionable tabs to be surfaced again while in
  // demo mode.
  if (!base::FeatureList::IsEnabled(
          features::kPerformanceInterventionDemoMode)) {
    CHECK(actionable_tabs != tabs);
  }
  actionable_tabs = tabs;
  for (auto& obs : actionable_tab_observers_[resource_type]) {
    obs.OnActionableTabListChanged(resource_type,
                                   actionable_tabs_[resource_type]);
  }
}
}  // namespace performance_manager::user_tuning
