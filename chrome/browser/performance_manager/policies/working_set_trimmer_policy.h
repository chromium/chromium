// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_WORKING_SET_TRIMMER_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_WORKING_SET_TRIMMER_POLICY_H_

#include "base/time/time.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/node_data_describer.h"

namespace performance_manager {

class ProcessNode;

namespace policies {

// Base class for platform-specific working set trimmer policies. Tracks the
// last trim time of each ProcessNode and provides a helper to trim a process's
// working set.
class WorkingSetTrimmerPolicy : public GraphOwned,
                                public NodeDataDescriberDefaultImpl {
 public:
  WorkingSetTrimmerPolicy();

  WorkingSetTrimmerPolicy(const WorkingSetTrimmerPolicy&) = delete;
  WorkingSetTrimmerPolicy& operator=(const WorkingSetTrimmerPolicy&) = delete;

  ~WorkingSetTrimmerPolicy() override;

  // CreatePolicyForPlatform will create a working set trimmer policy for a
  // specific platform which should be owned by the graph, you should always
  // check PlatformSupportsWorkingSetTrim() before creating a policy to do so as
  // it would result in unnecessary book-keeping.
  static std::unique_ptr<WorkingSetTrimmerPolicy> CreatePolicyForPlatform();

  // Returns true if running on a platform that supports working set trimming.
  static bool PlatformSupportsWorkingSetTrim();

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

 protected:
  // Returns the time in which this process was last trimmed.
  base::TimeTicks GetLastTrimTime(const ProcessNode* process_node);

  // Sets the last trim time to TimeTicks::Now().
  void SetLastTrimTimeNow(const ProcessNode* process_node);

  virtual void TrimWorkingSet(const ProcessNode* process_node);

 private:
  friend class WorkingSetTrimmerPolicyTest;

  // A helper method which sets the last trim time to the specified time.
  void SetLastTrimTime(const ProcessNode* process_node, base::TimeTicks time);

  // NodeDataDescriber implementation:
  base::DictValue DescribeProcessNodeData(
      const ProcessNode* node) const override;
};

}  // namespace policies
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_WORKING_SET_TRIMMER_POLICY_H_
