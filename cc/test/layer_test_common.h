// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_LAYER_TEST_COMMON_H_
#define CC_TEST_LAYER_TEST_COMMON_H_

#include "cc/layers/layer_collections.h"

#define EXPECT_SET_NEEDS_COMMIT(expect, code_to_test)                 \
  do {                                                                \
    EXPECT_CALL(*layer_tree_host_, SetNeedsCommit()).Times((expect)); \
    code_to_test;                                                     \
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());         \
  } while (false)

#define EXPECT_SET_NEEDS_UPDATE(expect, code_to_test)                       \
  do {                                                                      \
    EXPECT_CALL(*layer_tree_host_, SetNeedsUpdateLayers()).Times((expect)); \
    code_to_test;                                                           \
    Mock::VerifyAndClearExpectations(layer_tree_host_.get());               \
  } while (false)

namespace gfx {
class Rect;
}

namespace viz {
class QuadList;
}

namespace cc {

class LayerTreeHost;
class LayerTreeImpl;

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

}  // namespace cc

#endif  // CC_TEST_LAYER_TEST_COMMON_H_
