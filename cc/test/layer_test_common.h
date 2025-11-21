// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TEST_COMMON_H_
#define CC_TEST_LAYER_TEST_COMMON_H_

#include "cc/layers/layer_collections.h"

namespace gfx {
class Rect;
}

namespace viz {
class QuadList;
}

namespace cc {

class LayerTreeHost;
class LayerTreeImpl;
class LayerTreeSettings;

// LayerTreeSettings with different combinations of
// commit_to_active_tree and use_layer_lists.
LayerTreeSettings CommitToActiveTreeLayerListSettings();
LayerTreeSettings CommitToPendingTreeLayerListSettings();
LayerTreeSettings CommitToPendingTreeLayerTreeSettings();

// In tests that build layer tree and property trees directly at impl-side,
// before calling LayerTreeImpl::UpdateDrawProperties() or any function calling
// it, we should call this function to make the layer tree and property trees
// ready for draw property update.
void PrepareForUpdateDrawProperties(LayerTreeImpl*);

// Calls LayerTreeImpl::UpdateDrawProperties() after calling the above function.
// If |output_update_layer_list| is not null, it accepts layers output from
// draw_property_utils::FindLayersThatNeedUpdates().
void UpdateDrawProperties(LayerTreeImpl*,
                          LayerImplList* output_update_layer_list = nullptr);

// Main-thread counterpart of the above function. If |output_update_layer_list|
// is not null, it accepts layers output from
// draw_property_utils::FindLayersThatNeedUpdates().
void UpdateDrawProperties(LayerTreeHost*,
                          LayerList* output_update_layer_list = nullptr);

// Set device scale factor and update device viewport rect to be the root layer
// size scaled by the device scale factor.
void SetDeviceScaleAndUpdateViewportRect(LayerTreeImpl*,
                                         float device_scale_factor);
void SetDeviceScaleAndUpdateViewportRect(LayerTreeHost*,
                                         float device_scale_factor);

void VerifyQuadsExactlyCoverRect(const viz::QuadList& quads,
                                 const gfx::Rect& rect);

void VerifyQuadsAreOccluded(const viz::QuadList& quads,
                            const gfx::Rect& occluded,
                            size_t* partially_occluded_count);

enum LayerTreeImplTestMode {
  CommitToActiveTree,
  CommitToPendingTree,
  CommitToActiveTreeTreesInVizClient,
  CommitToPendingTreeTreesInVizClient,
  CommitToActiveTreeTreesInVizService,
};

#define INSTANTIATE_COMMIT_TO_TREE_BASE_TEST_P(name, ...)         \
  INSTANTIATE_TEST_SUITE_P(                                       \
      , name, ::testing::Values(__VA_ARGS__),                     \
      [](const ::testing::TestParamInfo<name::ParamType>& info) { \
        switch (info.param) {                                     \
          case CommitToActiveTree:                                \
            return "CommitToActiveTree";                          \
          case CommitToPendingTree:                               \
            return "CommitToPendingTree";                         \
          case CommitToActiveTreeTreesInVizClient:                \
            return "CommitToActiveTreeTreesInVizClient";          \
          case CommitToPendingTreeTreesInVizClient:               \
            return "CommitToPendingTreeTreesInVizClient";         \
          case CommitToActiveTreeTreesInVizService:               \
            return "CommitToActiveTreeTreesInVizService";         \
          default:                                                \
            NOTREACHED();                                         \
        }                                                         \
      })

// For parameterized test suites testing all tree modes including
// CommitToActiveTree / CommitToPendingTree, for all valid TreesInViz
// Client / Service combinations.
#define INSTANTIATE_COMMIT_TO_TREE_TEST_P(name)                                \
  INSTANTIATE_COMMIT_TO_TREE_BASE_TEST_P(                                      \
      name, CommitToActiveTree, CommitToPendingTree,                           \
      CommitToActiveTreeTreesInVizClient, CommitToPendingTreeTreesInVizClient, \
      CommitToActiveTreeTreesInVizService)

// For parameterized test suites testing all tree modes including
// CommitToActiveTree / CommitToPendingTree, excluding TreesInViz
// Client mode.
#define INSTANTIATE_COMPOSITOR_FRAME_PRODUCING_TREE_TEST_P(name)   \
  INSTANTIATE_COMMIT_TO_TREE_BASE_TEST_P(name, CommitToActiveTree, \
                                         CommitToPendingTree,      \
                                         CommitToActiveTreeTreesInVizService)

// For parameterized test suites testing all tree modes including
// CommitToActiveTree / CommitToPendingTree, excluding TreesInViz
// Service mode.
#define INSTANTIATE_CLIENT_MODE_TREE_TEST_P(name)    \
  INSTANTIATE_COMMIT_TO_TREE_BASE_TEST_P(            \
      name, CommitToActiveTree, CommitToPendingTree, \
      CommitToActiveTreeTreesInVizClient, CommitToPendingTreeTreesInVizClient)

// For parameterized test suites testing commits to Pending
// tree modes only. This excludes TreesInViz Service mode by, since
// it does not have a Pending tree.
#define INSTANTIATE_COMMIT_TO_PENDING_TREE_TEST_P(name)             \
  INSTANTIATE_COMMIT_TO_TREE_BASE_TEST_P(name, CommitToPendingTree, \
                                         CommitToPendingTreeTreesInVizClient)

}  // namespace cc

#endif  // CC_TEST_LAYER_TEST_COMMON_H_
