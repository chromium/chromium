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
LayerTreeSettings CommitToActiveTreeLayerTreeSettings();
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

// For parameterized test suites testing with both CommitToActiveTree (with
// the bool parameter being true) and !CommitToActiveTree settings.
#define INSTANTIATE_COMMIT_TO_TREE_TEST_P(name)                           \
  INSTANTIATE_TEST_SUITE_P(                                               \
      , name, ::testing::Bool(),                                          \
      [](const ::testing::TestParamInfo<name::ParamType>& info) {         \
        return info.param ? "CommitToActiveTree" : "CommitToPendingTree"; \
      })

}  // namespace cc

#endif  // CC_TEST_LAYER_TEST_COMMON_H_
