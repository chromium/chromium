// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/report_page_processes_policy.h"

#include <algorithm>
#include <set>

#include "base/functional/bind.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph_operations.h"
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

void ReportPageProcessesOnUIThread(
    const std::vector<ReportPageProcessesPolicy::PageProcess>& page_processes) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::ResourcedClient* client = ash::ResourcedClient::Get();
  if (!client) {
    return;
  }

  std::vector<ash::ResourcedClient::Process> processes;
  for (const auto& page_process : page_processes) {
    processes.emplace_back(page_process.pid, page_process.host_protected_page,
                           page_process.host_visible_page,
                           page_process.host_focused_page);
  }

  client->ReportBrowserProcesses(ash::ResourcedClient::Component::kAsh,
                                 processes);
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
              kReportPageProcessesMinVersion}) {
    LOG(WARNING) << "Resource Manager version " << resource_manager_version
                 << " does not support reporting page processes.";
    return;
  }

  std::vector<crosapi::mojom::PageProcessPtr> processes;

  for (const auto& page_process : page_processes) {
    crosapi::mojom::PageProcessPtr process = crosapi::mojom::PageProcess::New();
    process->pid = page_process.pid;
    process->host_protected_page = page_process.host_protected_page;
    process->host_visible_page = page_process.host_visible_page;
    process->host_focused_page = page_process.host_focused_page;
    processes.push_back(std::move(process));
  }

  service->GetRemote<crosapi::mojom::ResourceManager>()->ReportPageProcesses(
      std::move(processes));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
}

}  // namespace

ReportPageProcessesPolicy::ReportPageProcessesPolicy()
    : delayed_report_timer_(
          FROM_HERE,
          kReportProcessesMinimalInterval,
          base::BindRepeating(
              &ReportPageProcessesPolicy::HandlePageNodeEventsDelayed,
              base::Unretained(this))) {}

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
  if (delayed_report_timer_.IsRunning()) {
    // This event happened too soon after the last report. The updated process
    // list will be sent after the minimal interval period.
    has_delayed_events_ = true;
    return;
  }

  HandlePageNodeEvents();

  // Start the throttling timer. Any event that happens while it is running will
  // not cause a report until the minimum interval has passed.
  delayed_report_timer_.Reset();
}

void ReportPageProcessesPolicy::HandlePageNodeEventsDelayed() {
  if (has_delayed_events_) {
    HandlePageNodeEvents();
  }
}

void ReportPageProcessesPolicy::HandlePageNodeEvents() {
  has_delayed_events_ = false;

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
    bool is_visible = page_node->IsVisible();
    bool is_focused = page_node->IsFocused();
    candidates.emplace_back(page_node, is_marked, is_visible, is_protected,
                            is_focused,
                            page_node->GetTimeSinceLastVisibilityChange());
  }

  // Sorts with descending importance.
  std::sort(candidates.begin(), candidates.end(),
            [](const PageNodeSortProxy& lhs, const PageNodeSortProxy& rhs) {
              return rhs < lhs;
            });

  ListPageProcesses(candidates);
}

void ReportPageProcessesPolicy::ListPageProcesses(
    const std::vector<PageNodeSortProxy>& candidates) {
  std::set<base::ProcessId> pid_set;
  std::vector<PageProcess> page_processes;

  for (auto candidate : candidates) {
    base::flat_set<const ProcessNode*> processes =
        GraphOperations::GetAssociatedProcessNodes(candidate.page_node());
    for (auto* process : processes) {
      base::ProcessId pid = process->GetProcessId();
      if (pid == base::kNullProcessId) {
        continue;
      }
      if (!pid_set.insert(pid).second) {
        // If the process is already in a more important page node, skip. For
        // example, if both a non-protected page node and a protected page node
        // contain a process ID, the page process with this process ID is marked
        // as protected.
        continue;
      }
      page_processes.emplace_back(pid, candidate.is_protected(),
                                  candidate.is_visible(),
                                  candidate.is_focused());
    }
  }

  ReportPageProcesses(std::move(page_processes));
}

void ReportPageProcessesPolicy::ReportPageProcesses(
    std::vector<PageProcess> processes) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ReportPageProcessesOnUIThread, std::move(processes)));
}

}  // namespace performance_manager::policies
