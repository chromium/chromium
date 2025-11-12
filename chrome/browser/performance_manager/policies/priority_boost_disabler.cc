// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/priority_boost_disabler.h"

#include <windows.h>

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/public/scenarios/process_performance_scenario_observer.h"
#include "components/performance_manager/scenario_api/performance_scenario_observer.h"
#include "components/performance_manager/scenario_api/performance_scenarios.h"
#include "content/public/common/process_type.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace performance_manager {

namespace {

void DisableBoostIfValid(const ProcessNode* process_node) {
  const base::Process& process = process_node->GetProcess();
  if (process.IsValid()) {
    // The second argument to this function *disables* boosting if true. See
    // https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-setprocesspriorityboost
    ::SetProcessPriorityBoost(process.Handle(), /*bDisablePriorityBoost=*/true);
  }
}

}  // namespace

// Disables priority boosting on processes after they have finished loading
// for the first time.
class PriorityBoostDisabler::ProcessLoadingScenarioObserver
    : public performance_scenarios::PerformanceScenarioObserver {
 public:
  explicit ProcessLoadingScenarioObserver(const ProcessNode* process_node)
      : process_node_(process_node) {
    scoped_observation_.Observe(
        &ProcessPerformanceScenarioObserverList::GetForProcess(process_node));
  }

  void OnLoadingScenarioChanged(
      performance_scenarios::ScenarioScope scope,
      performance_scenarios::LoadingScenario old_scenario,
      performance_scenarios::LoadingScenario new_scenario) override {
    // Disable priority boosting for the process after the first page load has
    // finished, which means it's changing from a loading scenario to
    // kNoPageLoading.
    if (new_scenario ==
        performance_scenarios::LoadingScenario::kNoPageLoading) {
      DisableBoostIfValid(process_node_);
      scoped_observation_.Reset();
    }
  }

  bool after_first_page_load() const {
    return !scoped_observation_.IsObserving();
  }

 private:
  base::ScopedObservation<ProcessPerformanceScenarioObserverList,
                          performance_scenarios::PerformanceScenarioObserver>
      scoped_observation_{this};
  raw_ptr<const ProcessNode> process_node_ = nullptr;
};

// PriorityBoostDisabler implementation
PriorityBoostDisabler::PriorityBoostDisabler() = default;

PriorityBoostDisabler::~PriorityBoostDisabler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// GraphOwned overrides
void PriorityBoostDisabler::OnPassedToGraph(performance_manager::Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->AddProcessNodeObserver(this);
  // Process all existing nodes in the graph.
  for (const performance_manager::ProcessNode* process_node :
       graph->GetAllProcessNodes()) {
    OnProcessNodeAdded(process_node);
  }
}

void PriorityBoostDisabler::OnTakenFromGraph(
    performance_manager::Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RemoveProcessNodeObserver(this);
}

// ProcessNodeObserver overrides
void PriorityBoostDisabler::OnProcessNodeAdded(
    const performance_manager::ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the process is not a renderer, try to disable boosting. The process may
  // not be valid yet. If it's not, OnProcessLifetimeChange() will take care of
  // it.
  if (process_node->GetProcessType() != content::PROCESS_TYPE_RENDERER) {
    DisableBoostIfValid(process_node);
    return;
  }

  // The scenario_observers_ map owns the ProcessLoadingScenarioObserver
  // objects.
  auto observer =
      std::make_unique<PriorityBoostDisabler::ProcessLoadingScenarioObserver>(
          process_node);
  scenario_observers_.emplace(process_node, std::move(observer));
}

void PriorityBoostDisabler::OnProcessLifetimeChange(
    const performance_manager::ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!process_node->GetProcess().IsValid()) {
    return;
  }

  if (process_node->GetProcessType() != content::PROCESS_TYPE_RENDERER) {
    DisableBoostIfValid(process_node);
    return;
  }

  // If the renderer process has already had their first page load, disable it
  // now that the process is valid.
  auto it = scenario_observers_.find(process_node);
  if (it != scenario_observers_.end() && it->second->after_first_page_load()) {
    DisableBoostIfValid(process_node);
    // Since it's not necessary to keep observing for this renderer process,
    // remove the observer.
    scenario_observers_.erase(it);
  }
}

void PriorityBoostDisabler::OnBeforeProcessNodeRemoved(
    const performance_manager::ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the process is not a renderer, it won't have an associated observer.
  if (process_node->GetProcessType() != content::PROCESS_TYPE_RENDERER) {
    return;
  }
  // Remove the ProcessLoadingScenarioObserver associated with this process.
  scenario_observers_.erase(process_node);
}

}  // namespace performance_manager
