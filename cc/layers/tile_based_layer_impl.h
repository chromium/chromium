// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_TILE_BASED_LAYER_IMPL_H_
#define CC_LAYERS_TILE_BASED_LAYER_IMPL_H_

#include "cc/cc_export.h"
#include "cc/layers/layer_impl.h"

namespace cc {

// Base class for layer impls that manipulate tiles (e.g., PictureLayerImpl
// and TileDisplayLayerImpl).
class CC_EXPORT TileBasedLayerImpl : public LayerImpl {
 public:
  TileBasedLayerImpl(const TileBasedLayerImpl&) = delete;
  ~TileBasedLayerImpl() override;

  TileBasedLayerImpl& operator=(const TileBasedLayerImpl&) = delete;

  void SetIsBackdropFilterMask(bool is_backdrop_filter_mask) {
    if (this->is_backdrop_filter_mask() == is_backdrop_filter_mask) {
      return;
    }
    is_backdrop_filter_mask_ = is_backdrop_filter_mask;
    SetNeedsPushProperties();
  }

  bool is_backdrop_filter_mask() const { return is_backdrop_filter_mask_; }

 protected:
  TileBasedLayerImpl(LayerTreeImpl* tree_impl, int id);

  // Appends a solid-color quad with color `color`.
  void AppendSolidQuad(viz::CompositorRenderPass* render_pass,
                       AppendQuadsData* append_quads_data,
                       SkColor4f color);

 private:
  bool is_backdrop_filter_mask_ : 1 = false;
};

}  // namespace cc

#endif  // CC_LAYERS_TILE_BASED_LAYER_IMPL_H_
