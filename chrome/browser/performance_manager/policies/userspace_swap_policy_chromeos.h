// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_USERSPACE_SWAP_POLICY_CHROMEOS_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_USERSPACE_SWAP_POLICY_CHROMEOS_H_

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/resource_coordinator/lifecycle_unit_observer.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/graph/system_node.h"

namespace ash {
namespace memory {
namespace userspace_swap {
struct UserspaceSwapConfig;
}  // namespace userspace_swap
}  // namespace memory
}  // namespace ash

namespace performance_manager {
namespace policies {

// UserspaceSwapPolicy is a policy which will trigger a renderer to swap itself
// via userspace.
class UserspaceSwapPolicy : public GraphOwned,
                            public ProcessNode::ObserverDefaultImpl,
                            public SystemNode::ObserverDefaultImpl {
 public:
  UserspaceSwapPolicy();

  UserspaceSwapPolicy(const UserspaceSwapPolicy&) = delete;
  UserspaceSwapPolicy& operator=(const UserspaceSwapPolicy&) = delete;

  ~UserspaceSwapPolicy() override;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // ProcessNodeObserver implementation:
  void OnAllFramesInProcessFrozen(const ProcessNode* process_node) override;
  void OnProcessNodeAdded(const ProcessNode* process_node) override;
  void OnProcessLifetimeChange(const ProcessNode* process_node) override;

  // SystemNodeObserver:
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel new_level) override;

  // Returns true if running on a platform that supports the kernel features
  // necessary for userspace swapping, most important would be userfaultfd(2).
  static bool UserspaceSwapSupportedAndEnabled();

 protected:
  explicit UserspaceSwapPolicy(
      const ash::memory::userspace_swap::UserspaceSwapConfig& config);

  // The following methods are virtual for testing.
  virtual void SwapNodesOnGraph();
  virtual bool InitializeProcessNode(const ProcessNode* process_node);
  virtual uint64_t GetTotalSwapFileUsageBytes();
  virtual uint64_t GetSwapDeviceFreeSpaceBytes();
  virtual void SwapProcessNode(const ProcessNode* process_node);
  virtual uint64_t GetProcessNodeSwapFileUsageBytes(
      const ProcessNode* process_node);
  virtual uint64_t GetProcessNodeReclaimedBytes(
      const ProcessNode* process_node);
  virtual bool IsPageNodeVisible(const PageNode* page_node);
  virtual bool IsPageNodeAudible(const PageNode* page_node);
  virtual bool IsPageNodeLoadingOrBusy(const PageNode* page_node);
  virtual base::TimeDelta GetTimeSinceLastVisibilityChange(
      const PageNode* page_node);

  // IsEligibleToSwap will return true if the |page_node| belonging to the
  // |process_node| meets the configured criteria to be swap eligible. The
  // |page_node| is optional, if it's provided the visibility, audible, and
  // loading states will also be used to determine eligibility. The situation
  // where the |page_node| is omitted is when the process is frozen.
  virtual bool IsEligibleToSwap(const ProcessNode* process_node,
                                const PageNode* page_node);

  // Returns the time in which this process was last swapped, |process_node| is
  // the node in which you want to update.
  virtual base::TimeTicks GetLastSwapTime(const ProcessNode* process_node);

  // We cache the config object since it cannot change, this makes the code
  // easier to read and testing also becomes easier.
  const raw_ref<const ash::memory::userspace_swap::UserspaceSwapConfig> config_;

  // Keeps track of the last time we walked the graph looking for processes to
  // swap, the frequency we walk the graph is configurable.
  base::TimeTicks last_graph_walk_;

  // We periodically check the amount of free space on the device that backs our
  // swap files to make sure we're not letting it get below the configured
  // limit, we don't want to completely exhaust free space on a device.
  base::TimeTicks last_device_space_check_;
  uint64_t backing_store_available_bytes_ = 0;

 private:
  // A helper method which sets the last trim time to the specified time.
  void SetLastSwapTime(const ProcessNode* process_node, base::TimeTicks time);

  void PrintAllSwapMetrics();

  std::unique_ptr<base::RepeatingTimer> metrics_timer_ =
      std::make_unique<base::RepeatingTimer>();

  base::WeakPtrFactory<UserspaceSwapPolicy> weak_factory_{this};
};

}  // namespace policies
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_USERSPACE_SWAP_POLICY_CHROMEOS_H_
