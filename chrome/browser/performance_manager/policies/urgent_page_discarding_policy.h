// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_URGENT_PAGE_DISCARDING_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_URGENT_PAGE_DISCARDING_POLICY_H_

#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/system_node.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace performance_manager {

namespace policies {

// Urgently discard a tab when receiving a memory pressure signal.
class UrgentPageDiscardingPolicy : public GraphOwned,
                                   public SystemNode::ObserverDefaultImpl {
 public:
  UrgentPageDiscardingPolicy();
  ~UrgentPageDiscardingPolicy() override;
  UrgentPageDiscardingPolicy(const UrgentPageDiscardingPolicy& other) = delete;
  UrgentPageDiscardingPolicy& operator=(const UrgentPageDiscardingPolicy&) =
      delete;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

 private:
  // SystemNodeObserver:
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel new_level) override;

  // Callback called when a discard attempt has completed.
  void PostDiscardAttemptCallback(bool success);

#if BUILDFLAG(IS_CHROMEOS)
  // Called when the reclaim target is ready.
  void OnReclaimTarget(absl::optional<uint64_t> reclaim_target_kb);
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // True while we are in the process of discarding tab(s) in response to a
  // memory pressure notification. It becomes false once we're done responding
  // to this notification.
  bool handling_memory_pressure_notification_ = false;

  raw_ptr<Graph> graph_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace policies
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_URGENT_PAGE_DISCARDING_POLICY_H_
