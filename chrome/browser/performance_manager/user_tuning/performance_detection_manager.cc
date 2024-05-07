// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/user_tuning/performance_detection_manager.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/bind_post_task.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "chrome/browser/performance_manager/user_tuning/cpu_health_tracker.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_state.mojom-shared.h"
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

void PerformanceDetectionManager::DiscardTabs(
    std::vector<resource_attribution::PageContext> tabs,
    base::OnceCallback<void(bool)> post_discard_cb) {
  PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](std::vector<resource_attribution::PageContext> tabs,
             base::OnceCallback<void(bool)> post_discard_cb, Graph* graph) {
            std::vector<const PageNode*> eligible_nodes;
            for (resource_attribution::PageContext context : tabs) {
              const PageNode* page_node = context.GetPageNode();
              if (page_node) {
                eligible_nodes.emplace_back(page_node);
              }
            }
            policies::PageDiscardingHelper* const helper =
                policies::PageDiscardingHelper::GetFromGraph(graph);
            helper->ImmediatelyDiscardMultiplePages(
                eligible_nodes, ::mojom::LifecycleUnitDiscardReason::SUGGESTED,
                std::move(post_discard_cb));
          },
          std::move(tabs),
          post_discard_cb.is_null()
              ? base::DoNothing()
              : base::BindPostTask(content::GetUIThreadTaskRunner({}),
                                   std::move(post_discard_cb))));
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

  const std::vector<ResourceType> resource_types =
      base::ToVector(ResourceTypeSet::All());
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

  performance_manager::PerformanceManager::CallOnGraph(
      FROM_HERE,
      base::BindOnce(
          [](CpuHealthTracker::StatusChangeCallback on_status_change,
             CpuHealthTracker::ActionableTabResultCallback
                 on_actionable_list_change,
             Graph* graph) {
            std::unique_ptr<CpuHealthTracker> cpu_health_tracker =
                std::make_unique<CpuHealthTracker>(
                    std::move(on_status_change),
                    std::move(on_actionable_list_change));

            graph->PassToGraph(std::move(cpu_health_tracker));
          },
          std::move(on_status_change), std::move(on_actionable_list_change)));
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
  CHECK(actionable_tabs != tabs);
  actionable_tabs = tabs;
  for (auto& obs : actionable_tab_observers_[resource_type]) {
    obs.OnActionableTabListChanged(resource_type,
                                   actionable_tabs_[resource_type]);
  }
}
}  // namespace performance_manager::user_tuning
