// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_REPORT_PAGE_PROCESSES_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_REPORT_PAGE_PROCESSES_POLICY_H_

#include <vector>

#include "base/process/process_handle.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"

namespace performance_manager::policies {

// This class is responsible for sending the page process list to the resource
// manager daemon (resourced). The process list is used to estimate the memory
// usage of the browser. Based on the browser memory usage, resourced determines
// when to release memory from browser or VMs or container. When there are a lot
// of background processes in browser, resourced would avoid killing perceptible
// apps in VMs or container.
class ReportPageProcessesPolicy : public GraphOwned,
                                  public PageNode::ObserverDefaultImpl {
 public:
  ReportPageProcessesPolicy();
  ~ReportPageProcessesPolicy() override;
  ReportPageProcessesPolicy(const ReportPageProcessesPolicy& other) = delete;
  ReportPageProcessesPolicy& operator=(const ReportPageProcessesPolicy&) =
      delete;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // PageNode::ObserverDefaultImpl:
  void OnPageNodeAdded(const PageNode* page_node) override;
  void OnBeforePageNodeRemoved(const PageNode* page_node) override;
  void OnIsVisibleChanged(const PageNode* page_node) override;
  void OnTypeChanged(const PageNode* page_node,
                     PageType previous_type) override;

 protected:
  // These members are protected for testing.
  void HandlePageNodeEvents();

  // Reports the background process list to the resource manager daemon
  // (resourced). Based on the background process list, resourced determines
  // when to release memory from Chrome or VMs or containers.
  // It's virtual for testing.
  virtual void ReportBackgroundProcesses(std::vector<base::ProcessId> pids);

 private:
  // ReportPageProcessesPolicy is active when receiving page node events.
  void HandlePageNodeEventsThrottled();

  // Returns a vector of pids of the main frame renderer process of the
  // |candidates| (the child frame renderer processes are ignored). The order of
  // the pids is corresponding to the order of the |candidates|.
  std::vector<base::ProcessId> GetUniquePids(
      const std::vector<PageNodeSortProxy>& candidates);

  base::TimeTicks last_report_ = base::TimeTicks::Now();

  raw_ptr<Graph> graph_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_REPORT_PAGE_PROCESSES_POLICY_H_
