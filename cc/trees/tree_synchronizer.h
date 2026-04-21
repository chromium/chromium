// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_TREE_SYNCHRONIZER_H_
#define CC_TREES_TREE_SYNCHRONIZER_H_

#include <string>
#include <vector>

#include "cc/cc_export.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

class LayerTreeImpl;

class CC_EXPORT TreeSynchronizer {
 public:
  // Not instantiable.
  TreeSynchronizer() = delete;

  // Synchronizes main-thread layers to impl layers
  static void SynchronizeTrees(const CommitState& commit_state,
                               const ThreadUnsafeCommitState& unsafe_state,
                               LayerTreeImpl* pending_tree);
  static void PushLayerProperties(CommitState& commit_state,
                                  const ThreadUnsafeCommitState& unsafe_state,
                                  LayerTreeImpl* impl_tree);

  // Synchronizes pending tree impl layers to the active tree
  static void SynchronizeTrees(LayerTreeImpl* pending_tree,
                               LayerTreeImpl* active_tree);
  static void PushLayerProperties(LayerTreeImpl* pending_tree,
                                  LayerTreeImpl* active_tree);

  // Reorders layers for trees-in-viz
  static base::expected<void, std::string> SynchronizeLayerOrder(
      const std::vector<int32_t>& layer_order,
      LayerTreeImpl& tree);
};

}  // namespace cc

#endif  // CC_TREES_TREE_SYNCHRONIZER_H_
