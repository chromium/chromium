// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/report_page_processes_policy.h"

#include <set>

#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/resource_manager.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace performance_manager::policies {

namespace {

// The minimal interval of process list report. The throttling is to reduce
// the overhead of the pids reporting. When there is no memory pressure, the
// reported pid list is not used. Under memory pressure, the process list are
// used to calculate the browser memory usage. When the memory pressure is
// higher, it requires larger amount of page memory usage to change the low
// memory handling policy (whether to avoid killing perceptible apps) [1]. So
// small deviation of the memory usage caused by out-of-dated process list
// would only make the policy change a little bit earlier or later.
//
// [1]:
// https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform2/resourced/src/memory.rs;drc=a76ccbdab134a54a6b3314a6b78722b9b3fab6d1;l=506
constexpr base::TimeDelta kReportProcessesMinimalInterval = base::Seconds(3);

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

ReportPageProcessesPolicy::ReportPageProcessesPolicy() = default;
ReportPageProcessesPolicy::~ReportPageProcessesPolicy() = default;

void ReportPageProcessesPolicy::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph_ = graph;
  graph->AddPageNodeObserver(this);
}

void ReportPageProcessesPolicy::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->RemovePageNodeObserver(this);
  graph_ = nullptr;
}

void ReportPageProcessesPolicy::OnPageNodeAdded(const PageNode* page_node) {
  HandlePageNodeEventsThrottled();
}

void ReportPageProcessesPolicy::OnBeforePageNodeRemoved(
    const PageNode* page_node) {
  HandlePageNodeEventsThrottled();
}

void ReportPageProcessesPolicy::OnIsVisibleChanged(const PageNode* page_node) {
  HandlePageNodeEventsThrottled();
}

void ReportPageProcessesPolicy::OnTypeChanged(const PageNode* page_node,
                                              PageType previous_type) {
  HandlePageNodeEventsThrottled();
}

void ReportPageProcessesPolicy::HandlePageNodeEventsThrottled() {
  base::TimeTicks now = base::TimeTicks::Now();

  if (now - last_report_ > kReportProcessesMinimalInterval) {
    last_report_ = now;
    HandlePageNodeEvents();
  }
}

void ReportPageProcessesPolicy::HandlePageNodeEvents() {
  PageDiscardingHelper* discarding_helper =
      PageDiscardingHelper::GetFromGraph(graph_);

  std::vector<const PageNode*> page_nodes = graph_->GetAllPageNodes();

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
    if (!is_marked && !is_protected) {
      background_candidates.emplace_back(
          page_node, is_marked, is_visible, is_protected,
          page_node->GetTimeSinceLastVisibilityChange());
    }
  }

  std::vector<base::ProcessId> background_pids =
      GetUniquePids(background_candidates);
  ReportBackgroundProcesses(std::move(background_pids));
}

std::vector<base::ProcessId> ReportPageProcessesPolicy::GetUniquePids(
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

void ReportPageProcessesPolicy::ReportBackgroundProcesses(
    std::vector<base::ProcessId> pids) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ReportBackgroundProcessesOnUIThread, std::move(pids)));
}

}  // namespace performance_manager::policies
