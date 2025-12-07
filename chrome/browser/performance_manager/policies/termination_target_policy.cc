// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/termination_target_policy.h"

#include <algorithm>
#include <cstdint>

#include "base/byte_count.h"
#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "content/public/common/process_type.h"

namespace performance_manager {

// Private memory footprint threshold above which a process is considered large.
constexpr base::ByteCount kLargeProcessFootprintThreshold = base::MiB(100);

namespace {

struct PotentialTerminationTarget {
  raw_ptr<const ProcessNode> process = nullptr;
  base::ByteCount private_footprint;
  bool can_discard = true;
  // Last time a page in the process was visible. `base::TimeTicks::Max()` if
  // currently visible. `base::TimeTicks()` if there is no visible page in the
  // process.
  base::TimeTicks last_visible_time;

  // Returns true if `this` is a better termination target than `other`.
  bool operator<(const PotentialTerminationTarget& other) const {
    if (is_large() != other.is_large()) {
      return is_large();
    }

    if (can_discard != other.can_discard) {
      return can_discard;
    }

    return last_visible_time < other.last_visible_time;
  }

  bool is_large() const {
    return private_footprint > kLargeProcessFootprintThreshold;
  }
};

}  // namespace

TerminationTargetPolicy::TerminationTargetPolicy(
    std::unique_ptr<TerminationTargetSetter> termination_target_setter)
    : termination_target_setter_(std::move(termination_target_setter)) {}

TerminationTargetPolicy::~TerminationTargetPolicy() {
  if (current_termination_target_) {
    termination_target_setter_->SetTerminationTarget(nullptr);
  }
}

void TerminationTargetPolicy::OnPassedToGraph(Graph* graph) {
  graph->AddSystemNodeObserver(this);
  graph->AddPageNodeObserver(this);
  graph->AddProcessNodeObserver(this);
}

void TerminationTargetPolicy::OnTakenFromGraph(Graph* graph) {
  graph->RemoveProcessNodeObserver(this);
  graph->RemovePageNodeObserver(this);
  graph->RemoveSystemNodeObserver(this);
}

void TerminationTargetPolicy::OnIsVisibleChanged(const PageNode* page_node) {
  UpdateTerminationTarget();
}

void TerminationTargetPolicy::OnPageNodeRemoved(const PageNode* page_node) {
  UpdateTerminationTarget();
}

void TerminationTargetPolicy::OnBeforeProcessNodeRemoved(
    const ProcessNode* process_node) {
  if (process_node == current_termination_target_) {
    UpdateTerminationTarget(/*process_being_deleted=*/process_node);
  }
}

void TerminationTargetPolicy::OnProcessLifetimeChange(
    const ProcessNode* process_node) {
  if (process_node == current_termination_target_) {
    UpdateTerminationTarget(process_node);
  }
}

void TerminationTargetPolicy::OnProcessMemoryMetricsAvailable(
    const SystemNode* system_node) {
  UpdateTerminationTarget();
}

void TerminationTargetPolicy::UpdateTerminationTarget(
    const ProcessNode* process_being_deleted) {
  std::vector<PotentialTerminationTarget> targets;
  auto process_nodes = GetOwningGraph()->GetAllProcessNodes();
  targets.reserve(process_nodes.size());

  policies::DiscardEligibilityPolicy* eligibility_policy =
      policies::DiscardEligibilityPolicy::GetFromGraph(GetOwningGraph());

  for (const ProcessNode* process_node : process_nodes) {
    if (process_node->GetProcessType() != content::PROCESS_TYPE_RENDERER) {
      continue;
    }

    if (process_node == process_being_deleted) {
      continue;
    }

    if (!process_node->GetProcess().IsValid()) {
      continue;
    }

    targets.emplace_back();
    PotentialTerminationTarget& target = targets.back();
    target.process = process_node;
    target.private_footprint = process_node->GetPrivateFootprint();

    for (const FrameNode* frame_node : process_node->GetFrameNodes()) {
      const PageNode* page_node = frame_node->GetPageNode();
      if (eligibility_policy->CanDiscard(
              page_node,
              policies::DiscardEligibilityPolicy::DiscardReason::URGENT,
              /*minimum_time_in_background=*/base::TimeDelta()) !=
          policies::CanDiscardResult::kEligible) {
        target.can_discard = false;
      }
      if (page_node->IsVisible()) {
        target.last_visible_time = base::TimeTicks::Max();
      } else {
        target.last_visible_time = std::max(
            target.last_visible_time, page_node->GetLastVisibilityChangeTime());
      }
    }
  }

  auto min_element_it = std::min_element(targets.begin(), targets.end());
  if (min_element_it != targets.end()) {
    if (min_element_it->process != current_termination_target_) {
      termination_target_setter_->SetTerminationTarget(min_element_it->process);
      current_termination_target_ = min_element_it->process;
    }
  } else if (current_termination_target_) {
    termination_target_setter_->SetTerminationTarget(nullptr);
    current_termination_target_ = nullptr;
  }
}

}  // namespace performance_manager
