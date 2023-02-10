// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_TEST_LAYER_TREE_IMPL_H_
#define CC_SLIM_TEST_LAYER_TREE_IMPL_H_

#include "base/containers/flat_set.h"
#include "cc/slim/layer_tree_impl.h"
#include "cc/slim/test_layer_tree_client.h"

namespace cc::slim {

class TestLayerTreeImpl : public LayerTreeImpl {
 public:
  explicit TestLayerTreeImpl(TestLayerTreeClient* client)
      : LayerTreeImpl(client) {}
  ~TestLayerTreeImpl() override = default;

  using LayerTreeImpl::NeedsBeginFrames;
  const base::flat_set<viz::SurfaceRange>& referenced_surfaces() const {
    return referenced_surfaces_;
  }

  void ResetNeedsBeginFrame();
};

}  // namespace cc::slim

#endif  // CC_SLIM_TEST_LAYER_TREE_IMPL_H_
