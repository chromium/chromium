// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_URGENT_PAGE_DISCARDING_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_URGENT_PAGE_DISCARDING_POLICY_H_

#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/sequence_checker.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager {

namespace policies {

// Urgently discard a tab when receiving a memory pressure signal. The discard
// strategy used by this policy is based on a feature flag, see
// UrgentDiscardingParams for more details.
class UrgentPageDiscardingPolicy : public GraphOwned {
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
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  // Register to start listening to memory pressure. Called on startup or after
  // handling a pressure event.
  void RegisterMemoryPressureListener();

  // Unregister to stop listening to memory pressure. Called on shutdown or
  // when handling a pressure event.
  void UnregisterMemoryPressureListener();

  // Callback called when a discard attempt has completed.
  void PostDiscardAttemptCallback(bool success);

  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;
  Graph* graph_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace policies
}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_URGENT_PAGE_DISCARDING_POLICY_H_
