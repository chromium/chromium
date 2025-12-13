// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_DISCARD_PAGE_WITH_CRASHED_SUBFRAME_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_DISCARD_PAGE_WITH_CRASHED_SUBFRAME_POLICY_H_

#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"

namespace performance_manager::policies {

// Policy to discard a page if one of its subframes crashes.
//
// With ReloadHiddenTabsWithCrashedSubframes feature enabled, a invisible page
// will be reloaded if a subframe is crashed (e.g. Low-Memory-Kill on Android)
// when the page become visible again. This policy is additional to that
// feature. It discards a page if a subframe is crashed because keeping the
// other renderer processes for the page alive is not necessary but rather waste
// of memory.
//
// On Android, the behavior of discarding a page on memory pressure (i.e. LMK)
// matches the behavior on other desktop platforms of
// UrgentPageDiscardingPolicy.
class DiscardPageWithCrashedSubframePolicy : public GraphOwned,
                                             public FrameNodeObserver {
 public:
  DiscardPageWithCrashedSubframePolicy();
  ~DiscardPageWithCrashedSubframePolicy() override;
  DiscardPageWithCrashedSubframePolicy(
      const DiscardPageWithCrashedSubframePolicy& other) = delete;
  DiscardPageWithCrashedSubframePolicy& operator=(
      const DiscardPageWithCrashedSubframePolicy&) = delete;

  // FrameNodeObserver:
  void OnCrossProcessSubframeRenderProcessGone(
      const FrameNode* frame_node) override;

  // GraphOwned:
  void OnPassedToGraph(Graph* graph) override;
  void OnTakenFromGraph(Graph* graph) override;
};

}  // namespace performance_manager::policies

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_POLICIES_DISCARD_PAGE_WITH_CRASHED_SUBFRAME_POLICY_H_
