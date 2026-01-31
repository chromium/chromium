// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/priority_boost_loading_browser_network_policy.h"

#include "base/check.h"
#include "base/process/process.h"
#include "base/scoped_observation.h"
#include "chrome/browser/performance_manager/policies/priority_boost_helpers.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/scenarios/process_performance_scenario_observer.h"
#include "components/performance_manager/public/scenarios/process_performance_scenarios.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "content/public/common/process_type.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace performance_manager::policies {

// Disables priority boosting on processes while they are loading, and enables
// it again once the loading is finished.
class PriorityBoostLoadingBrowserNetworkPolicy::ProcessLoadingScenarioObserver
    : public performance_scenarios::PerformanceScenarioObserver {
 public:
  explicit ProcessLoadingScenarioObserver(
      PriorityBoostLoadingBrowserNetworkPolicy* policy,
      const ProcessNode* process_node)
      : policy_(policy), process_node_(process_node) {
    scoped_observation_.Observe(
        &ProcessPerformanceScenarioObserverList::GetForProcess(process_node));
  }

  void OnLoadingScenarioChanged(
      performance_scenarios::ScenarioScope scope,
      performance_scenarios::LoadingScenario old_scenario,
      performance_scenarios::LoadingScenario new_scenario) override {
    // Enable priority boosting for the process if it starts loading, which
    // means it changed from kNoPageLoading to any of the other
    // LoadingScenarios. Undo this setting once the loading ends, meaning the
    // scenario changed back to kNoPageLoading.
    if (old_scenario ==
        performance_scenarios::LoadingScenario::kNoPageLoading) {
      policy_->OnRendererStartedLoading();
      SetDisableBoostIfValid(process_node_, false);
    } else if (new_scenario ==
               performance_scenarios::LoadingScenario::kNoPageLoading) {
      policy_->OnRendererStoppedLoading();
      SetDisableBoostIfValid(process_node_, true);
    }
  }

 private:
  base::ScopedObservation<ProcessPerformanceScenarioObserverList,
                          performance_scenarios::PerformanceScenarioObserver>
      scoped_observation_{this};
  const raw_ptr<PriorityBoostLoadingBrowserNetworkPolicy> policy_;
  raw_ptr<const ProcessNode> process_node_ = nullptr;
};

PriorityBoostLoadingBrowserNetworkPolicy::
    PriorityBoostLoadingBrowserNetworkPolicy() = default;

PriorityBoostLoadingBrowserNetworkPolicy::
    ~PriorityBoostLoadingBrowserNetworkPolicy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PriorityBoostLoadingBrowserNetworkPolicy::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->AddProcessNodeObserver(this);
  CHECK(graph->HasOnlySystemNode());
}

void PriorityBoostLoadingBrowserNetworkPolicy::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RemoveProcessNodeObserver(this);
}

void PriorityBoostLoadingBrowserNetworkPolicy::OnProcessNodeAdded(
    const ProcessNode* process_node) {
  ApplyPolicy(process_node);
}

void PriorityBoostLoadingBrowserNetworkPolicy::OnProcessLifetimeChange(
    const ProcessNode* process_node) {
  ApplyPolicy(process_node);

  if (process_node->GetProcessType() == content::PROCESS_TYPE_RENDERER) {
    // If the process is already loading by the time it becomes valid, enable
    // boosting and update the counter.
    if (GetProcessLoadingScenario(process_node) !=
        performance_scenarios::LoadingScenario::kNoPageLoading) {
      SetDisableBoostIfValid(process_node, false);
      OnRendererStartedLoading();
    }
  }
}

void PriorityBoostLoadingBrowserNetworkPolicy::OnBeforeProcessNodeRemoved(
    const performance_manager::ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (process_node->GetProcessType() == content::PROCESS_TYPE_UTILITY &&
      process_node->GetMetricsName() == network::mojom::NetworkService::Name_) {
    network_process_node_ = nullptr;
    return;
  }

  // If the process is not a renderer, it won't have an associated observer.
  if (process_node->GetProcessType() != content::PROCESS_TYPE_RENDERER) {
    return;
  }
  // If the process was loading when it was removed, decrement the loading
  // renderers count.
  if (GetProcessLoadingScenario(process_node) !=
      performance_scenarios::LoadingScenario::kNoPageLoading) {
    OnRendererStoppedLoading();
  }
  // Remove the ProcessLoadingScenarioObserver associated with this process.
  scenario_observers_.erase(process_node);
}

void PriorityBoostLoadingBrowserNetworkPolicy::AddLoadingObserver(
    const ProcessNode* process_node) {
  // The scenario_observers_ map owns the ProcessLoadingScenarioObserver
  // objects.
  auto [it, success] = scenario_observers_.try_emplace(process_node, nullptr);
  if (success) {
    it->second =
        std::make_unique<ProcessLoadingScenarioObserver>(this, process_node);
  }
}

void PriorityBoostLoadingBrowserNetworkPolicy::ApplyPolicy(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (process_node->GetProcessType() == content::PROCESS_TYPE_BROWSER) {
    return;
  }
  if (process_node->GetProcessType() == content::PROCESS_TYPE_UTILITY &&
      process_node->GetMetricsName() == network::mojom::NetworkService::Name_) {
    network_process_node_ = process_node;
    // If at least one renderer is loading, enable priority boosting. Otherwise,
    // disable it.
    SetDisableBoostIfValid(network_process_node_,
                           loading_renderers_count_ == 0);
    return;
  }
  if (process_node->GetProcessType() == content::PROCESS_TYPE_RENDERER) {
    AddLoadingObserver(process_node);
  }
  SetDisableBoostIfValid(process_node, true);
}

void PriorityBoostLoadingBrowserNetworkPolicy::OnRendererStartedLoading() {
  if (loading_renderers_count_ == 0 && network_process_node_) {
    // The first renderer started loading, so enable priority boosting for the
    // network process.
    SetDisableBoostIfValid(network_process_node_, false);
  }
  loading_renderers_count_++;
}

void PriorityBoostLoadingBrowserNetworkPolicy::OnRendererStoppedLoading() {
  loading_renderers_count_--;
  if (loading_renderers_count_ == 0 && network_process_node_) {
    // No renderers are loading now, so disable priority boosting for the
    // network process.
    SetDisableBoostIfValid(network_process_node_, true);
  }
}

}  // namespace performance_manager::policies
