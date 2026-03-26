// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_TREE_SYNCHRONIZER_H_
#define CC_TREES_TREE_SYNCHRONIZER_H_

#include "cc/cc_export.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

class LayerImpl;
class LayerTreeImpl;
class Layer;

class CC_EXPORT TreeSynchronizer {
 public:
  // Not instantiable.
  TreeSynchronizer() = delete;

  // Accepts a Layer tree and returns a reference to a LayerImpl tree that
  // duplicates the structure of the Layer tree, reusing the LayerImpls in the
  // tree provided by old_layer_impl_root if possible.
  static void SynchronizeTrees(const CommitState& commit_state,
                               const ThreadUnsafeCommitState& unsafe_state,
                               LayerTreeImpl* tree_impl);

  static void SynchronizeTrees(LayerTreeImpl* pending_tree,
                               LayerTreeImpl* active_tree);

  static void PushLayerProperties(const CommitState& commit_state,
                                  const ThreadUnsafeCommitState& unsafe_state,
                                  LayerTreeImpl* impl_tree);

  static void PushLayerProperties(LayerTreeImpl* pending_tree,
                                  LayerTreeImpl* active_tree);
};

}  // namespace cc

#endif  // CC_TREES_TREE_SYNCHRONIZER_H_
