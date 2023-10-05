// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/oom_score_policy_chromeos.h"

#include <algorithm>
#include <set>

#include "base/process/memory.h"  // For AdjustOOMScore
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "content/public/browser/browser_thread.h"  // For GetUIThreadTaskRunner
#include "content/public/common/content_constants.h"  // For kLowestRendererOomScore

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/resource_manager.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace performance_manager {
namespace policies {

namespace {

// TODO(crbug/1489325): oom_score_adj of some processes could be out-of-dated.
// Override OnIsFocusedChanged to update the oom_score_adj of the focused tab
// to make it harder to be killed by the Linux oom killer.
constexpr base::TimeDelta kOomScoresAssignmentMinimalInterval =
    base::Seconds(10);

// The minimal interval of background pids report. The throttling is to reduce
// the overhead of the pids reporting. When there is no memory pressure, the
// reported pid list is not used. Under memory pressure, the background pids
// are used to calculate the background Chrome memory usage. When the memory
// pressure is higher, it requires larger amount of background memory usage to
// change the low memory handling policy (whether to avoid killing perceptible
// apps) [1]. So small deviation of the background memory usage caused by
// out-of-dated pid list would only make the policy change a little bit earlier
// or later.
//
// [1]:
// https://chromium-review.googlesource.com/c/chromiumos/platform2/+/4889461/9/resourced/src/memory.rs#665
constexpr base::TimeDelta kBackgroundPidsReportMinimalInterval =
    base::Seconds(3);

void ReportBackgroundProcessesOnUIThread(
    const std::vector<base::ProcessId>& pids) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ResourcedClient* client = ash::ResourcedClient::Get();
  if (client) {
    client->ReportBackgroundProcesses(ash::ResourcedClient::Component::kAsh,
                                      pids);
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  chromeos::LacrosService* service = chromeos::LacrosService::Get();
  // Check LacrosService availability to avoid crashing
  // lacros_chrome_browsertests.
  if (!service || !service->IsAvailable<crosapi::mojom::ResourceManager>()) {
    LOG(ERROR) << "ResourceManager is not available";
    return;
  }

  int resource_manager_version =
      service->GetInterfaceVersion<crosapi::mojom::ResourceManager>();
  if (resource_manager_version <
      int{crosapi::mojom::ResourceManager::MethodMinVersions::
              kReportBackgroundProcessesMinVersion}) {
    LOG(WARNING) << "Resource Manager version " << resource_manager_version
                 << " does not support reporting background processes.";
    return;
  }

  service->GetRemote<crosapi::mojom::ResourceManager>()
      ->ReportBackgroundProcesses(pids);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

}  // namespace

OomScorePolicyChromeOS::OomScorePolicyChromeOS() = default;
OomScorePolicyChromeOS::~OomScorePolicyChromeOS() = default;

void OomScorePolicyChromeOS::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph_ = graph;
  graph->AddPageNodeObserver(this);
}

void OomScorePolicyChromeOS::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RemovePageNodeObserver(this);
  graph_ = nullptr;
}

void OomScorePolicyChromeOS::OnPageNodeAdded(const PageNode* page_node) {
  HandlePageNodeEventsThrottled();
}

void OomScorePolicyChromeOS::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  HandlePageNodeEventsThrottled();
}

void OomScorePolicyChromeOS::OnIsVisibleChanged(const PageNode* page_node) {
  HandlePageNodeEventsThrottled();
}

void OomScorePolicyChromeOS::OnTypeChanged(const PageNode* page_node,
                                           PageType previous_type) {
  HandlePageNodeEventsThrottled();
}

void OomScorePolicyChromeOS::HandlePageNodeEventsThrottled() {
  base::TimeTicks now = base::TimeTicks::Now();
  bool oom_scores_assignment = false;
  bool background_pids_report = false;

  if (now - last_oom_scores_assignment_ > kOomScoresAssignmentMinimalInterval) {
    last_oom_scores_assignment_ = now;
    oom_scores_assignment = true;
  }
  if (now - last_background_pids_report_ >
      kBackgroundPidsReportMinimalInterval) {
    last_background_pids_report_ = now;
    background_pids_report = true;
  }

  HandlePageNodeEvents(oom_scores_assignment, background_pids_report);
}

void OomScorePolicyChromeOS::HandlePageNodeEvents(bool oom_scores_assignment,
                                                  bool background_pids_report) {
  if (!oom_scores_assignment && !background_pids_report) {
    return;
  }

  PageDiscardingHelper* discarding_helper =
      PageDiscardingHelper::GetFromGraph(graph_);

  std::vector<const PageNode*> page_nodes = graph_->GetAllPageNodes();

  std::vector<PageNodeSortProxy> candidates;
  std::vector<PageNodeSortProxy> background_candidates;

  for (const auto* page_node : page_nodes) {
    PageDiscardingHelper::CanDiscardResult can_discard_result =
        discarding_helper->CanDiscard(
            page_node, PageDiscardingHelper::DiscardReason::URGENT);
    bool is_marked =
        (can_discard_result == PageDiscardingHelper::CanDiscardResult::kMarked);
    bool is_protected = (can_discard_result ==
                         PageDiscardingHelper::CanDiscardResult::kProtected);
    bool is_visible = page_node->IsVisible();
    if (oom_scores_assignment) {
      candidates.emplace_back(page_node, is_marked, is_visible, is_protected,
                              page_node->GetTimeSinceLastVisibilityChange());
    }
    if (background_pids_report && !is_marked && !is_protected) {
      background_candidates.emplace_back(
          page_node, is_marked, is_visible, is_protected,
          page_node->GetTimeSinceLastVisibilityChange());
    }
  }

  if (oom_scores_assignment) {
    // Sorts with descending importance.
    std::sort(candidates.begin(), candidates.end(),
              [](const PageNodeSortProxy& lhs, const PageNodeSortProxy& rhs) {
                return rhs < lhs;
              });

    oom_score_map_ = DistributeOomScore(candidates);
  }

  if (background_pids_report) {
    std::vector<base::ProcessId> background_pids =
        GetUniquePids(background_candidates);
    ReportBackgroundProcesses(std::move(background_pids));
  }
}

OomScorePolicyChromeOS::ProcessScoreMap
OomScorePolicyChromeOS::DistributeOomScore(
    const std::vector<PageNodeSortProxy>& candidates) {
  ProcessScoreMap score_map;

  std::vector<base::ProcessId> pids = GetUniquePids(candidates);

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

std::vector<base::ProcessId> OomScorePolicyChromeOS::GetUniquePids(
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

void OomScorePolicyChromeOS::ReportBackgroundProcesses(
    std::vector<base::ProcessId> pids) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ReportBackgroundProcessesOnUIThread, std::move(pids)));
}

}  // namespace policies
}  // namespace performance_manager
