// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_OOM_SCORE_POLICY_CHROMEOS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_OOM_SCORE_POLICY_CHROMEOS_H_

#include <vector>

#include "base/process/process_handle.h"  // For ProcessId
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/system_node.h"

namespace performance_manager {

namespace policies {

// Assigning oom score adj to renderer processes. Process with lowest oom score
// adj is the last to be killed by Linux oom killer. The more important process
// would be assigned lower oom score adj. See the following web page for more
// explanation on Linux oom score adj(adjust).
// [1]: https://man7.org/linux/man-pages/man1/choom.1.html
//
// This class is also responsible to send the background process list to the
// resource manager daemon (resourced). The background process list is used to
// estimate the memory usage of the background processes. Based on the
// background Chrome memory usage, resourced determines when to release memory
// from Chrome or VMs or container. When there are a lot of background processes
// in Ash Chrome, resourced would avoid killing perceptible apps in VMs or
// container.
class OomScorePolicyChromeOS : public GraphOwned,
                               public PageNode::ObserverDefaultImpl {
 public:
  OomScorePolicyChromeOS();
  ~OomScorePolicyChromeOS() override;
  OomScorePolicyChromeOS(const OomScorePolicyChromeOS& other) = delete;
  OomScorePolicyChromeOS& operator=(const OomScorePolicyChromeOS&) = delete;

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
  void HandlePageNodeEvents(bool oom_scores_assignment,
                            bool background_pids_report);

  // Returns the cached oom score adj. If the pid is not cached, returns -1 (a
  // value not in the valid oom score adj range for renderer processes).
  int GetCachedOomScore(base::ProcessId pid);

  // Reports the background process list to the resource manager daemon
  // (resourced). Based on the background process list, resourced determines
  // when to release memory from Chrome or VMs or containers.
  // It's virtual for testing.
  virtual void ReportBackgroundProcesses(std::vector<base::ProcessId> pids);

  raw_ptr<Graph> graph_ = nullptr;

 private:
  // Cache OOM scores in memory.
  using ProcessScoreMap = base::flat_map<base::ProcessId, int>;

  // OomScorePolicyChromeOS is active when receiving page node events.
  void HandlePageNodeEventsThrottled();

  ProcessScoreMap DistributeOomScore(
      const std::vector<PageNodeSortProxy>& candidates);

  // Returns a vector of pids of the main frame renderer process of the
  // |candidates| (the child frame renderer processes are ignored). The order of
  // the pids is corresponding to the order of the |candidates|.
  std::vector<base::ProcessId> GetUniquePids(
      const std::vector<PageNodeSortProxy>& candidates);

  base::TimeTicks last_oom_scores_assignment_ = base::TimeTicks::Now();
  base::TimeTicks last_background_pids_report_ = base::TimeTicks::Now();

  // Map maintaining the process handle - oom_score mapping.
  ProcessScoreMap oom_score_map_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OomScorePolicyChromeOS> weak_factory_{this};
};

}  // namespace policies
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_OOM_SCORE_POLICY_CHROMEOS_H_
