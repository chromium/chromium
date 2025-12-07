// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_TRANSIENT_KEEP_ALIVE_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_TRANSIENT_KEEP_ALIVE_POLICY_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph_registered.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "content/public/browser/child_process_id.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace performance_manager::policies {

inline constexpr char kKeepAliveResultHistogramName[] =
    "PerformanceManager.TransientKeepAlive.Result";
inline constexpr char kKeepAliveTimeToReuseHistogramName[] =
    "PerformanceManager.TransientKeepAlive.TimeToReuse";

// A policy that keeps empty renderer processes alive temporarily to improve
// navigation performance through process reuse.
//
// Process Lifecycle:
// 1. First main frame added -> Process is tracked and the RenderProcessHost's
//    PendingReuseRefCount is incremented. (Process has frames).
// 2. Last main frame removed -> Keep-alive timer starts. (Process is empty).
// 3a. New frame added before timeout -> Timer canceled. Returns to state 1.
//     (Success case: kReused).
// 3b. Timeout expires -> Keep-alive reference released (RenderProcessHost's
//     PendingReuseRefCount is decremented), process untracked.
//     (Failure case: kTimedOut).
// 3c. Limit exceeded -> Oldest empty process is evicted. Keep-alive reference
//     released (RenderProcessHost's PendingReuseRefCount is decremented),
//     process untracked. (Failure case: kEvicted).
//
// The policy uses the RenderProcessHost's PendingReuseRefCount to keep
// processes alive and enforces a configurable limit on simultaneous
// kept-alive processes.
class TransientKeepAlivePolicy
    : public FrameNodeObserver,
      public ProcessNodeObserver,
      public GraphOwnedAndRegistered<TransientKeepAlivePolicy> {
 public:
  // Entries should not be renumbered and new entries should only be added to
  // the end.
  // LINT.IfChange(TransientKeepAliveResult)
  enum class TransientKeepAliveResult {
    kReused = 0,
    kEvicted = 1,
    kTimedOut = 2,
    kMaxValue = kTimedOut,
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/performance_manager/enums.xml:TransientKeepAliveResult)

  TransientKeepAlivePolicy();
  TransientKeepAlivePolicy(const TransientKeepAlivePolicy&) = delete;
  TransientKeepAlivePolicy& operator=(const TransientKeepAlivePolicy&) = delete;
  ~TransientKeepAlivePolicy() override;

  // FrameNodeObserver:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;
  void OnFrameNodeRemoved(
      const FrameNode* frame_node,
      const FrameNode* previous_parent_frame_node,
      const PageNode* previous_page_node,
      const ProcessNode* previous_process_node,
      const FrameNode* previous_parent_or_outer_document_or_embedder) override;

  // ProcessNodeObserver:
  void OnProcessNodeRemoved(const ProcessNode* process_node) override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

 private:
  struct KeepAliveInfo {
    KeepAliveInfo();
    KeepAliveInfo(const KeepAliveInfo&) = delete;
    KeepAliveInfo& operator=(const KeepAliveInfo&) = delete;
    ~KeepAliveInfo();

    base::TimeTicks keep_alive_start_time;
    base::OneShotTimer expiration_timer;
  };

  // Ensures we don't exceed the `kTransientKeepAlivePolicyMaxCount` limit by
  // evicting if needed.
  void EnforceKeepAliveCountLimit();

  // Releases the keep-alive on the given process and untracks it.
  // `was_timed_out` is true if this is called by the timer, false if
  // called by eviction.
  void ReleaseKeepAlive(const ProcessNode* process_node);

  // Finds and evicts the process that has been in the "kept-alive"
  // state the longest.
  void ReleaseOldestProcessKeptAlive();

  // Tracks processes that currently host one or more main frames.
  // Processes in this set are moved to `empty_kept_alive_processes_` when their
  // last main frame is removed.
  // Note: Uses absl::flat_hash_set for fast lookups, insertions, and deletions,
  // as this set is frequently mutated and not guaranteed to be small.
  absl::flat_hash_set<const ProcessNode*> tracked_processes_with_frames_;

  // Tracks empty renderer processes being kept alive temporarily.
  // Each process in this map has 0 main frames and a running expiration timer
  // (in `KeepAliveInfo`). The size of this map is limited by
  // `kTransientKeepAlivePolicyMaxCount`.
  // Note: Using std::unique_ptr for the value since timers are not movable.
  base::flat_map<const ProcessNode*, std::unique_ptr<KeepAliveInfo>>
      empty_kept_alive_processes_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_TRANSIENT_KEEP_ALIVE_POLICY_H_
