// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/transient_keep_alive_policy.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/graph/process_node.h"
#include "components/performance_manager/public/render_process_host_proxy.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace performance_manager::policies {

namespace {

// Helper function to efficiently check if a process has any main frames.
bool HasMainFrames(const ProcessNode* process_node) {
  for (const FrameNode* frame_node : process_node->GetFrameNodes()) {
    if (frame_node->IsMainFrame()) {
      return true;
    }
  }
  return false;
}

// Helper function posted by ReleaseOldestProcessKeptAlive to decrement
// the ref count asynchronously, breaking the re-entrancy chain.
void DecrementPendingReuseRefCountById(content::ChildProcessId rph_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderProcessHost* rph = content::RenderProcessHost::FromID(rph_id);
  if (rph) {
    rph->DecrementPendingReuseRefCount();
  }
}

}  // namespace

TransientKeepAlivePolicy::TransientKeepAlivePolicy() = default;
TransientKeepAlivePolicy::~TransientKeepAlivePolicy() = default;

TransientKeepAlivePolicy::KeepAliveInfo::KeepAliveInfo() = default;
TransientKeepAlivePolicy::KeepAliveInfo::~KeepAliveInfo() = default;

void TransientKeepAlivePolicy::OnPassedToGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  graph->AddFrameNodeObserver(this);
  graph->AddProcessNodeObserver(this);
}

void TransientKeepAlivePolicy::OnTakenFromGraph(Graph* graph) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The policy should not be holding any processes alive when it's torn down.
  CHECK(tracked_processes_with_frames_.empty() &&
        empty_kept_alive_processes_.empty());
  graph->RemoveFrameNodeObserver(this);
  graph->RemoveProcessNodeObserver(this);
}

void TransientKeepAlivePolicy::OnFrameNodeAdded(const FrameNode* frame_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!frame_node->IsMainFrame()) {
    return;
  }

  const ProcessNode* process_node = frame_node->GetProcessNode();
  CHECK_EQ(process_node->GetProcessType(), content::PROCESS_TYPE_RENDERER);

  //
  // CASE 1: REUSED PROCESS
  //
  // Check if a frame is being added to one of the empty processes currently
  // being kept alive by this policy.
  auto empty_it = empty_kept_alive_processes_.find(process_node);
  if (empty_it != empty_kept_alive_processes_.end()) {
    // It was empty. Now it's being reused.
    base::UmaHistogramEnumeration(kKeepAliveResultHistogramName,
                                  TransientKeepAliveResult::kReused);
    base::UmaHistogramMediumTimes(
        kKeepAliveTimeToReuseHistogramName,
        base::TimeTicks::Now() - empty_it->second->keep_alive_start_time);

    // Move it from the "empty" map to the "active" set.
    empty_kept_alive_processes_.erase(empty_it);
    // We know it's not in the active set, so this insert will always succeed.
    auto [_, inserted] = tracked_processes_with_frames_.insert(process_node);
    CHECK(inserted);
    return;
  }

  // Not a Reused process. It's either a new frame in an already-active process
  // (CASE 2) or a brand new process (CASE 3).
  auto [it, inserted] = tracked_processes_with_frames_.insert(process_node);
  if (!inserted) {
    //
    // CASE 2: ALREADY TRACKED
    //
    // The insert failed because the process was already in our
    // "active" set. This is just an additional main frame being added. No
    // action needed.
    return;
  }
  //
  // CASE 3: NEW PROCESS
  //
  // The insert succeeded. This is the first time this policy has seen
  // this renderer process host a main frame. Increment its PendingReuseRefCount
  // and track it in the "active" set.
  content::RenderProcessHost* rph =
      process_node->GetRenderProcessHostProxy().Get();
  CHECK(rph);
  rph->IncrementPendingReuseRefCount();
}

void TransientKeepAlivePolicy::OnFrameNodeRemoved(
    const FrameNode* frame_node,
    const FrameNode* previous_parent_frame_node,
    const PageNode* previous_page_node,
    const ProcessNode* previous_process_node,
    const FrameNode* previous_parent_or_outer_document_or_embedder) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Determine if the removed node *was* a main frame by checking its
  // previous parent (main frames have no parent frame node).
  // Note: frame_node->IsMainFrame() is unreliable here as the parent
  // link is already cleared.
  bool was_main_frame = !previous_parent_frame_node;

  // Only consider main frames.
  if (!was_main_frame) {
    return;
  }

  // Verify the process was a renderer, as expected for a frame.
  DCHECK_EQ(previous_process_node->GetProcessType(),
            content::PROCESS_TYPE_RENDERER);

  // We only care about processes that are in our "active" set.
  auto tracked_it = tracked_processes_with_frames_.find(previous_process_node);

  // The main frame has been removed. Check if the process is now empty of main
  // frames.
  if (HasMainFrames(previous_process_node)) {
    // Another main frame still exists. No action needed.
    return;
  }

  //
  // LAST MAIN FRAME REMOVED
  //
  // The process is now empty. Move it from the "active" set to the
  // "empty" map and start the keep-alive timer.
  tracked_processes_with_frames_.erase(tracked_it);

  // Evict an old process if needed *before* adding this one.
  EnforceKeepAliveCountLimit();

  // Add this newly empty process to the keep-alive map and start its timer.
  auto keep_alive_info = std::make_unique<KeepAliveInfo>();
  keep_alive_info->keep_alive_start_time = base::TimeTicks::Now();
  keep_alive_info->expiration_timer.Start(
      FROM_HERE, features::kTransientKeepAlivePolicyDuration.Get(),
      base::BindOnce(&TransientKeepAlivePolicy::ReleaseKeepAlive,
                     base::Unretained(this), previous_process_node));

  empty_kept_alive_processes_.emplace(previous_process_node,
                                      std::move(keep_alive_info));
}

void TransientKeepAlivePolicy::OnProcessNodeRemoved(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ignore non-renderer processes.
  if (process_node->GetProcessType() != content::PROCESS_TYPE_RENDERER) {
    return;
  }

  // Check if it's in the "active" set.
  if (tracked_processes_with_frames_.erase(process_node)) {
    content::RenderProcessHost* render_process_host =
        process_node->GetRenderProcessHostProxy().Get();
    CHECK(render_process_host);
    render_process_host->DecrementPendingReuseRefCount();
    return;
  }

  // Check if it's in the "empty" map.
  if (empty_kept_alive_processes_.erase(process_node)) {
    content::RenderProcessHost* render_process_host =
        process_node->GetRenderProcessHostProxy().Get();
    CHECK(render_process_host);
    render_process_host->DecrementPendingReuseRefCount();
  }
}

// Evict the oldest processes until we are under the limit.
void TransientKeepAlivePolicy::EnforceKeepAliveCountLimit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  const size_t limit = features::kTransientKeepAlivePolicyMaxCount.Get();
  // If we're not at the limit, there's nothing to do.
  if (empty_kept_alive_processes_.size() < limit) {
    return;
  }
  // Otherwise, we must be exactly at the limit. Evict the oldest.
  DCHECK_EQ(empty_kept_alive_processes_.size(), limit);
  ReleaseOldestProcessKeptAlive();
}

void TransientKeepAlivePolicy::ReleaseKeepAlive(
    const ProcessNode* process_node) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // This function is called when a timer expires or by eviction.
  // The process must be in the `empty_kept_alive_processes_` map.
  size_t removed = empty_kept_alive_processes_.erase(process_node);
  CHECK_EQ(removed, 1u);

  base::UmaHistogramEnumeration(kKeepAliveResultHistogramName,
                                TransientKeepAliveResult::kTimedOut);

  content::RenderProcessHost* render_process_host =
      process_node->GetRenderProcessHostProxy().Get();
  CHECK(render_process_host);
  render_process_host->DecrementPendingReuseRefCount();
}

// Finds and evicts the single oldest process being kept alive. This is called
// by `EnforceKeepAliveCountLimit` when the number of empty processes exceeds
// the limit.
void TransientKeepAlivePolicy::ReleaseOldestProcessKeptAlive() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Find the process with the oldest (smallest) `keep_alive_start_time`.
  // We must iterate, as the map is sorted by ProcessNode* address, not
  // insertion time.
  auto oldest_process_it = empty_kept_alive_processes_.end();
  base::TimeTicks oldest_start_time = base::TimeTicks::Max();

  for (auto it = empty_kept_alive_processes_.begin();
       it != empty_kept_alive_processes_.end(); ++it) {
    if (it->second->keep_alive_start_time < oldest_start_time) {
      oldest_start_time = it->second->keep_alive_start_time;
      oldest_process_it = it;
    }
  }

  if (oldest_process_it == empty_kept_alive_processes_.end()) {
    // Can happen if `EnforceKeepAliveCountLimit` was called on an empty map.
    return;
  }

  // Release the keep-alive reference directly since we have the iterator.
  // This avoids a redundant map lookup compared to calling
  // ReleaseKeepAlive().
  oldest_process_it->second->expiration_timer.Stop();

  const ProcessNode* process_node_to_release = oldest_process_it->first;
  base::UmaHistogramEnumeration(kKeepAliveResultHistogramName,
                                TransientKeepAliveResult::kEvicted);

  // Get the RPH now to capture its ID. We assume it's valid here
  // because the timer was running.
  content::RenderProcessHost* render_process_host =
      process_node_to_release->GetRenderProcessHostProxy().Get();
  CHECK(render_process_host);
  content::ChildProcessId render_process_host_id = render_process_host->GetID();

  // Erase from our map *first* to prevent OnProcessNodeRemoved from
  // also trying to decrement the refcount.
  empty_kept_alive_processes_.erase(oldest_process_it);

  // Post the actual ref count decrement to a new task to break the
  // synchronous call chain and avoid re-entrancy.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&DecrementPendingReuseRefCountById,
                                render_process_host_id));
}

}  // namespace performance_manager::policies
