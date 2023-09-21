// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/oom_score_policy_chromeos.h"

#include <algorithm>
#include <set>

#include "base/process/memory.h"  // For AdjustOOMScore
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "content/public/common/content_constants.h"  // For kLowestRendererOomScore

namespace performance_manager {
namespace policies {

namespace {

constexpr base::TimeDelta kOomScoreAssignmentInterval = base::Seconds(10);

}  // namespace

OomScorePolicyChromeOS::OomScorePolicyChromeOS() = default;
OomScorePolicyChromeOS::~OomScorePolicyChromeOS() = default;

void OomScorePolicyChromeOS::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph_ = graph;
  timer_.Start(FROM_HERE, kOomScoreAssignmentInterval,
               base::BindRepeating(&OomScorePolicyChromeOS::AssignOomScores,
                                   weak_factory_.GetWeakPtr()));
}

void OomScorePolicyChromeOS::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph_ = nullptr;
  timer_.Stop();
}

void OomScorePolicyChromeOS::AssignOomScores() {
  PageDiscardingHelper* discarding_helper =
      PageDiscardingHelper::GetFromGraph(graph_);

  std::vector<const PageNode*> page_nodes = graph_->GetAllPageNodes();

  std::vector<PageNodeSortProxy> candidates;
  for (const auto* page_node : page_nodes) {
    PageDiscardingHelper::CanDiscardResult can_discard_result =
        discarding_helper->CanDiscard(
            page_node, PageDiscardingHelper::DiscardReason::URGENT);
    bool is_marked =
        (can_discard_result == PageDiscardingHelper::CanDiscardResult::kMarked);
    bool is_protected = (can_discard_result ==
                         PageDiscardingHelper::CanDiscardResult::kProtected);
    candidates.emplace_back(page_node, is_marked, is_protected,
                            page_node->GetTimeSinceLastVisibilityChange());
  }
  // Sorts with descending importance.
  std::sort(candidates.begin(), candidates.end(),
            [](const PageNodeSortProxy& lhs, const PageNodeSortProxy& rhs) {
              return rhs < lhs;
            });

  oom_score_map_ = DistributeOomScore(candidates);
}

OomScorePolicyChromeOS::ProcessScoreMap
OomScorePolicyChromeOS::DistributeOomScore(
    const std::vector<PageNodeSortProxy>& candidates) {
  ProcessScoreMap score_map;

  std::vector<base::ProcessId> pids = GetUniqueSortedPids(candidates);

  const int pid_count = pids.size();
  if (pid_count == 0) {
    return score_map;
  }

  // Now we distribute oom_score_adj evenly in the range based on the sorted
  // list. We're assigning priorities in the range of kLowestRendererOomScore
  // to kHighestRendererOomScore (defined in chrome_constants.h). oom_score_adj
  // takes values from -1000 to 1000. Negative values are reserved for system
  // processes. Higher values are more likely to be killed by the Linux kernel
  // OOM killer.
  const float range = static_cast<float>(content::kHighestRendererOomScore -
                                         content::kLowestRendererOomScore);
  const float adj_increment = (pid_count == 1) ? 0 : range / (pid_count - 1);

  float adj_raw = content::kLowestRendererOomScore;
  for (base::ProcessId pid : pids) {
    const int adj = round(adj_raw);

    score_map[pid] = adj;

    if (GetCachedOomScore(pid) != adj) {
      VLOG(3) << "Update OOM score " << adj << " for " << pid;
      if (!base::AdjustOOMScore(pid, adj)) {
        LOG(ERROR) << "Failed to set oom_score_adj to " << adj
                   << " for process " << pid;
      }
    }
    adj_raw += adj_increment;
  }

  return score_map;
}

std::vector<base::ProcessId> OomScorePolicyChromeOS::GetUniqueSortedPids(
    const std::vector<PageNodeSortProxy>& candidates) {
  std::vector<base::ProcessId> pids;

  std::set<base::ProcessId> pid_set;

  for (const auto& candidate : candidates) {
    const FrameNode* main_frame_node =
        candidate.page_node()->GetMainFrameNode();
    if (!main_frame_node) {
      continue;
    }
    base::ProcessId pid = main_frame_node->GetProcessNode()->GetProcessId();

    if (pid == base::kNullProcessId || pid_set.find(pid) != pid_set.end()) {
      continue;
    }

    pids.push_back(pid);
    pid_set.insert(pid);
  }

  return pids;
}

int OomScorePolicyChromeOS::GetCachedOomScore(base::ProcessHandle pid) {
  auto it = oom_score_map_.find(pid);
  if (it == oom_score_map_.end()) {
    return -1;
  }
  return it->second;
}

}  // namespace policies
}  // namespace performance_manager
