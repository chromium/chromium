// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_FRAME_THROTTLING_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_FRAME_THROTTLING_POLICY_H_

#include <set>

#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "content/public/browser/frame_rate_throttling.h"
#include "content/public/browser/global_routing_id.h"

namespace performance_manager::policies {

// This policy is responsible for doing specific throttling when the frames
// become visible but unimportant. Currently it can only throttle frame rate.
//
// As for frame rate throttling, it signals the frame sink manager that the
// specified frame sinks should start sending BeginFrames at a specific
// interval.
class FrameThrottlingPolicy : public GraphOwned,
                              public FrameNode::ObserverDefaultImpl {
 public:
  FrameThrottlingPolicy();
  ~FrameThrottlingPolicy() override;
  FrameThrottlingPolicy(const FrameThrottlingPolicy& other) = delete;
  FrameThrottlingPolicy& operator=(const FrameThrottlingPolicy&) = delete;

  // FrameNode::ObserverDefaultImpl:
  void OnFrameNodeAdded(const FrameNode* frame_node) override;
  void OnBeforeFrameNodeRemoved(const FrameNode* frame_node) override;
  void OnIsImportantChanged(const FrameNode* frame_node) override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;

  bool IsFrameSinkThrottled(const content::GlobalRenderFrameHostId& id) const;

 private:
  void EnableFrameSinkThrottle(const content::GlobalRenderFrameHostId& id);
  void DisableFrameSinkThrottle(const content::GlobalRenderFrameHostId& id);

  // The set of GlobalRenderFrameHostId, their corresponding frames are
  // throttled to send BeginFrames at a specific interval.
  std::set<content::GlobalRenderFrameHostId> throttled_frames_;

  base::TimeDelta throttle_interval_;
};

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_FRAME_THROTTLING_POLICY_H_
