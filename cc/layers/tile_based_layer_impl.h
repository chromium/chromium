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

  // LayerImpl overrides:
  void AppendQuads(const AppendQuadsContext& context,
                   viz::CompositorRenderPass* render_pass,
                   AppendQuadsData* append_quads_data) override;

  void SetSolidColor(std::optional<SkColor4f> color) { solid_color_ = color; }

 protected:
  TileBasedLayerImpl(LayerTreeImpl* tree_impl, int id);

  std::optional<SkColor4f> solid_color() const { return solid_color_; }

 private:
  // Invoked when the draw mode is DRAW_MODE_RESOURCELESS_SOFTWARE.
  virtual void AppendQuadsForResourcelessSoftwareDraw(
      const AppendQuadsContext& context,
      viz::CompositorRenderPass* render_pass,
      AppendQuadsData* append_quads_data,
      viz::SharedQuadState* shared_quad_state,
      const Occlusion& scaled_occlusion) = 0;

  // Called when AppendQuads() goes through a flow for which behavior is
  // subclass-specific (i.e., not defined in TileBasedLayerImpl::AppendQuads()
  // itself). `quad_offset` is the offset by which appended quads should be
  // adjusted.
  virtual void AppendQuadsSpecialization(
      const AppendQuadsContext& context,
      viz::CompositorRenderPass* render_pass,
      AppendQuadsData* append_quads_data,
      viz::SharedQuadState* shared_quad_state,
      const Occlusion& scaled_occlusion,
      const gfx::Vector2d& quad_offset) = 0;

  virtual float GetMaximumContentsScaleForUseInAppendQuads() = 0;

  virtual bool IsDirectlyCompositedImage() const = 0;

  // Appends a solid-color quad with color `color`.
  void AppendSolidQuad(viz::CompositorRenderPass* render_pass,
                       AppendQuadsData* append_quads_data,
                       SkColor4f color);

  bool is_backdrop_filter_mask_ : 1 = false;
  std::optional<SkColor4f> solid_color_;
};

}  // namespace cc

#endif  // CC_LAYERS_TILE_BASED_LAYER_IMPL_H_
