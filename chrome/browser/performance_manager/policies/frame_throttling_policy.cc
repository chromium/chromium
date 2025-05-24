// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/frame_throttling_policy.h"

#include <algorithm>

#include "base/containers/contains.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"

namespace performance_manager::policies {

FrameThrottlingPolicy::FrameThrottlingPolicy() {
  throttle_interval_ = viz::BeginFrameArgs::DefaultInterval() * 2;
}

FrameThrottlingPolicy::~FrameThrottlingPolicy() = default;

void FrameThrottlingPolicy::OnFrameNodeAdded(const FrameNode* frame_node) {
  // A frame is always important upon being added. The current implementation of
  // this class is based on the fact.
  DCHECK(frame_node->IsImportant());
}

void FrameThrottlingPolicy::OnBeforeFrameNodeRemoved(
    const FrameNode* frame_node) {
  DCHECK(frame_node->GetRenderFrameHostProxy().is_valid());
  const content::GlobalRenderFrameHostId id =
      frame_node->GetRenderFrameHostProxy().global_frame_routing_id();
  if (!frame_node->IsImportant()) {
    DisableFrameSinkThrottle(id);
  }
}

void FrameThrottlingPolicy::OnIsImportantChanged(const FrameNode* frame_node) {
  DCHECK(frame_node->GetRenderFrameHostProxy().is_valid());
  const content::GlobalRenderFrameHostId id =
      frame_node->GetRenderFrameHostProxy().global_frame_routing_id();
  if (!frame_node->IsImportant()) {
    EnableFrameSinkThrottle(id);
  } else {
    DisableFrameSinkThrottle(id);
  }
}

void FrameThrottlingPolicy::OnPassedToGraph(Graph* graph) {
  graph->AddFrameNodeObserver(this);
}

void FrameThrottlingPolicy::OnTakenFromGraph(Graph* graph) {
  graph->RemoveFrameNodeObserver(this);
}

bool FrameThrottlingPolicy::IsFrameSinkThrottled(
    const content::GlobalRenderFrameHostId& id) const {
  return base::Contains(throttled_frames_, id);
}

void FrameThrottlingPolicy::EnableFrameSinkThrottle(
    const content::GlobalRenderFrameHostId& id) {
  // Only called when a frame goes from important to unimportant, so the frame
  // should not be in the throttled set before insertion.
  bool inserted = throttled_frames_.insert(id).second;
  DCHECK(inserted);
  content::UpdateThrottlingFrameSinks(throttled_frames_, throttle_interval_);
}

void FrameThrottlingPolicy::DisableFrameSinkThrottle(
    const content::GlobalRenderFrameHostId& id) {
  // Only called when a frame goes from unimportant to important or an
  // unimportant frame is to be removed, the frame should be in the set before
  // erasing.
  size_t removed = throttled_frames_.erase(id);
  DCHECK_EQ(removed, 1u);
  content::UpdateThrottlingFrameSinks(throttled_frames_, throttle_interval_);
}

}  // namespace performance_manager::policies
