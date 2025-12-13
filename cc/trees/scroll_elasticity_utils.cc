// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/scroll_elasticity_utils.h"

#include "cc/trees/layer_tree_host_impl.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"

namespace cc::scroll_elasticity_utils {
bool ShouldAllowOverscrollEffect(const ScrollNode& node,
                                 const ScrollTree& scroll_tree,
                                 const LayerTreeHostImpl* host_impl) {
  // Always allow root elastic overscroll
  if (node.scrolls_inner_viewport || node.scrolls_outer_viewport) {
    return true;
  }
  // Allow composited or threaded scrollers.
  const bool allow_pending_tree =
      !host_impl || !host_impl->CommitsToActiveTree();
  return scroll_tree.CanRealizeScrollsOnActiveTree(node) ||
         (allow_pending_tree &&
          scroll_tree.CanRealizeScrollsOnPendingTree(node));
}
}  // namespace cc::scroll_elasticity_utils
