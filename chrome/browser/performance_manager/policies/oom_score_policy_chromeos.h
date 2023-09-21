// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_OOM_SCORE_POLICY_CHROMEOS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_OOM_SCORE_POLICY_CHROMEOS_H_

#include <vector>

#include "base/process/process_handle.h"  // For ProcessId
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/system_node.h"

namespace performance_manager {

namespace policies {

// Assigning oom score adj to renderer processes. Process with lowest oom score
// adj is the last to be killed by Linux oom killer. The more important process
// would be assigned lower oom score adj. See the following web page for more
// explanation on Linux oom score adj(adjust).
// [1]: https://man7.org/linux/man-pages/man1/choom.1.html
class OomScorePolicyChromeOS : public GraphOwned {
 public:
  OomScorePolicyChromeOS();
  ~OomScorePolicyChromeOS() override;
  OomScorePolicyChromeOS(const OomScorePolicyChromeOS& other) = delete;
  OomScorePolicyChromeOS& operator=(const OomScorePolicyChromeOS&) = delete;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

 protected:
  // These members are protected for testing.
  void AssignOomScores();

  // Returns the cached oom score adj. If the pid is not cached, returns -1 (a
  // value not in the valid oom score adj range for renderer processes).
  int GetCachedOomScore(base::ProcessId pid);

  raw_ptr<Graph> graph_ = nullptr;

 private:
  // Cache OOM scores in memory.
  using ProcessScoreMap = base::flat_map<base::ProcessId, int>;

  ProcessScoreMap DistributeOomScore(
      const std::vector<PageNodeSortProxy>& candidates);

  // Returns a vector of pids from most important process to least important
  // process.
  std::vector<base::ProcessId> GetUniqueSortedPids(
      const std::vector<PageNodeSortProxy>& candidates);

  base::RepeatingTimer timer_;

  // Map maintaining the process handle - oom_score mapping.
  ProcessScoreMap oom_score_map_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<OomScorePolicyChromeOS> weak_factory_{this};
};

}  // namespace policies
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_OOM_SCORE_POLICY_CHROMEOS_H_
