// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_TILE_BASED_LAYER_IMPL_H_
#define CC_LAYERS_TILE_BASED_LAYER_IMPL_H_

#include <algorithm>

#include "cc/base/math_util.h"
#include "cc/cc_export.h"
#include "cc/debug/debug_colors.h"
#include "cc/layers/append_quads_context.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/solid_color_layer_impl.h"
#include "cc/tiles/tiling_set_coverage_iterator.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"

namespace cc {

// Base class for layer impls that manipulate tiles (e.g., PictureLayerImpl
// and TileDisplayLayerImpl).
template <typename Tiling>
class CC_EXPORT TileBasedLayerImpl : public LayerImpl {
 public:
  enum class TilingResolution {
    kHigh,
    kAboveHigh,
    kBelowHigh,
  };

  TileBasedLayerImpl(const TileBasedLayerImpl&) = delete;
  ~TileBasedLayerImpl() override = default;

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
  TileBasedLayerImpl(LayerTreeImpl* tree_impl, int id)
      : LayerImpl(tree_impl, id) {}

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
  // NOTE: `shared_quad_state` is *not* adjusted by `quad_offset` when passed
  // into this method to allow implementations to operate on the original state
  // (e.g., to locate tiles in layer space). However, it will be properly
  // adjusted before AppendQuads() returns to the caller.
  virtual void AppendQuadsSpecialization(
      const AppendQuadsContext& context,
      viz::CompositorRenderPass* render_pass,
      AppendQuadsData* append_quads_data,
      viz::SharedQuadState* shared_quad_state,
      const Occlusion& scaled_occlusion,
      const gfx::Vector2d& quad_offset) = 0;

  virtual float GetMaximumContentsScaleForUseInAppendQuads() = 0;

  virtual bool IsDirectlyCompositedImage() const = 0;

  virtual TilingResolution GetTilingResolutionForDebugBorders(
      const Tiling* tiling) const = 0;

  virtual TilingSetCoverageIterator<Tiling> Cover(
      const gfx::Rect& coverage_rect,
      float coverage_scale,
      float ideal_contents_scale) = 0;

  virtual float GetIdealContentsScaleKey() const = 0;

  // Appends a solid-color quad with color `color`.
  void AppendSolidQuad(viz::CompositorRenderPass* render_pass,
                       AppendQuadsData* append_quads_data,
                       SkColor4f color);

  bool is_backdrop_filter_mask_ : 1 = false;
  std::optional<SkColor4f> solid_color_;
};

template <typename Tiling>
void TileBasedLayerImpl<Tiling>::AppendQuads(
    const AppendQuadsContext& context,
    viz::CompositorRenderPass* render_pass,
    AppendQuadsData* append_quads_data) {
  // RenderSurfaceImpl::AppendQuads sets mask properties in the DrawQuad for
  // the masked surface, which will apply to both the backdrop filter and the
  // contents of the masked surface, so we should not append quads of the mask
  // layer in DstIn blend mode which would apply the mask in another codepath.
  if (is_backdrop_filter_mask()) {
    return;
  }

  if (solid_color()) {
    AppendSolidQuad(render_pass, append_quads_data, *solid_color());
    return;
  }

  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateScaledSharedQuadState(shared_quad_state,
                                GetMaximumContentsScaleForUseInAppendQuads(),
                                contents_opaque());

  if (IsDirectlyCompositedImage()) {
    // Directly composited images should be clipped to the layer's content rect.
    // When a PictureLayerTiling is created for a directly composited image, the
    // layer bounds are multiplied by the raster scale in order to compute the
    // tile size. If the aspect ratio of the layer doesn't match that of the
    // image, it's possible that one of the dimensions of the resulting size
    // (layer bounds * raster scale) is a fractional number, as raster scale
    // does not scale x and y independently.
    // When this happens, the ToEnclosingRect() operation in
    // |PictureLayerTiling::EnclosingContentsRectFromLayer()| will
    // create a tiling that, when scaled by |max_contents_scale| above, is
    // larger than the layer bounds by a fraction of a pixel.
    gfx::Rect bounds_in_target_space = MathUtil::MapEnclosingClippedRect(
        draw_properties().target_space_transform, gfx::Rect(bounds()));
    if (is_clipped()) {
      bounds_in_target_space.Intersect(draw_properties().clip_rect);
    }

    if (shared_quad_state->clip_rect) {
      bounds_in_target_space.Intersect(*shared_quad_state->clip_rect);
    }

    shared_quad_state->clip_rect = bounds_in_target_space;
  }

  const Occlusion scaled_occlusion =
      draw_properties()
          .occlusion_in_content_space.GetOcclusionWithGivenDrawTransform(
              shared_quad_state->quad_to_target_transform);

  if (context.draw_mode == DRAW_MODE_RESOURCELESS_SOFTWARE) {
    AppendQuadsForResourcelessSoftwareDraw(context, render_pass,
                                           append_quads_data, shared_quad_state,
                                           scaled_occlusion);
    return;
  }

  // If the visible rect is scrolled far enough away, then we may run into a
  // floating point precision in AA calculations in the renderer. See
  // crbug.com/765297. In order to avoid this, we shift the quads up from where
  // they logically reside and adjust the shared_quad_state's transform instead.
  // We only do this in scale/translate matrices to ensure the math is correct.
  // NOTE: Implementations of AppendQuadsSpecialization() need to use the
  // original state in `shared_quad_state` to correctly locate the tiles to
  // draw. For this reason, we delay adjusting `shared_quad_state` itself until
  // the bottom of the method below.
  gfx::Vector2d quad_offset;
  if (shared_quad_state->quad_to_target_transform.IsScaleOrTranslation()) {
    const auto& visible_rect = shared_quad_state->visible_quad_layer_rect;
    quad_offset = gfx::Vector2d(-visible_rect.x(), -visible_rect.y());
  }

  gfx::Rect debug_border_rect(shared_quad_state->quad_layer_rect);
  debug_border_rect.Offset(quad_offset);
  AppendDebugBorderQuad(render_pass, debug_border_rect, shared_quad_state,
                        append_quads_data);

  const float device_scale_factor = layer_tree_impl()->device_scale_factor();
  const float max_contents_scale = GetMaximumContentsScaleForUseInAppendQuads();
  const float ideal_scale_key = GetIdealContentsScaleKey();

  // Append debug borders for the quads in this layer.
  if (ShowDebugBorders(DebugBorderType::LAYER)) {
    for (auto iter = Cover(shared_quad_state->visible_quad_layer_rect,
                           max_contents_scale, ideal_scale_key);
         iter; ++iter) {
      SkColor4f color;
      float width;
      if (*iter && iter->IsReadyToDraw()) {
        TileDrawInfo::Mode mode = iter->draw_mode();
        if (mode == TileDrawInfo::SOLID_COLOR_MODE) {
          color = DebugColors::SolidColorTileBorderColor();
          width = DebugColors::SolidColorTileBorderWidth(device_scale_factor);
        } else if (mode == TileDrawInfo::OOM_MODE) {
          color = DebugColors::OOMTileBorderColor();
          width = DebugColors::OOMTileBorderWidth(device_scale_factor);
        } else {
          switch (GetTilingResolutionForDebugBorders(iter.CurrentTiling())) {
            case TilingResolution::kHigh:
              color = DebugColors::HighResTileBorderColor();
              width = DebugColors::HighResTileBorderWidth(device_scale_factor);
              break;
            case TilingResolution::kAboveHigh:
              color = DebugColors::AboveHighResTileBorderColor();
              width =
                  DebugColors::AboveHighResTileBorderWidth(device_scale_factor);
              break;
            case TilingResolution::kBelowHigh:
              color = DebugColors::BelowHighResTileBorderColor();
              width =
                  DebugColors::BelowHighResTileBorderWidth(device_scale_factor);
              break;
          }
        }
      } else {
        color = DebugColors::MissingTileBorderColor();
        width = DebugColors::MissingTileBorderWidth(device_scale_factor);
      }

      auto* debug_border_quad =
          render_pass->CreateAndAppendDrawQuad<viz::DebugBorderDrawQuad>();
      gfx::Rect geometry_rect = iter.geometry_rect();
      geometry_rect.Offset(quad_offset);
      gfx::Rect visible_geometry_rect = geometry_rect;
      debug_border_quad->SetNew(shared_quad_state, geometry_rect,
                                visible_geometry_rect, color, width);
    }
  }

  AppendQuadsSpecialization(context, render_pass, append_quads_data,
                            shared_quad_state, scaled_occlusion, quad_offset);

  // Adjust shared_quad_state with the quad_offset, since by contract
  // AppendQuadsSpecialization() has adjusted each quad appended by that offset.
  shared_quad_state->quad_to_target_transform.Translate(-quad_offset);
  shared_quad_state->quad_layer_rect.Offset(quad_offset);
  shared_quad_state->visible_quad_layer_rect.Offset(quad_offset);
}

template <typename Tiling>
void TileBasedLayerImpl<Tiling>::AppendSolidQuad(
    viz::CompositorRenderPass* render_pass,
    AppendQuadsData* append_quads_data,
    SkColor4f color) {
  // TODO(crbug.com/41468388): This is still hard-coded at 1.0. This has some
  // history:
  //   - for crbug.com/769319, the contents scale was allowed to change, to
  //     avoid blurring on high-dpi screens.
  //   - for crbug.com/796558, the max device scale was hard-coded back to 1.0
  //     for single-tile masks, to avoid problems with transforms.
  // To avoid those transform/scale bugs, this is currently left at 1.0. See
  // crbug.com/979672 for more context and test links.
  float max_contents_scale = 1;

  // The downstream CA layers use shared_quad_state to generate resources of
  // the right size even if it is a solid color picture layer.
  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  PopulateScaledSharedQuadState(shared_quad_state, max_contents_scale,
                                contents_opaque());

  AppendDebugBorderQuad(render_pass, gfx::Rect(bounds()), shared_quad_state,
                        append_quads_data);

  gfx::Rect scaled_visible_layer_rect =
      shared_quad_state->visible_quad_layer_rect;
  Occlusion occlusion = draw_properties().occlusion_in_content_space;

  EffectNode* effect_node = GetEffectTree().Node(effect_tree_index());
  SolidColorLayerImpl::AppendSolidQuads(
      render_pass, occlusion, shared_quad_state, scaled_visible_layer_rect,
      color, !layer_tree_impl()->settings().enable_edge_anti_aliasing,
      effect_node->blend_mode, append_quads_data);
}

}  // namespace cc

#endif  // CC_LAYERS_TILE_BASED_LAYER_IMPL_H_
