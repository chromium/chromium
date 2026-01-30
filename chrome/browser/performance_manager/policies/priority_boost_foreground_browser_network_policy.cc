// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/priority_boost_foreground_browser_network_policy.h"

#include "base/check.h"
#include "base/process/process.h"
#include "chrome/browser/performance_manager/policies/priority_boost_helpers.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "content/public/common/process_type.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace performance_manager::policies {

PriorityBoostForegroundBrowserNetworkPolicy::
    PriorityBoostForegroundBrowserNetworkPolicy() = default;

PriorityBoostForegroundBrowserNetworkPolicy::
    ~PriorityBoostForegroundBrowserNetworkPolicy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PriorityBoostForegroundBrowserNetworkPolicy::OnPassedToGraph(
    Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->AddProcessNodeObserver(this);
  CHECK(graph->HasOnlySystemNode());
}

void PriorityBoostForegroundBrowserNetworkPolicy::OnTakenFromGraph(
    Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RemoveProcessNodeObserver(this);
}

void PriorityBoostForegroundBrowserNetworkPolicy::OnProcessNodeAdded(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ApplyStartupPolicy(process_node);
}

void PriorityBoostForegroundBrowserNetworkPolicy::OnProcessLifetimeChange(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ApplyStartupPolicy(process_node);
}

void PriorityBoostForegroundBrowserNetworkPolicy::OnBeforeProcessNodeRemoved(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void PriorityBoostForegroundBrowserNetworkPolicy::OnPriorityChanged(
    const ProcessNode* process_node,
    base::Process::Priority previous_value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (process_node->GetProcessType() != content::PROCESS_TYPE_RENDERER) {
    return;
  }
  CHECK(process_node->GetPriority() != previous_value);
  ApplyRendererPriorityPolicy(process_node);
}

void PriorityBoostForegroundBrowserNetworkPolicy::ApplyStartupPolicy(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Permanently disable priority boosting for all processes, except the
  // browser, network, and renderer processes.
  if (process_node->GetProcessType() == content::PROCESS_TYPE_BROWSER ||
      (process_node->GetProcessType() == content::PROCESS_TYPE_UTILITY &&
       process_node->GetMetricsName() ==
           network::mojom::NetworkService::Name_)) {
    return;
  }
  if (process_node->GetProcessType() == content::PROCESS_TYPE_RENDERER) {
    ApplyRendererPriorityPolicy(process_node);
    return;
  }
  SetDisableBoostIfValid(process_node, true);
}

void PriorityBoostForegroundBrowserNetworkPolicy::ApplyRendererPriorityPolicy(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(process_node->GetProcessType() == content::PROCESS_TYPE_RENDERER);
  if (process_node->GetPriority() == base::Process::Priority::kBestEffort) {
    // If the process is in the background (kBestEffort), disable
    // boosting.
    SetDisableBoostIfValid(process_node, true);
  } else {
    // If the process is in the foreground (kUserVisible or
    // kUserBlocking), enable boosting.
    SetDisableBoostIfValid(process_node, false);
  }
}

}  // namespace performance_manager::policies
