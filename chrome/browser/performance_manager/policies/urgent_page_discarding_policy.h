// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_URGENT_PAGE_DISCARDING_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_URGENT_PAGE_DISCARDING_POLICY_H_

#include <optional>

#include "base/memory/memory_pressure_listener.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/performance_manager/policies/sustained_memory_pressure_evaluator.h"
#include "components/memory_pressure/reclaim_target.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/system_node.h"

namespace performance_manager {

namespace policies {

// Urgently discard a tab when receiving a memory pressure signal.
class UrgentPageDiscardingPolicy : public GraphOwned,
                                   public base::MemoryPressureListener {
 public:
  UrgentPageDiscardingPolicy();
  ~UrgentPageDiscardingPolicy() override;
  UrgentPageDiscardingPolicy(const UrgentPageDiscardingPolicy& other) = delete;
  UrgentPageDiscardingPolicy& operator=(const UrgentPageDiscardingPolicy&) =
      delete;

  // GraphOwned implementation:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  // When invoked, the policy will not discard pages on memory pressure.
  static void DisableForTesting();

 private:
  // base::MemoryPressureListener:
  void OnMemoryPressure(base::MemoryPressureLevel new_level) override;

  // Callback for `sustained_memory_pressure_evaluator_`.
  void OnSustainedMemoryPressure(bool is_sustained_memory_pressure);

  // Discards a tab while in a memory pressure statte.
  void HandleMemoryPressureEvent();

  // Callback called when a discard attempt has completed.
  void PostDiscardAttemptCallback(bool success);

#if BUILDFLAG(IS_CHROMEOS)
  // Called when the reclaim target is ready.
  void OnReclaimTarget(
      base::TimeTicks on_memory_pressure_at,
      std::optional<memory_pressure::ReclaimTarget> reclaim_target_kb);
#endif  // BUILDFLAG(IS_CHROMEOS)

  std::optional<base::MemoryPressureListenerRegistration>
      memory_pressure_listener_registration_;

  // True while we are in the process of discarding tab(s) in response to a
  // memory pressure notification. It becomes false once we're done responding
  // to this notification.
  bool handling_memory_pressure_notification_ = false;

  // Determines if the system is in a sustained memory pressure state.
  std::optional<SustainedMemoryPressureEvaluator>
      sustained_memory_pressure_evaluator_;

  // While in a sustained memory pressure state, continue discarding a tab every
  // time the timer fires.
  base::RepeatingTimer sustained_memory_pressure_timer_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace policies
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_URGENT_PAGE_DISCARDING_POLICY_H_
