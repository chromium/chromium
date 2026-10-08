// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_MEMORY_SAVER_MEMORY_COORDINATOR_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_MEMORY_SAVER_MEMORY_COORDINATOR_POLICY_H_

#include "base/memory_coordinator/memory_consumer.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "content/public/common/memory_coordinator/memory_coordinator_policy.h"
#include "content/public/common/memory_coordinator/predicate_memory_coordinator_policy.h"

namespace content {
class MemoryCoordinatorPolicyManager;
}  // namespace content

namespace performance_manager::policies {

// A MemoryCoordinatorPolicy that scales down the memory limits of all stateful,
// limit-supporting MemoryConsumers across all processes when Memory Saver Mode
// is active.
//
// Multipliers applied by MemorySaverModeAggressiveness level:
// - kAggressive:   25% (base::MemoryLimit::FromPercent(25))
// - kMedium:       50% (base::MemoryLimit::FromPercent(50))
// - kConservative: 75% (base::MemoryLimit::FromPercent(75))
// - Disabled:     100% (base::MemoryLimit::Default(), clearing the override)
class MemorySaverMemoryCoordinatorPolicy
    : public content::PredicateMemoryCoordinatorPolicy {
 public:
  explicit MemorySaverMemoryCoordinatorPolicy(
      content::MemoryCoordinatorPolicyManager& manager);
  ~MemorySaverMemoryCoordinatorPolicy() override;

  MemorySaverMemoryCoordinatorPolicy(
      const MemorySaverMemoryCoordinatorPolicy&) = delete;
  MemorySaverMemoryCoordinatorPolicy& operator=(
      const MemorySaverMemoryCoordinatorPolicy&) = delete;

  // Updates the active memory limit based on whether Memory Saver Mode is
  // enabled and its configured aggressiveness level.
  void UpdateState(bool enabled,
                   user_tuning::prefs::MemorySaverModeAggressiveness mode);

 private:
  base::MemoryLimit current_limit_ = base::MemoryLimit::Default();
  content::MemoryCoordinatorPolicyRegistration registration_;
};

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_MEMORY_SAVER_MEMORY_COORDINATOR_POLICY_H_
