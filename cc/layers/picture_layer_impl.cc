// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "cc/base/features.h"
#include "cc/base/math_util.h"
#include "cc/benchmarks/micro_benchmark_impl.h"
#include "cc/debug/debug_colors.h"
#include "cc/layers/append_quads_data.h"
#include "cc/layers/solid_color_layer_impl.h"
#include "cc/paint/display_item_list.h"
#include "cc/tiles/tile_manager.h"
#include "cc/tiles/tiling_set_raster_queue_all.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion.h"
#include "cc/trees/transform_node.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/picture_draw_quad.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/traced_value.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"

namespace cc {
namespace {
// This must be > 1 as we multiply or divide by this to find a new raster
// scale during pinch.
const float kMaxScaleRatioDuringPinch = 2.0f;

// When creating a new tiling during pinch, snap to an existing
// tiling's scale if the desired scale is within this ratio.
const float kSnapToExistingTilingRatio = 1.2f;

// Large contents scale can cause overflow issues. Cap the ideal contents scale
// by this constant, since scales larger than this are usually not correct or
// their scale doesn't matter as long as it's large. Content scales usually
// closely match the default device-scale factor (so it's usually <= 5). See
// Renderer4.IdealContentsScale UMA (deprecated) for distribution of content
// scales.
const float kMaxIdealContentsScale = 10000.f;

// We try to avoid raster scale adjustment for will-change:transform for
// performance, unless the scale is too small compared to the ideal scale and
// the native scale.
const float kMinScaleRatioForWillChangeTransform = 0.25f;

// Used to avoid raster scale adjustment during a transform animation by
// using the maximum animation scale, but sometimes the maximum animation scale
// can't be accurately calculated (e.g. with nested scale transforms). We'll
// adjust raster scale if it is not affected by invalid scale and is smaller
// than the ideal scale divided by this ratio. The situation is rare.
// See PropertyTrees::MaximumAnimationToScreenScale() and
// AnimationAffectedByInvalidScale().
const float kRatioToAdjustRasterScaleForTransformAnimation = 1.5f;

// Intersect rects which may have right() and bottom() that overflow integer
// boundaries. This code is similar to gfx::Rect::Intersect with the exception
// that the types are promoted to int64_t when there is a chance of overflow.
gfx::Rect SafeIntersectRects(const gfx::Rect& one, const gfx::Rect& two) {
  if (one.IsEmpty() || two.IsEmpty())
    return gfx::Rect();

  int rx = std::max(one.x(), two.x());
  int ry = std::max(one.y(), two.y());
  int64_t rr = std::min(static_cast<int64_t>(one.x()) + one.width(),
                        static_cast<int64_t>(two.x()) + two.width());
  int64_t rb = std::min(static_cast<int64_t>(one.y()) + one.height(),
                        static_cast<int64_t>(two.y()) + two.height());
  if (rx > rr || ry > rb)
    return gfx::Rect();
  return gfx::Rect(rx, ry, static_cast<int>(rr - rx),
                   static_cast<int>(rb - ry));
}

}  // namespace

PictureLayerImpl::PictureLayerImpl(LayerTreeImpl* tree_impl, int id)
    : LayerImpl(tree_impl,
                id,
                tree_impl->always_push_properties_on_picture_layers()) {
  layer_tree_impl()->RegisterPictureLayerImpl(this);
}

PictureLayerImpl::~PictureLayerImpl() {
  if (twin_layer_)
    twin_layer_->twin_layer_ = nullptr;

  // We only track PaintWorklet-containing PictureLayerImpls on the pending
  // tree. However this deletion may happen outside the commit flow when we are
  // on the recycle tree instead, so just check !IsActiveTree().
  if (!paint_worklet_records_.empty() && !layer_tree_impl()->IsActiveTree())
    layer_tree_impl()->NotifyLayerHasPaintWorkletsChanged(this, false);

  // Similarly, AnimatedPaintWorkletTracker is only valid on the pending tree.
  if (!layer_tree_impl()->IsActiveTree()) {
    layer_tree_impl()
        ->paint_worklet_tracker()
        .UpdatePaintWorkletInputProperties({}, this);
  }

  layer_tree_impl()->UnregisterPictureLayerImpl(this);

  // Unregister for all images on the current raster source.
  UnregisterAnimatedImages();
}

mojom::LayerType PictureLayerImpl::GetLayerType() const {
  return mojom::LayerType::kPicture;
}

std::unique_ptr<LayerImpl> PictureLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return PictureLayerImpl::Create(tree_impl, id());
}

void PictureLayerImpl::PushPropertiesTo(LayerImpl* base_layer) {
  PictureLayerImpl* layer_impl = static_cast<PictureLayerImpl*>(base_layer);

  layer_impl->has_animated_image_update_rect_ = has_animated_image_update_rect_;
  layer_impl->has_non_animated_image_update_rect_ =
      has_non_animated_image_update_rect_;

  LayerImpl::PushPropertiesTo(base_layer);

  // Twin relationships should never change once established.
  DCHECK(!twin_layer_ || twin_layer_ == layer_impl);
  DCHECK(!twin_layer_ || layer_impl->twin_layer_ == this);
  // The twin relationship does not need to exist before the first
  // PushPropertiesTo from pending to active layer since before that the active
  // layer can not have a pile or tilings, it has only been created and inserted
  // into the tree at that point.
  twin_layer_ = layer_impl;
  layer_impl->twin_layer_ = this;

  layer_impl->SetIsBackdropFilterMask(is_backdrop_filter_mask_);

  // Solid color layers have no tilings.
  DCHECK(!raster_source_->IsSolidColor() || tilings_->num_tilings() == 0);
  // The pending tree should only have a high res (and possibly low res) tiling.
  DCHECK_LE(tilings_->num_tilings(),
            layer_tree_impl()->create_low_res_tiling() ? 2u : 1u);

  layer_impl->set_gpu_raster_max_texture_size(gpu_raster_max_texture_size_);
  layer_impl->UpdateRasterSourceInternal(
      raster_source_, &invalidation_, tilings_.get(), &paint_worklet_records_,
      discardable_image_map_.get());
  DCHECK(invalidation_.IsEmpty());

  // After syncing a solid color layer, the active layer has no tilings.
  DCHECK(!raster_source_->IsSolidColor() ||
         layer_impl->tilings_->num_tilings() == 0);

  layer_impl->raster_page_scale_ = raster_page_scale_;
  layer_impl->raster_device_scale_ = raster_device_scale_;
  layer_impl->raster_source_scale_ = raster_source_scale_;
  layer_impl->raster_contents_scale_ = raster_contents_scale_;
  layer_impl->low_res_raster_contents_scale_ = low_res_raster_contents_scale_;
  // Simply push the value to the active tree without any extra invalidations,
  // since the pending tree tiles would have this handled. This is here to
  // ensure the state is consistent for future raster.
  layer_impl->lcd_text_disallowed_reason_ = lcd_text_disallowed_reason_;

  if (layer_tree_impl()->settings().UseLayerContextForDisplay()) {
    // Move tile updates over to the active layer so they get pushed to the
    // display tree. Note that active layers never accumulate their own tile
    // updates, so replacement is safe.
    layer_impl->updated_tiles_ = std::move(updated_tiles_);
    updated_tiles_.clear();
    layer_impl->SetNeedsPushProperties();
  }

  layer_impl->SanityCheckTilingState();
}

void PictureLayerImpl::AppendQuads(viz::CompositorRenderPass* render_pass,
                                   AppendQuadsData* append_quads_data) {
  // RenderSurfaceImpl::AppendQuads sets mask properties in the DrawQuad for
  // the masked surface, which will apply to both the backdrop filter and the
  // contents of the masked surface, so we should not append quads of the mask
  // layer in DstIn blend mode which would apply the mask in another codepath.
  if (is_backdrop_filter_mask_)
    return;

  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();

  if (raster_source_->IsSolidColor()) {
    // TODO(crbug.com/41468388): This is still hard-coded at 1.0. This has some
    // history:
    //  - for crbug.com/769319, the contents scale was allowed to change, to
    //    avoid blurring on high-dpi screens.
    //  - for crbug.com/796558, the max device scale was hard-coded back to 1.0
    //    for single-tile masks, to avoid problems with transforms.
    // To avoid those transform/scale bugs, this is currently left at 1.0. See
    // crbug.com/979672 for more context and test links.
    float max_contents_scale = 1;

    // The downstream CA layers use shared_quad_state to generate resources of
    // the right size even if it is a solid color picture layer.
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
        raster_source_->GetSolidColor(),
        !layer_tree_impl()->settings().enable_edge_anti_aliasing,
        effect_node->blend_mode, append_quads_data);
    return;
  }

  float device_scale_factor = layer_tree_impl()->device_scale_factor();
  // If we don't have tilings, we're likely going to append a checkerboard quad
  // the size of the layer. In that case, use scale 1 for more stable
  // to-screen-space mapping.
  float max_contents_scale =
      tilings_->num_tilings() ? MaximumTilingContentsScale() : 1.f;
  PopulateScaledSharedQuadState(shared_quad_state, max_contents_scale,
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
    if (is_clipped())
      bounds_in_target_space.Intersect(draw_properties().clip_rect);

    if (shared_quad_state->clip_rect)
      bounds_in_target_space.Intersect(*shared_quad_state->clip_rect);

    shared_quad_state->clip_rect = bounds_in_target_space;
  }

  Occlusion scaled_occlusion =
      draw_properties()
          .occlusion_in_content_space.GetOcclusionWithGivenDrawTransform(
              shared_quad_state->quad_to_target_transform);

  if (current_draw_mode_ == DRAW_MODE_RESOURCELESS_SOFTWARE) {
    DCHECK(shared_quad_state->quad_layer_rect.origin() == gfx::Point(0, 0));
    AppendDebugBorderQuad(
        render_pass, shared_quad_state->quad_layer_rect, shared_quad_state,
        append_quads_data, DebugColors::DirectPictureBorderColor(),
        DebugColors::DirectPictureBorderWidth(device_scale_factor));

    gfx::Rect geometry_rect = shared_quad_state->visible_quad_layer_rect;
    gfx::Rect visible_geometry_rect =
        scaled_occlusion.GetUnoccludedContentRect(geometry_rect);
    bool needs_blending = !contents_opaque();

    // The raster source may not be valid over the entire visible rect,
    // and rastering outside of that may cause incorrect pixels.
    gfx::Rect scaled_recorded_bounds = gfx::ScaleToEnclosingRect(
        raster_source_->recorded_bounds(), max_contents_scale);
    geometry_rect.Intersect(scaled_recorded_bounds);
    visible_geometry_rect.Intersect(scaled_recorded_bounds);

    if (visible_geometry_rect.IsEmpty())
      return;

    DCHECK(raster_source_->HasRecordings());
    gfx::Rect quad_content_rect = shared_quad_state->visible_quad_layer_rect;
    gfx::Size texture_size = quad_content_rect.size();
    gfx::RectF texture_rect = gfx::RectF(gfx::SizeF(texture_size));

    viz::PictureDrawQuad::ImageAnimationMap image_animation_map;
    const auto* controller = layer_tree_impl()->image_animation_controller();
    WhichTree tree = layer_tree_impl()->IsPendingTree()
                         ? WhichTree::PENDING_TREE
                         : WhichTree::ACTIVE_TREE;
    for (const auto& image_data :
         discardable_image_map_->animated_images_metadata()) {
      image_animation_map[image_data.paint_image_id] =
          controller->GetFrameIndexForImage(image_data.paint_image_id, tree);
    }

    auto* quad = render_pass->CreateAndAppendDrawQuad<viz::PictureDrawQuad>();
    quad->SetNew(
        shared_quad_state, geometry_rect, visible_geometry_rect, needs_blending,
        texture_rect, texture_size, nearest_neighbor_, quad_content_rect,
        max_contents_scale, std::move(image_animation_map),
        raster_source_->GetDisplayItemList(), GetRasterInducingScrollOffsets());
    ValidateQuadResources(quad);
    return;
  }

  // If we're doing a regular AppendQuads (ie, not solid color or resourceless
  // software draw, and if the visible rect is scrolled far enough away, then we
  // may run into a floating point precision in AA calculations in the renderer.
  // See crbug.com/765297. In order to avoid this, we shift the quads up from
  // where they logically reside and adjust the shared_quad_state's transform
  // instead. We only do this in a scale/translate matrices to ensure the math
  // is correct.
  gfx::Vector2d quad_offset;
  if (shared_quad_state->quad_to_target_transform.IsScaleOrTranslation()) {
    const auto& visible_rect = shared_quad_state->visible_quad_layer_rect;
    quad_offset = gfx::Vector2d(-visible_rect.x(), -visible_rect.y());
  }

  gfx::Rect debug_border_rect(shared_quad_state->quad_layer_rect);
  debug_border_rect.Offset(quad_offset);
  AppendDebugBorderQuad(render_pass, debug_border_rect, shared_quad_state,
                        append_quads_data);

  if (ShowDebugBorders(DebugBorderType::LAYER)) {
    for (auto iter =
             tilings_->Cover(shared_quad_state->visible_quad_layer_rect,
                             max_contents_scale, ideal_contents_scale_key());
         iter; ++iter) {
      SkColor4f color;
      float width;
      if (*iter && iter->draw_info().IsReadyToDraw()) {
        TileDrawInfo::Mode mode = iter->draw_info().mode();
        if (mode == TileDrawInfo::SOLID_COLOR_MODE) {
          color = DebugColors::SolidColorTileBorderColor();
          width = DebugColors::SolidColorTileBorderWidth(device_scale_factor);
        } else if (mode == TileDrawInfo::OOM_MODE) {
          color = DebugColors::OOMTileBorderColor();
          width = DebugColors::OOMTileBorderWidth(device_scale_factor);
        } else if (iter.resolution() == HIGH_RESOLUTION) {
          color = DebugColors::HighResTileBorderColor();
          width = DebugColors::HighResTileBorderWidth(device_scale_factor);
        } else if (iter.resolution() == LOW_RESOLUTION) {
          color = DebugColors::LowResTileBorderColor();
          width = DebugColors::LowResTileBorderWidth(device_scale_factor);
        } else if (iter->contents_scale_key() > max_contents_scale) {
          color = DebugColors::ExtraHighResTileBorderColor();
          width = DebugColors::ExtraHighResTileBorderWidth(device_scale_factor);
        } else {
          color = DebugColors::ExtraLowResTileBorderColor();
          width = DebugColors::ExtraLowResTileBorderWidth(device_scale_factor);
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

  if (layer_tree_impl()->debug_state().highlight_non_lcd_text_layers) {
    SkColor4f color =
        DebugColors::NonLCDTextHighlightColor(lcd_text_disallowed_reason());
    if (color != SkColors::kTransparent &&
        GetRasterSource()->GetDisplayItemList()->AreaOfDrawText(
            gfx::Rect(bounds()))) {
      render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>()->SetNew(
          shared_quad_state, debug_border_rect, debug_border_rect, color,
          append_quads_data);
    }
  }

  // Keep track of the tilings that were used so that tilings that are
  // unused can be considered for removal.
  last_append_quads_tilings_.clear();

  // Ignore missing tiles outside of viewport for tile priority. This is
  // normally the same as draw viewport but can be independently overridden by
  // embedders like Android WebView with SetExternalTilePriorityConstraints.
  gfx::Rect scaled_viewport_for_tile_priority = gfx::ScaleToEnclosingRect(
      viewport_rect_for_tile_priority_in_content_space_, max_contents_scale);

  std::optional<gfx::Rect> scaled_cull_rect;
  const ScrollTree& scroll_tree =
      layer_tree_impl()->property_trees()->scroll_tree();
  if (const ScrollNode* scroll_node = scroll_tree.Node(scroll_tree_index())) {
    if (transform_tree_index() == scroll_node->transform_id) {
      if (const gfx::Rect* cull_rect =
              scroll_tree.ScrollingContentsCullRect(scroll_node->element_id)) {
        scaled_cull_rect =
            gfx::ScaleToEnclosingRect(*cull_rect, max_contents_scale);
      }
    }
  }

  int missing_tile_count = 0u;
  only_used_low_res_last_append_quads_ = true;
  gfx::Rect scaled_recorded_bounds = gfx::ScaleToEnclosingRect(
      raster_source_->recorded_bounds(), max_contents_scale);
  for (auto iter =
           tilings_->Cover(shared_quad_state->visible_quad_layer_rect,
                           max_contents_scale, ideal_contents_scale_key());
       iter; ++iter) {
    gfx::Rect geometry_rect = iter.geometry_rect();
    if (!scaled_recorded_bounds.Intersects(geometry_rect)) {
      // This happens when the tiling rect is snapped to be bigger than the
      // recorded bounds, and CoverageIterator returns a "missing" tile
      // to cover some of the empty area. The tile should be ignored, otherwise
      // it would be mistakenly treated as checkerboarded and drawn with the
      // safe background color.
      // TODO(crbug.com/328677988): Ideally we should check intersection with
      // visible_geometry_rect and remove the visible_geometry_rect.IsEmpty()
      // condition below.
      continue;
    }
    gfx::Rect visible_geometry_rect =
        scaled_occlusion.GetUnoccludedContentRect(geometry_rect);

    gfx::Rect offset_geometry_rect = geometry_rect;
    offset_geometry_rect.Offset(quad_offset);
    gfx::Rect offset_visible_geometry_rect = visible_geometry_rect;
    offset_visible_geometry_rect.Offset(quad_offset);

    bool needs_blending = !contents_opaque();
    if (visible_geometry_rect.IsEmpty())
      continue;

    uint64_t visible_geometry_area = visible_geometry_rect.size().Area64();
    append_quads_data->visible_layer_area += visible_geometry_area;

    bool has_draw_quad = false;
    if (*iter && iter->draw_info().IsReadyToDraw()) {
      const TileDrawInfo& draw_info = iter->draw_info();
      // Mark the tile used for raster. This is used to reclaim old prepaint
      // tiles in TileManager.
      iter->mark_used();

      switch (draw_info.mode()) {
        case TileDrawInfo::RESOURCE_MODE: {
          gfx::RectF texture_rect = iter.texture_rect();

          // The raster_contents_scale_ is the best scale that the layer is
          // trying to produce, even though it may not be ideal. Since that's
          // the best the layer can promise in the future, consider those as
          // complete. Also consider a tile complete if it is ideal scale or
          // better. Note that PLTS::CoverageIterator prefers the _smallest_
          // scale that is >= ideal, which may be < raster_contents_scale_.
          if (iter->contents_scale_key() != raster_contents_scale_key() &&
              iter->contents_scale_key() < ideal_contents_scale_key() &&
              geometry_rect.Intersects(scaled_viewport_for_tile_priority)) {
            append_quads_data->num_incompletely_rastered_tiles++;
          }

          auto* quad =
              render_pass->CreateAndAppendDrawQuad<viz::TileDrawQuad>();
          quad->SetNew(
              shared_quad_state, offset_geometry_rect,
              offset_visible_geometry_rect, needs_blending,
              draw_info.resource_id_for_export(), texture_rect,
              draw_info.resource_size(), draw_info.is_premultiplied(),
              nearest_neighbor_,
              !layer_tree_impl()->settings().enable_edge_anti_aliasing);
          ValidateQuadResources(quad);
          has_draw_quad = true;
          break;
        }
        case TileDrawInfo::SOLID_COLOR_MODE: {
          float alpha = draw_info.solid_color().fA * shared_quad_state->opacity;
          if (alpha >= std::numeric_limits<float>::epsilon()) {
            auto* quad =
                render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
            quad->SetNew(
                shared_quad_state, offset_geometry_rect,
                offset_visible_geometry_rect, draw_info.solid_color(),
                !layer_tree_impl()->settings().enable_edge_anti_aliasing);
            ValidateQuadResources(quad);
          }
          has_draw_quad = true;
          break;
        }
        case TileDrawInfo::OOM_MODE:
          break;  // Checkerboard.
      }
    }

    int64_t needs_record_content_area = 0;
    if (scaled_cull_rect) {
      gfx::Rect fully_recorded_rect =
          gfx::IntersectRects(*scaled_cull_rect, visible_geometry_rect);
      if (fully_recorded_rect != visible_geometry_rect) {
        append_quads_data->num_incompletely_recorded_tiles++;
        needs_record_content_area =
            visible_geometry_area - fully_recorded_rect.size().Area64();
        append_quads_data->checkerboarded_needs_record_content_area +=
            needs_record_content_area;
      }
    }

    if (!has_draw_quad) {
      // Checkerboard due to missing raster.
      SkColor4f color = safe_opaque_background_color();
      if (ShowDebugBorders(DebugBorderType::LAYER)) {
        // Fill the whole tile with the missing tile color.
        color = DebugColors::DefaultCheckerboardColor();
      }
      auto* quad =
          render_pass->CreateAndAppendDrawQuad<viz::SolidColorDrawQuad>();
      quad->SetNew(shared_quad_state, offset_geometry_rect,
                   offset_visible_geometry_rect, color, false);
      ValidateQuadResources(quad);

      if (geometry_rect.Intersects(scaled_viewport_for_tile_priority)) {
        append_quads_data->num_missing_tiles++;
        ++missing_tile_count;
      }
      append_quads_data->checkerboarded_visible_content_area +=
          visible_geometry_area;
      // Intersect checkerboard rect with recorded bounds to generate rect where
      // we checkerboarded and has recording. The area where we don't have
      // recording is not necessarily a Rect, and its area is calculated using
      // subtraction.
      gfx::Rect visible_rect_has_recording = visible_geometry_rect;
      visible_rect_has_recording.Intersect(scaled_recorded_bounds);
      append_quads_data->checkerboarded_needs_raster_content_area +=
          visible_rect_has_recording.size().Area64();

      // Report data on any missing images that might be the largest
      // contentful image.
      if (*iter) {
        UMA_HISTOGRAM_BOOLEAN(
            "Compositing.DecodeLCPCandidateImage.MissedDeadline",
            iter->HasMissingLCPCandidateImages());
      }

      continue;
    }

    append_quads_data->checkerboarded_visible_content_area +=
        needs_record_content_area;

    if (iter.resolution() != HIGH_RESOLUTION) {
      append_quads_data->approximated_visible_content_area +=
          visible_geometry_area;
    }

    // If we have a draw quad, but it's not low resolution, then
    // mark that we've used something other than low res to draw.
    if (iter.resolution() != LOW_RESOLUTION)
      only_used_low_res_last_append_quads_ = false;

    if (last_append_quads_tilings_.empty() ||
        last_append_quads_tilings_.back() != iter.CurrentTiling()) {
      last_append_quads_tilings_.push_back(iter.CurrentTiling());
    }
  }

  // Adjust shared_quad_state with the quad_offset, since we've adjusted each
  // quad we've appended by it.
  shared_quad_state->quad_to_target_transform.Translate(-quad_offset);
  shared_quad_state->quad_layer_rect.Offset(quad_offset);
  shared_quad_state->visible_quad_layer_rect.Offset(quad_offset);

  if (missing_tile_count) {
    TRACE_EVENT_INSTANT1("cc", "PictureLayerImpl::AppendQuads checkerboard",
                         TRACE_EVENT_SCOPE_THREAD, "missing_tile_count",
                         missing_tile_count);
  }

  // Aggressively remove any tilings that are not seen to save memory. Note
  // that this is at the expense of doing cause more frequent re-painting. A
  // better scheme would be to maintain a tighter visible_layer_rect for the
  // finer tilings.
  CleanUpTilingsOnActiveLayer(last_append_quads_tilings_);
  SanityCheckTilingState();
}

bool PictureLayerImpl::UpdateTiles() {
  if (!CanHaveTilings()) {
    ideal_page_scale_ = 0.f;
    ideal_device_scale_ = 0.f;
    ideal_contents_scale_ = gfx::Vector2dF(0.f, 0.f);
    ideal_source_scale_ = gfx::Vector2dF(0.f, 0.f);
    SanityCheckTilingState();
    return false;
  }

  // Remove any non-ideal tilings that were not used last time we generated
  // quads to save memory and processing time. Note that pending tree should
  // only have one or two tilings (high and low res), so only clean up the
  // active layer. This cleans it up here in case AppendQuads didn't run.
  // If it did run, this would not remove any additional tilings.
  if (layer_tree_impl()->IsActiveTree())
    CleanUpTilingsOnActiveLayer(last_append_quads_tilings_);

  UpdateIdealScales();

  const bool should_adjust_raster_scale = ShouldAdjustRasterScale();
  if (should_adjust_raster_scale)
    RecalculateRasterScales();
  UpdateTilingsForRasterScaleAndTranslation(should_adjust_raster_scale);
  raster_source_size_changed_ = false;

  if (layer_tree_impl()->IsActiveTree())
    AddLowResolutionTilingIfNeeded();

  DCHECK(raster_page_scale_);
  DCHECK(raster_device_scale_);
  DCHECK(raster_source_scale_.x());
  DCHECK(raster_source_scale_.y());
  DCHECK(raster_contents_scale_.x());
  DCHECK(raster_contents_scale_.y());
  DCHECK(low_res_raster_contents_scale_);

  was_screen_space_transform_animating_ =
      draw_properties().screen_space_transform_is_animating;

  double current_frame_time_in_seconds =
      (layer_tree_impl()->CurrentBeginFrameArgs().frame_time -
       base::TimeTicks()).InSecondsF();
  UpdateViewportRectForTilePriorityInContentSpace();

  // The tiling set can require tiles for activation any of the following
  // conditions are true:
  // - This layer produced a high-res or non-ideal-res tile last frame.
  // - We're in requires high res to draw mode.
  // - We're not in smoothness takes priority mode.
  // To put different, the tiling set can't require tiles for activation if
  // we're in smoothness mode and only used low-res or checkerboard to draw last
  // frame and we don't need high res to draw.
  //
  // The reason for this is that we should be able to activate sooner and get a
  // more up to date recording, so we don't run out of recording on the active
  // tree.
  // A layer must be a drawing layer for it to require tiles for activation.
  bool can_require_tiles_for_activation = false;
  if (contributes_to_drawn_render_surface()) {
    can_require_tiles_for_activation =
        !only_used_low_res_last_append_quads_ || RequiresHighResToDraw() ||
        !layer_tree_impl()->SmoothnessTakesPriority();
  }

  static const base::NoDestructor<Occlusion> kEmptyOcclusion;
  const Occlusion& occlusion_in_content_space =
      layer_tree_impl()->settings().use_occlusion_for_tile_prioritization
          ? draw_properties().occlusion_in_content_space
          : *kEmptyOcclusion;

  // Pass |occlusion_in_content_space| for |occlusion_in_layer_space| since
  // they are the same space in picture layer, as contents scale is always 1.
  bool updated = tilings_->UpdateTilePriorities(
      viewport_rect_for_tile_priority_in_content_space_,
      ideal_contents_scale_key(), current_frame_time_in_seconds,
      occlusion_in_content_space, can_require_tiles_for_activation);
  DCHECK_GT(tilings_->num_tilings(), 0u);
  SanityCheckTilingState();
  return updated;
}

void PictureLayerImpl::UpdateViewportRectForTilePriorityInContentSpace() {
  // If visible_layer_rect() is empty or viewport_rect_for_tile_priority is
  // set to be different from the device viewport, try to inverse project the
  // viewport into layer space and use that. Otherwise just use
  // visible_layer_rect().
  gfx::Rect visible_rect_in_content_space = visible_layer_rect();
  gfx::Rect viewport_rect_for_tile_priority =
      layer_tree_impl()->ViewportRectForTilePriority();
  if (visible_rect_in_content_space.IsEmpty() ||
      layer_tree_impl()->GetDeviceViewport() !=
          viewport_rect_for_tile_priority) {
    gfx::Transform view_to_layer;
    if (ScreenSpaceTransform().GetInverse(&view_to_layer)) {
      // Transform from view space to content space.
      visible_rect_in_content_space = MathUtil::ProjectEnclosingClippedRect(
          view_to_layer, viewport_rect_for_tile_priority);

      // We have to allow for a viewport that is outside of the layer bounds in
      // order to compute tile priorities correctly for offscreen content that
      // is going to make it on screen. However, we also have to limit the
      // viewport since it can be very large due to screen_space_transforms. As
      // a heuristic, we clip to bounds padded by skewport_extrapolation_limit *
      // maximum tiling scale, since this should allow sufficient room for
      // skewport calculations.
      gfx::Rect padded_bounds(bounds());
      int padding_amount = layer_tree_impl()
                               ->settings()
                               .skewport_extrapolation_limit_in_screen_pixels *
                           MaximumTilingContentsScale();
      padded_bounds.Inset(-padding_amount);
      visible_rect_in_content_space =
          SafeIntersectRects(visible_rect_in_content_space, padded_bounds);
    }
  }
  viewport_rect_for_tile_priority_in_content_space_ =
      visible_rect_in_content_space;
}

PictureLayerImpl* PictureLayerImpl::GetPendingOrActiveTwinLayer() const {
  if (!twin_layer_ || !twin_layer_->IsOnActiveOrPendingTree())
    return nullptr;
  return twin_layer_;
}

void PictureLayerImpl::UpdateRasterSource(
    scoped_refptr<RasterSource> raster_source,
    Region* new_invalidation) {
  CHECK(layer_tree_impl()->IsSyncTree());
  UpdateRasterSourceInternal(
      std::move(raster_source), new_invalidation,
      // These pointers being null indicates we are committing.
      nullptr, nullptr, nullptr);
}

void PictureLayerImpl::UpdateRasterSourceInternal(
    scoped_refptr<RasterSource> raster_source,
    Region* new_invalidation,
    const PictureLayerTilingSet* pending_set,
    const PaintWorkletRecordMap* pending_paint_worklet_records,
    const DiscardableImageMap* pending_discardable_image_map) {
  CHECK(raster_source);
  // The layer bounds and the raster source size may differ if the raster source
  // wasn't updated (ie. PictureLayer::Update didn't happen). In that case the
  // raster source should be empty.
  DCHECK(raster_source->size().IsEmpty() || bounds() == raster_source->size())
      << " layer bounds " << bounds().ToString() << " raster_source size "
      << raster_source->size().ToString();

  if (!raster_source_ || raster_source_->size() != raster_source->size()) {
    raster_source_size_changed_ = true;
  }

  // We have an updated recording if the DisplayItemList in the new RasterSource
  // is different.
  const bool recording_updated =
      !raster_source_ || raster_source_->GetDisplayItemList() !=
                             raster_source->GetDisplayItemList();

    // If the MSAA sample count has changed, we need to re-raster the complete
    // layer.
  if (recording_updated && raster_source_) {
    const auto& current_display_item_list =
        raster_source_->GetDisplayItemList();
    const auto& new_display_item_list = raster_source->GetDisplayItemList();
    if (current_display_item_list && new_display_item_list) {
      bool needs_full_invalidation =
          layer_tree_impl()->GetMSAASampleCountForRaster(
              *current_display_item_list) !=
          layer_tree_impl()->GetMSAASampleCountForRaster(
              *new_display_item_list);
      needs_full_invalidation |=
          layer_tree_impl()->GetTargetColorParams(
              current_display_item_list->content_color_usage()) !=
          layer_tree_impl()->GetTargetColorParams(
              new_display_item_list->content_color_usage());
      if (needs_full_invalidation) {
        new_invalidation->Union(gfx::Rect(raster_source->size()));
      }
    }
  }

  // The |raster_source_| is initially null, so have to check for that for the
  // first frame.
  bool could_have_tilings = CanHaveTilings();
  raster_source_.swap(raster_source);

  raster_source_->set_debug_name(DebugName());

  UpdateDirectlyCompositedImageFromRasterSource();

  if (pending_set) {
    // During activation, check if we need to pull the discardable image map
    // from the pending tree.
    if (pending_discardable_image_map != discardable_image_map_) {
      CHECK(pending_paint_worklet_records);
      paint_worklet_records_ = *pending_paint_worklet_records;
      UnregisterAnimatedImages();
      discardable_image_map_ = pending_discardable_image_map;
      RegisterAnimatedImages();
    }
  } else if (recording_updated) {
    needs_regenerate_discardable_image_map_ = true;
  }

  // The |new_invalidation| must be cleared before updating tilings since they
  // access the invalidation through the PictureLayerTilingClient interface.
  invalidation_.Clear();
  invalidation_.Swap(new_invalidation);

  bool can_have_tilings = CanHaveTilings();
  DCHECK(!pending_set ||
         can_have_tilings == GetPendingOrActiveTwinLayer()->CanHaveTilings());

  // Need to call UpdateTiles again if CanHaveTilings changed.
  if (could_have_tilings != can_have_tilings)
    layer_tree_impl()->set_needs_update_draw_properties();

  if (!can_have_tilings) {
    RemoveAllTilings();
    return;
  }

  // We could do this after doing UpdateTiles, which would avoid doing this for
  // tilings that are going to disappear on the pending tree (if scale changed).
  // But that would also be more complicated, so we just do it here for now.
  //
  // TODO(crbug.com/41389434): If the LayerTreeFrameSink is lost, and we
  // activate, this ends up running with the old LayerTreeFrameSink, or possibly
  // with a null LayerTreeFrameSink, which can give incorrect results or maybe
  // crash.
  if (pending_set) {
    tilings_->UpdateTilingsToCurrentRasterSourceForActivation(
        raster_source_, pending_set, invalidation_, MinimumContentsScale(),
        MaximumContentsScale());
  } else {
    tilings_->UpdateTilingsToCurrentRasterSourceForCommit(
        raster_source_, invalidation_, MinimumContentsScale(),
        MaximumContentsScale());
  }
}

void PictureLayerImpl::RegenerateDiscardableImageMapIfNeeded() {
  CHECK(layer_tree_impl()->IsSyncTree());
  if (!needs_regenerate_discardable_image_map_) {
    return;
  }
  needs_regenerate_discardable_image_map_ = false;

  UnregisterAnimatedImages();
  if (const auto* display_list = raster_source_->GetDisplayItemList().get()) {
    scoped_refptr<DiscardableImageMap> image_map =
        display_list->GenerateDiscardableImageMap(
            GetRasterInducingScrollOffsets());
    SetPaintWorkletInputs(image_map->paint_worklet_inputs());
    layer_tree_impl()->UpdateImageDecodingHints(
        image_map->TakeDecodingModeMap());
    discardable_image_map_ = std::move(image_map);
  } else {
    SetPaintWorkletInputs({});
    discardable_image_map_ = nullptr;
  }
  RegisterAnimatedImages();
}

void PictureLayerImpl::UpdateCanUseLCDText(
    bool raster_translation_aligns_pixels) {
  // If we have pending/active trees, the active tree doesn't update lcd text
  // status but copies it from the pending tree.
  if (!layer_tree_impl()->IsSyncTree())
    return;

  lcd_text_disallowed_reason_ =
      ComputeLCDTextDisallowedReason(raster_translation_aligns_pixels);
}

bool PictureLayerImpl::AffectedByWillChangeTransformHint() const {
  TransformNode* transform_node =
      GetTransformTree().Node(transform_tree_index());
  return transform_node &&
         transform_node->node_or_ancestors_will_change_transform;
}

LCDTextDisallowedReason PictureLayerImpl::ComputeLCDTextDisallowedReason(
    bool raster_translation_aligns_pixels) const {
  // No need to use LCD text if there is no text.
  if (!raster_source_ || !raster_source_->GetDisplayItemList() ||
      !raster_source_->GetDisplayItemList()->has_draw_text_ops()) {
    return LCDTextDisallowedReason::kNoText;
  }

  if (layer_tree_impl()->settings().layers_always_allowed_lcd_text) {
    return LCDTextDisallowedReason::kNone;
  }
  if (!layer_tree_impl()->settings().can_use_lcd_text) {
    return LCDTextDisallowedReason::kSetting;
  }

  TransformNode* transform_node =
      GetTransformTree().Node(transform_tree_index());
  if (transform_node->node_or_ancestors_will_change_transform) {
    return LCDTextDisallowedReason::kWillChangeTransform;
  }

  if (screen_space_transform_is_animating()) {
    return LCDTextDisallowedReason::kTransformAnimation;
  }

  EffectNode* effect_node = GetEffectTree().Node(effect_tree_index());
  if (effect_node->node_or_ancestor_has_filters ||
      effect_node->affected_by_backdrop_filter) {
    return LCDTextDisallowedReason::kPixelOrColorEffect;
  }

  // If raster translation aligns pixels, we can ignore fractional layer offset
  // and transform for LCD text.
  if (!raster_translation_aligns_pixels) {
    if (static_cast<int>(offset_to_transform_parent().x()) !=
        offset_to_transform_parent().x()) {
      return LCDTextDisallowedReason::kNonIntegralXOffset;
    }
    if (static_cast<int>(offset_to_transform_parent().y()) !=
        offset_to_transform_parent().y()) {
      return LCDTextDisallowedReason::kNonIntegralYOffset;
    }
    return LCDTextDisallowedReason::kNonIntegralTranslation;
  }

  if (!contents_opaque_for_text()) {
    if (!background_color().isOpaque()) {
      return LCDTextDisallowedReason::kBackgroundColorNotOpaque;
    }
    return LCDTextDisallowedReason::kContentsNotOpaque;
  }
  return LCDTextDisallowedReason::kNone;
}

LCDTextDisallowedReason
PictureLayerImpl::ComputeLCDTextDisallowedReasonForTesting() const {
  gfx::Vector2dF raster_translation;
  return ComputeLCDTextDisallowedReason(
      CalculateRasterTranslation(raster_translation));
}

void PictureLayerImpl::NotifyTileStateChanged(const Tile* tile) {
  if (layer_tree_impl()->IsActiveTree())
    damage_rect_.Union(tile->enclosing_layer_rect());
  if (tile->draw_info().NeedsRaster()) {
    PictureLayerTiling* tiling =
        tilings_->FindTilingWithScaleKey(tile->contents_scale_key());
    if (tiling) {
      tiling->set_all_tiles_done(false);
      tilings_->set_all_tiles_done(false);
    }
  }

  if (layer_tree_impl()->settings().UseLayerContextForDisplay() &&
      !IsActive()) {
    // Accumulate tile changes on the pending tree only. These are pushed to the
    // active tree in PushPropertiesTo().
    updated_tiles_[tile->contents_scale_key()].emplace(tile->tiling_i_index(),
                                                       tile->tiling_j_index());
  }
}

gfx::Rect PictureLayerImpl::GetDamageRect() const {
  return damage_rect_;
}

void PictureLayerImpl::ResetChangeTracking() {
  LayerImpl::ResetChangeTracking();
  damage_rect_.SetRect(0, 0, 0, 0);
  has_animated_image_update_rect_ = false;
  has_non_animated_image_update_rect_ = false;
}

void PictureLayerImpl::DidBeginTracing() {
  raster_source_->DidBeginTracing();
}

void PictureLayerImpl::ReleaseResources() {
  tilings_->ReleaseAllResources();
  ResetRasterScale();
}

void PictureLayerImpl::ReleaseTileResources() {
  // All resources are tile resources.
  ReleaseResources();
}

void PictureLayerImpl::RecreateTileResources() {
  // Recreate tilings with new settings, since some of those might change when
  // we release resources.
  tilings_ = CreatePictureLayerTilingSet();
}

Region PictureLayerImpl::GetInvalidationRegionForDebugging() {
  // |invalidation_| gives the invalidation contained in the source frame, but
  // is not cleared after drawing from the layer. However, update_rect() is
  // cleared once the invalidation is drawn, which is useful for debugging
  // visualizations. This method intersects the two to give a more exact
  // representation of what was invalidated that is cleared after drawing.
  return IntersectRegions(invalidation_, update_rect());
}

std::unique_ptr<Tile> PictureLayerImpl::CreateTile(
    const Tile::CreateInfo& info) {
  SetNeedsPushProperties();
  tilings_->set_all_tiles_done(false);

  int flags = 0;

  // We don't handle solid color single texture masks for backdrop filters,
  // so we shouldn't bother analyzing those.
  // Otherwise, always analyze to maximize memory savings.
  if (!is_backdrop_filter_mask_)
    flags = Tile::USE_PICTURE_ANALYSIS;

  if (contents_opaque())
    flags |= Tile::IS_OPAQUE;

  return layer_tree_impl()->tile_manager()->CreateTile(
      info, id(), layer_tree_impl()->source_frame_number(), flags);
}

const Region* PictureLayerImpl::GetPendingInvalidation() {
  if (layer_tree_impl()->IsPendingTree())
    return &invalidation_;
  if (layer_tree_impl()->IsRecycleTree())
    return nullptr;
  DCHECK(layer_tree_impl()->IsActiveTree());
  if (PictureLayerImpl* twin_layer = GetPendingOrActiveTwinLayer())
    return &twin_layer->invalidation_;
  return nullptr;
}

const PictureLayerTiling* PictureLayerImpl::GetPendingOrActiveTwinTiling(
    const PictureLayerTiling* tiling) const {
  PictureLayerImpl* twin_layer = GetPendingOrActiveTwinLayer();
  if (!twin_layer)
    return nullptr;
  const PictureLayerTiling* twin_tiling =
      twin_layer->tilings_->FindTilingWithScaleKey(
          tiling->contents_scale_key());
  if (twin_tiling &&
      twin_tiling->raster_transform() == tiling->raster_transform())
    return twin_tiling;
  return nullptr;
}

bool PictureLayerImpl::RequiresHighResToDraw() const {
  return layer_tree_impl()->RequiresHighResToDraw();
}

const PaintWorkletRecordMap& PictureLayerImpl::GetPaintWorkletRecords() const {
  return paint_worklet_records_;
}

bool PictureLayerImpl::IsDirectlyCompositedImage() const {
  return directly_composited_image_default_raster_scale_ > 0.f;
}

std::vector<const DrawImage*> PictureLayerImpl::GetDiscardableImagesInRect(
    const gfx::Rect& rect) const {
  return discardable_image_map_->GetDiscardableImagesInRect(rect);
}

ScrollOffsetMap PictureLayerImpl::GetRasterInducingScrollOffsets() const {
  ScrollOffsetMap map;
  if (raster_source_) {
    const ScrollTree& scroll_tree =
        layer_tree_impl()->property_trees()->scroll_tree();
    for (auto [element_id, _] :
         raster_source_->GetDisplayItemList()->raster_inducing_scrolls()) {
      map[element_id] = scroll_tree.current_scroll_offset(element_id);
    }
  }
  return map;
}

const GlobalStateThatImpactsTilePriority& PictureLayerImpl::global_tile_state()
    const {
  return layer_tree_impl()->global_tile_state();
}

gfx::Rect PictureLayerImpl::GetEnclosingVisibleRectInTargetSpace() const {
  return GetScaledEnclosingVisibleRectInTargetSpace(
      MaximumTilingContentsScale());
}

bool PictureLayerImpl::ShouldAnimate(PaintImage::Id paint_image_id) const {
  // If we are registered with the animation controller, which queries whether
  // the image should be animated, then we must have recordings with this image.
  CHECK(discardable_image_map_);
  CHECK(!discardable_image_map_->empty());

  // Only animate images for layers which HasValidTilePriorities. This check is
  // important for 2 reasons:
  // 1) It avoids doing additional work for layers we don't plan to rasterize
  //    and/or draw. The updated state will be pulled by the animation system
  //    if the draw properties change.
  // 2) It eliminates considering layers on the recycle tree. Once the pending
  //    tree is activated, the layers on the recycle tree remain registered as
  //    animation drivers, but should not drive animations since they don't have
  //    updated draw properties.
  //
  //  Additionally only animate images which are on-screen, animations are
  //  paused once they are not visible.
  if (!HasValidTilePriorities())
    return false;

  const auto& rects = discardable_image_map_->GetRectsForImage(paint_image_id);
  for (const auto& r : rects) {
    if (r.Intersects(visible_layer_rect()))
      return true;
  }
  return false;
}

gfx::Size PictureLayerImpl::CalculateTileSize(const gfx::Size& content_bounds) {
  return tile_size_calculator_.CalculateTileSize(content_bounds);
}

void PictureLayerImpl::GetContentsResourceId(
    viz::ResourceId* resource_id,
    gfx::Size* resource_size,
    gfx::SizeF* resource_uv_size) const {
  // We need contents resource for backdrop filter masks only.
  if (!is_backdrop_filter_mask()) {
    *resource_id = viz::kInvalidResourceId;
    return;
  }

  float dest_scale = MaximumTilingContentsScale();
  gfx::Rect content_rect =
      gfx::ScaleToEnclosingRect(gfx::Rect(bounds()), dest_scale);
  auto iter =
      tilings_->Cover(content_rect, dest_scale, ideal_contents_scale_key());

  // Mask resource not ready yet.
  if (!iter || !*iter) {
    *resource_id = viz::kInvalidResourceId;
    return;
  }

  // Masks only supported if they fit on exactly one tile.
  DCHECK(iter.geometry_rect() == content_rect)
      << "iter rect " << iter.geometry_rect().ToString() << " content rect "
      << content_rect.ToString();

  const TileDrawInfo& draw_info = iter->draw_info();
  if (!draw_info.IsReadyToDraw() ||
      draw_info.mode() != TileDrawInfo::RESOURCE_MODE) {
    *resource_id = viz::kInvalidResourceId;
    return;
  }

  *resource_id = draw_info.resource_id_for_export();
  *resource_size = draw_info.resource_size();
  // |resource_uv_size| represents the range of UV coordinates that map to the
  // content being drawn. Typically, we draw to the entire texture, so these
  // coordinates are (1.0f, 1.0f). However, if we are rasterizing to an
  // over-large texture, this size will be smaller, mapping to the subset of the
  // texture being used.
  gfx::SizeF requested_tile_size =
      gfx::SizeF(iter->tiling()->tiling_data()->tiling_rect().size());
  DCHECK_LE(requested_tile_size.width(), draw_info.resource_size().width());
  DCHECK_LE(requested_tile_size.height(), draw_info.resource_size().height());
  *resource_uv_size = gfx::SizeF(
      requested_tile_size.width() / draw_info.resource_size().width(),
      requested_tile_size.height() / draw_info.resource_size().height());
}

void PictureLayerImpl::UpdateDirectlyCompositedImageFromRasterSource() {
  float new_default_raster_scale = 0;
  bool new_nearest_neighbor = false;
  if (const auto& info = raster_source_->directly_composited_image_info()) {
    // TODO(crbug.com/40176440): Support 2D scales in directly composited
    // images.
    new_default_raster_scale =
        GetPreferredRasterScale(info->default_raster_scale);
    new_nearest_neighbor = info->nearest_neighbor;
  }

  directly_composited_image_default_raster_scale_changed_ =
      new_default_raster_scale !=
      directly_composited_image_default_raster_scale_;

  if (new_nearest_neighbor != nearest_neighbor_ ||
      directly_composited_image_default_raster_scale_changed_) {
    directly_composited_image_default_raster_scale_ = new_default_raster_scale;
    nearest_neighbor_ = new_nearest_neighbor;
    NoteLayerPropertyChanged();
  }
}

bool PictureLayerImpl::ShouldDirectlyCompositeImage(float raster_scale) const {
  // Even if there are minor rendering differences, we want to apply directly
  // compositing images in cases where doing so is going to save memory.
  if (raster_scale < 0.1f)
    return true;

  // If the results of scaling the bounds by the expected raster scale
  // would end up with a content rect whose width/height are more than one
  // pixel different from the layer bounds, don't directly composite the image
  // to avoid incorrect rendering.
  gfx::SizeF layer_bounds(bounds());
  gfx::RectF scaled_bounds_rect(layer_bounds);
  scaled_bounds_rect.Scale(raster_scale);

  // Take the scaled bounds, get the enclosing rect then scale it back down -
  // this is the same set of operations that will happen when using the tiling
  // at that raster scale.
  gfx::RectF content_rect(gfx::ToEnclosingRect(scaled_bounds_rect));
  content_rect.InvScale(raster_scale);

  return std::abs(layer_bounds.width() - content_rect.width()) < 1.f &&
         std::abs(layer_bounds.height() - content_rect.height()) < 1.f;
}

float PictureLayerImpl::CalculateDirectlyCompositedImageRasterScale() const {
  DCHECK(IsDirectlyCompositedImage());
  // If the default raster scale didn't change, we will calculate based on the
  // previous raster source scale. The calculation may change based on updated
  // ideal source scale.
  float adjusted_raster_scale =
      directly_composited_image_default_raster_scale_changed_
          ? directly_composited_image_default_raster_scale_
          : raster_source_scale_key();

  // We never want a raster scale larger than the default, since that uses more
  // memory but can't result it better quality (upscaling will happen in the
  // display compositor instead).
  float max_scale = std::max(directly_composited_image_default_raster_scale_,
                             MinimumContentsScale());
  float min_scale = MinimumContentsScale();

  float clamped_ideal_source_scale =
      std::clamp(ideal_source_scale_key(), min_scale, max_scale);
  // Use clamped_ideal_source_scale if adjusted_raster_scale is too far away.
  constexpr float kFarAwayFactor = 32.f;
  if (adjusted_raster_scale < clamped_ideal_source_scale / kFarAwayFactor) {
    adjusted_raster_scale = clamped_ideal_source_scale;
  } else if (adjusted_raster_scale >
             clamped_ideal_source_scale * kFarAwayFactor) {
    adjusted_raster_scale = clamped_ideal_source_scale;
  } else {
    while (adjusted_raster_scale < clamped_ideal_source_scale)
      adjusted_raster_scale *= 2.f;

    // Make sure the adjusted scale is not more than 2x away from the ideal
    // scale in order to save memory. Note that ShouldAdjustRasterScale() uses
    // factor 4 to determine when the scale needs to be updated. This means that
    // the layer may need to be re-rasterized if scale is increased by factor
    // of 2, but not again when it's scaled back to the original size.
    while (adjusted_raster_scale >= 2 * clamped_ideal_source_scale)
      adjusted_raster_scale /= 2.f;
  }

  adjusted_raster_scale =
      std::clamp(adjusted_raster_scale, min_scale, max_scale);
  return adjusted_raster_scale;
}

PictureLayerTiling* PictureLayerImpl::AddTiling(
    const gfx::AxisTransform2d& raster_transform) {
  DCHECK(CanHaveTilings());
  DCHECK_GE(raster_transform.scale().x(), MinimumContentsScale());
  DCHECK_GE(raster_transform.scale().y(), MinimumContentsScale());
  DCHECK_LE(raster_transform.scale().x(), MaximumContentsScale());
  DCHECK_LE(raster_transform.scale().y(), MaximumContentsScale());
  DCHECK(raster_source_->HasRecordings());
  bool tiling_can_use_lcd_text =
      can_use_lcd_text() && raster_transform.scale() == raster_contents_scale_;
  return tilings_->AddTiling(raster_transform, raster_source_,
                             tiling_can_use_lcd_text);
}

void PictureLayerImpl::RemoveAllTilings() {
  tilings_->RemoveAllTilings();
  // If there are no tilings, then raster scales are no longer meaningful.
  ResetRasterScale();
}

bool PictureLayerImpl::CanRecreateHighResTilingForLCDTextAndRasterTransform(
    const PictureLayerTiling& high_res) const {
  // Prefer re-rasterization for a change in LCD status from the following
  // reasons since visual artifacts of LCD text on non-opaque background are
  // very noticeable. This state also only changes during a commit and is likely
  // to be discrete as opposed to every frame of the animation.
  if (high_res.can_use_lcd_text() &&
      (lcd_text_disallowed_reason_ ==
           LCDTextDisallowedReason::kBackgroundColorNotOpaque ||
       lcd_text_disallowed_reason_ ==
           LCDTextDisallowedReason::kContentsNotOpaque)) {
    // LCD text state changes require a commit and the existing tiling is
    // invalidated before scheduling rasterization work for the new pending
    // tree. So it shouldn't be possible for the new pending tree to be ready to
    // activate before we have invalidated the existing high rest tiling. This
    // is important to avoid activating a tree with missing tiles which can
    // cause flickering.
    DCHECK(!layer_tree_impl()->IsSyncTree() ||
           !layer_tree_impl()->IsReadyToActivate());
    return true;
  }
  // We can recreate the tiling if we would invalidate all of its tiles.
  if (high_res.may_contain_low_resolution_tiles())
    return true;
  // Keep the non-ideal raster translation unchanged for transform animations
  // to avoid re-rasterization during animation.
  if (draw_properties().screen_space_transform_is_animating ||
      AffectedByWillChangeTransformHint())
    return false;
  // Also avoid re-rasterization during pinch-zoom.
  if (layer_tree_impl()->PinchGestureActive())
    return false;
  // Keep the current LCD text and raster translation if there is no text and
  // the raster scale is ideal.
  if (lcd_text_disallowed_reason_ == LCDTextDisallowedReason::kNoText &&
      high_res.raster_transform().scale() == raster_contents_scale_)
    return false;
  // If ReadyToActivate() is already scheduled, recreating tiling should be
  // delayed until the activation is executed. Otherwise the tiles in viewport
  // will be deleted.
  if (layer_tree_impl()->IsSyncTree() && layer_tree_impl()->IsReadyToActivate())
    return false;
  // To reduce memory usage, don't recreate highres tiling during scroll
  if (layer_tree_impl()->GetActivelyScrollingType() !=
      ActivelyScrollingType::kNone) {
    return false;
  }

  return true;
}

void PictureLayerImpl::UpdateTilingsForRasterScaleAndTranslation(
    bool has_adjusted_raster_scale) {
  PictureLayerTiling* high_res =
      tilings_->FindTilingWithScaleKey(raster_contents_scale_key());

  gfx::Vector2dF raster_translation;
  bool raster_translation_aligns_pixels =
      CalculateRasterTranslation(raster_translation);
  UpdateCanUseLCDText(raster_translation_aligns_pixels);
  if (high_res) {
    bool raster_transform_is_not_ideal =
        high_res->raster_transform().scale() != raster_contents_scale_ ||
        high_res->raster_transform().translation() != raster_translation;
    bool can_use_lcd_text_changed =
        high_res->can_use_lcd_text() != can_use_lcd_text();
    bool can_recreate_highres_tiling =
        CanRecreateHighResTilingForLCDTextAndRasterTransform(*high_res);
    // Only for the sync tree to avoid flickering.
    bool should_recreate_high_res =
        (raster_transform_is_not_ideal || can_use_lcd_text_changed) &&
        layer_tree_impl()->IsSyncTree() && can_recreate_highres_tiling;
    // Only request an invalidation if we don't already have a pending tree.
    bool can_request_invalidation_for_high_res =
        (raster_transform_is_not_ideal || can_use_lcd_text_changed) &&
        !layer_tree_impl()->settings().commit_to_active_tree &&
        layer_tree_impl()->IsActiveTree() && can_recreate_highres_tiling &&
        !layer_tree_impl()->HasPendingTree();

    if (should_recreate_high_res) {
      tilings_->Remove(high_res);
      high_res = nullptr;
    } else if (can_request_invalidation_for_high_res) {
      // Anytime a condition which flips whether we can recreate the tiling
      // changes, we'll get a call to UpdateDrawProperties. We check whether we
      // could recreate the tiling when this runs on the active tree to trigger
      // an impl-side invalidation (if needed).
      layer_tree_impl()->RequestImplSideInvalidationForRerasterTiling();
    } else if (!has_adjusted_raster_scale) {
      // Nothing changed, no need to update tilings.
      DCHECK_EQ(HIGH_RESOLUTION, high_res->resolution());
      SanityCheckTilingState();
      return;
    }
  }

  // Reset all resolution enums on tilings, we'll be setting new values in this
  // function.
  tilings_->MarkAllTilingsNonIdeal();

  if (!high_res) {
    // We always need a high res tiling, so create one if it doesn't exist.
    high_res = AddTiling(gfx::AxisTransform2d::FromScaleAndTranslation(
        raster_contents_scale_, raster_translation));
  } else if (high_res->may_contain_low_resolution_tiles()) {
    // If the tiling we find here was LOW_RESOLUTION previously, it may not be
    // fully rastered, so destroy the old tiles.
    high_res->Reset();
    // Reset the flag now that we'll make it high res, it will have fully
    // rastered content.
    high_res->reset_may_contain_low_resolution_tiles();
  }
  high_res->set_resolution(HIGH_RESOLUTION);

  if (layer_tree_impl()->IsPendingTree() ||
      (layer_tree_impl()->settings().commit_to_active_tree &&
       IsDirectlyCompositedImage())) {
    // On the pending tree, drop any tilings that are non-ideal since we don't
    // need them to activate anyway.

    // For DirectlyCompositedImages, if we recomputed a new raster scale, we
    // should drop the non-ideal ones if we're committing to the active tree.
    // Otherwise a non-ideal scale that is _larger_ than the HIGH_RESOLUTION
    // tile will be used as the coverage scale, and we'll produce a slightly
    // different rendering. We don't drop the tilings on the active tree if
    // we're not committing to the active tree to prevent checkerboarding.
    tilings_->RemoveNonIdealTilings();
  }

  SanityCheckTilingState();
}

bool PictureLayerImpl::ShouldAdjustRasterScale() const {
  if (!raster_contents_scale_.x() || !raster_contents_scale_.y())
    return true;

  // Adjust raster scale if the raster source size changed. This is mainly to
  // reset the preserved scale for will-change:transform but may also help in
  // other cases, which won't affect performance much because the change has
  // involved the main thread and/or we'll (at least partly) re-raster anyway.
  if (raster_source_size_changed_)
    return true;

  if (IsDirectlyCompositedImage()) {
    // If the default raster scale changed, that means the bounds or image size
    // changed. We should recalculate in order to raster at the intrinsic image
    // size. Note that this is not a comparison of the used raster_source_scale_
    // and desired because of the adjustments in RecalculateRasterScales.
    if (directly_composited_image_default_raster_scale_changed_)
      return true;

    // First check to see if we need to adjust based on ideal_source_scale_
    // changing (i.e. scale transform has been modified). These limits exist
    // so that we don't raster at the intrinsic image size if the layer will
    // be scaled down more than 4x ideal. This saves memory without sacrificing
    // noticeable quality. We'll also bump the scale back up in the case where
    // the ideal scale is increased.
    float max_scale = std::max(directly_composited_image_default_raster_scale_,
                               MinimumContentsScale());
    if (raster_source_scale_key() <
        std::min(ideal_source_scale_key(), max_scale))
      return true;
    if (raster_source_scale_key() > 4 * ideal_source_scale_key())
      return true;

    return false;
  }

  if (was_screen_space_transform_animating_ !=
      draw_properties().screen_space_transform_is_animating) {
    if (draw_properties().screen_space_transform_is_animating) {
      // Entering animation.
      // Skip adjusting raster scale if max animation scale already matches
      // raster scale.
      float maximum_animation_scale =
          layer_tree_impl()->property_trees()->MaximumAnimationToScreenScale(
              transform_tree_index());
      if ((maximum_animation_scale != raster_contents_scale_.x() ||
           maximum_animation_scale != raster_contents_scale_.y())) {
        return true;
      }
    } else {
      // Exiting animation.
      // Skip adjusting raster scale when animations finish if we have a
      // will-change: transform hint to preserve maximum resolution tiles
      // needed.
      if (!AffectedByWillChangeTransformHint())
        return true;
    }
  }

  bool is_pinching = layer_tree_impl()->PinchGestureActive();
  if (is_pinching && raster_page_scale_) {
    // We change our raster scale when it is:
    // - Higher than ideal (need a lower-res tiling available)
    // - Too far from ideal (need a higher-res tiling available)
    float ratio = ideal_page_scale_ / raster_page_scale_;
    if (raster_page_scale_ > ideal_page_scale_ ||
        ratio > kMaxScaleRatioDuringPinch)
      return true;
  }

  if (!is_pinching) {
    // When not pinching, match the ideal page scale factor.
    if (raster_page_scale_ != ideal_page_scale_)
      return true;
  }

  // Always match the ideal device scale factor.
  if (raster_device_scale_ != ideal_device_scale_)
    return true;

  float max_scale = MaximumContentsScale();
  if (raster_contents_scale_.x() > max_scale ||
      raster_contents_scale_.y() > max_scale)
    return true;
  float min_scale = MinimumContentsScale();
  if (raster_contents_scale_.x() < min_scale ||
      raster_contents_scale_.y() < min_scale)
    return true;

  // Avoid frequent raster scale changes if we have an animating transform.
  if (draw_properties().screen_space_transform_is_animating) {
    // Except when the device viewport rect has changed because the raster scale
    // may depend on the rect.
    if (layer_tree_impl()->device_viewport_rect_changed()) {
      return true;
    }
    // Or when the raster scale is not affected by invalid scale and is too
    // small compared to the ideal scale.
    if (ideal_contents_scale_.x() >
            raster_contents_scale_.x() *
                kRatioToAdjustRasterScaleForTransformAnimation ||
        ideal_contents_scale_.y() >
            raster_contents_scale_.y() *
                kRatioToAdjustRasterScaleForTransformAnimation) {
      auto* property_trees = layer_tree_impl()->property_trees();
      int transform_id = transform_tree_index();
      if (property_trees->AnimationScaleCacheIsInvalid(transform_id) ||
          !property_trees->AnimationAffectedByInvalidScale(transform_id)) {
        return true;
      }
    }
    return false;
  }

  // Don't change the raster scale if the raster scale is already ideal.
  if (raster_source_scale_ == ideal_source_scale_)
    return false;

  // Don't update will-change: transform layers if the raster contents scale is
  // bigger than the minimum scale.
  if (AffectedByWillChangeTransformHint()) {
    float min_raster_scale = MinimumRasterContentsScaleForWillChangeTransform();
    if (raster_contents_scale_.x() >= min_raster_scale &&
        raster_contents_scale_.y() >= min_raster_scale)
      return false;
  }

  // Match the raster scale in all other cases.
  return true;
}

void PictureLayerImpl::AddLowResolutionTilingIfNeeded() {
  DCHECK(layer_tree_impl()->IsActiveTree());

  if (!layer_tree_impl()->create_low_res_tiling())
    return;

  // We should have a high resolution tiling at raster_contents_scale, so if the
  // low res one is the same then we shouldn't try to override this tiling by
  // marking it as a low res.
  if (raster_contents_scale_key() == low_res_raster_contents_scale_)
    return;

  PictureLayerTiling* low_res =
      tilings_->FindTilingWithScaleKey(low_res_raster_contents_scale_);
  DCHECK(!low_res || low_res->resolution() != HIGH_RESOLUTION);

  // Only create new low res tilings when the transform is static.  This
  // prevents wastefully creating a paired low res tiling for every new high
  // res tiling during a pinch or a CSS animation.
  bool is_pinching = layer_tree_impl()->PinchGestureActive();
  bool is_animating = draw_properties().screen_space_transform_is_animating;
  if (!is_pinching && !is_animating) {
    if (!low_res)
      low_res = AddTiling(gfx::AxisTransform2d(low_res_raster_contents_scale_,
                                               gfx::Vector2dF()));
    low_res->set_resolution(LOW_RESOLUTION);
  }
}

void PictureLayerImpl::RecalculateRasterScales() {
  if (IsDirectlyCompositedImage()) {
    // TODO(crbug.com/40176440): Support 2D scales in directly composited
    // images.
    float used_raster_scale = CalculateDirectlyCompositedImageRasterScale();
    directly_composited_image_default_raster_scale_changed_ = false;
    if (ShouldDirectlyCompositeImage(used_raster_scale)) {
      raster_source_scale_ =
          gfx::Vector2dF(used_raster_scale, used_raster_scale);
      raster_page_scale_ = 1.f;
      raster_device_scale_ = 1.f;
      raster_contents_scale_ = raster_source_scale_;
      low_res_raster_contents_scale_ = used_raster_scale;
      return;
    }

    // If we should not directly composite this image, reset values and fall
    // back to normal raster scale calculations below.
    directly_composited_image_default_raster_scale_ = 0.f;
  }

  gfx::Vector2dF old_raster_contents_scale = raster_contents_scale_;
  float old_raster_page_scale = raster_page_scale_;

  // The raster scale if previous tilings should be preserved.
  gfx::Vector2dF preserved_raster_contents_scale = old_raster_contents_scale;

  raster_device_scale_ = ideal_device_scale_;
  raster_page_scale_ = ideal_page_scale_;
  raster_source_scale_ = ideal_source_scale_;
  raster_contents_scale_ = ideal_contents_scale_;

  // During pinch we completely ignore the current ideal scale, and just use
  // a multiple of the previous scale.
  bool is_pinching = layer_tree_impl()->PinchGestureActive();
  if (is_pinching && !old_raster_contents_scale.IsZero()) {
    // See ShouldAdjustRasterScale:
    // - When zooming out, preemptively create new tiling at lower resolution.
    // - When zooming in, approximate ideal using multiple of kMaxScaleRatio.
    bool zooming_out = old_raster_page_scale > ideal_page_scale_;
    float desired_contents_scale =
        std::max(old_raster_contents_scale.x(), old_raster_contents_scale.y());
    float ideal_scale = ideal_contents_scale_key();
    if (zooming_out) {
      while (desired_contents_scale > ideal_scale)
        desired_contents_scale /= kMaxScaleRatioDuringPinch;
    } else {
      while (desired_contents_scale < ideal_scale)
        desired_contents_scale *= kMaxScaleRatioDuringPinch;
    }
    if (const auto* snapped_to_tiling = tilings_->FindTilingWithNearestScaleKey(
            desired_contents_scale, kSnapToExistingTilingRatio)) {
      raster_contents_scale_ = snapped_to_tiling->raster_transform().scale();
    } else {
      raster_contents_scale_ = old_raster_contents_scale;
      raster_contents_scale_.Scale(desired_contents_scale /
                                   raster_contents_scale_key());
    }
    preserved_raster_contents_scale = raster_contents_scale_;
    raster_page_scale_ =
        std::max(raster_contents_scale_.x() / raster_source_scale_.x(),
                 raster_contents_scale_.y() / raster_source_scale_.y()) /
        raster_device_scale_;
  }

  if (draw_properties().screen_space_transform_is_animating)
    AdjustRasterScaleForTransformAnimation(preserved_raster_contents_scale);

  if (AffectedByWillChangeTransformHint()) {
    float min_scale = MinimumRasterContentsScaleForWillChangeTransform();
    raster_contents_scale_.SetToMax(gfx::Vector2dF(min_scale, min_scale));
  }

  float min_scale = MinimumContentsScale();
  float max_scale = MaximumContentsScale();
  raster_contents_scale_.SetToMax(gfx::Vector2dF(min_scale, min_scale));
  raster_contents_scale_.SetToMin(gfx::Vector2dF(max_scale, max_scale));
  DCHECK_GE(raster_contents_scale_.x(), min_scale);
  DCHECK_GE(raster_contents_scale_.y(), min_scale);
  DCHECK_LE(raster_contents_scale_.x(), max_scale);
  DCHECK_LE(raster_contents_scale_.y(), max_scale);

  // If this layer would create zero or one tiles at this content scale,
  // don't create a low res tiling.
  gfx::Size raster_bounds = gfx::ScaleToCeiledSize(
      raster_source_->recorded_bounds().size(), raster_contents_scale_.x(),
      raster_contents_scale_.y());
  gfx::Size tile_size = CalculateTileSize(raster_bounds);
  bool tile_covers_bounds = tile_size.width() >= raster_bounds.width() &&
                            tile_size.height() >= raster_bounds.height();
  if (tile_size.IsEmpty() || tile_covers_bounds) {
    low_res_raster_contents_scale_ = raster_contents_scale_key();
    return;
  }

  float low_res_factor =
      layer_tree_impl()->settings().low_res_contents_scale_factor;
  low_res_raster_contents_scale_ =
      std::max(raster_contents_scale_key() * low_res_factor, min_scale);
  DCHECK_LE(low_res_raster_contents_scale_, raster_contents_scale_key());
  DCHECK_GE(low_res_raster_contents_scale_, min_scale);
  DCHECK_LE(low_res_raster_contents_scale_, max_scale);
}

void PictureLayerImpl::AdjustRasterScaleForTransformAnimation(
    const gfx::Vector2dF& preserved_raster_contents_scale) {
  DCHECK(draw_properties().screen_space_transform_is_animating);

  float maximum_animation_scale =
      layer_tree_impl()->property_trees()->MaximumAnimationToScreenScale(
          transform_tree_index());
  raster_contents_scale_.SetToMax(
      gfx::Vector2dF(maximum_animation_scale, maximum_animation_scale));

  if (AffectedByWillChangeTransformHint()) {
    // If we have a will-change: transform hint, do not shrink the content
    // raster scale, otherwise we will end up throwing away larger tiles we may
    // need again.
    raster_contents_scale_.SetToMax(preserved_raster_contents_scale);
  }

  // However we want to avoid excessive memory use. Choose a scale at which this
  // layer's rastered content is not larger than the viewport.
  gfx::Size viewport = layer_tree_impl()->GetDeviceViewport().size();
  // To avoid too small scale in a small viewport.
  constexpr int kMinViewportDimension = 500;
  float max_viewport_dimension =
      std::max({viewport.width(), viewport.height(), kMinViewportDimension});
  DCHECK(max_viewport_dimension);
  // Use square to compensate for viewports with different aspect ratios.
  float squared_viewport_area = max_viewport_dimension * max_viewport_dimension;

  gfx::SizeF max_visible_bounds(raster_source_->recorded_bounds().size());
  // Clamp max_visible_bounds by max_viewport_dimension to avoid too small
  // scale for huge layers for which the far from viewport area won't be
  // rasterized and out of viewport area is rasterized in low priority.
  max_visible_bounds.SetToMin(
      gfx::SizeF(max_viewport_dimension, max_viewport_dimension));
  gfx::SizeF max_visible_bounds_at_max_scale =
      gfx::ScaleSize(max_visible_bounds, raster_contents_scale_.x(),
                     raster_contents_scale_.y());
  float maximum_area = max_visible_bounds_at_max_scale.width() *
                       max_visible_bounds_at_max_scale.height();
  // Clamp the scale to make the rastered content not larger than the viewport.
  if (maximum_area > squared_viewport_area) [[unlikely]] {
    raster_contents_scale_.Scale(
        1.f / std::sqrt(maximum_area / squared_viewport_area));
  }
}

void PictureLayerImpl::CleanUpTilingsOnActiveLayer(
    const std::vector<raw_ptr<PictureLayerTiling, VectorExperimental>>&
        used_tilings) {
  DCHECK(layer_tree_impl()->IsActiveTree());
  if (tilings_->num_tilings() == 0)
    return;

  float min_acceptable_high_res_scale =
      std::min(raster_contents_scale_key(), ideal_contents_scale_key());
  float max_acceptable_high_res_scale =
      std::max(raster_contents_scale_key(), ideal_contents_scale_key());

  PictureLayerImpl* twin = GetPendingOrActiveTwinLayer();
  if (twin && twin->CanHaveTilings()) {
    min_acceptable_high_res_scale = std::min(
        {min_acceptable_high_res_scale, twin->raster_contents_scale_key(),
         twin->ideal_contents_scale_key()});
    max_acceptable_high_res_scale = std::max(
        {max_acceptable_high_res_scale, twin->raster_contents_scale_key(),
         twin->ideal_contents_scale_key()});
  }

  PictureLayerTilingSet* twin_set = twin ? twin->tilings_.get() : nullptr;
  tilings_->CleanUpTilings(min_acceptable_high_res_scale,
                           max_acceptable_high_res_scale, used_tilings,
                           twin_set);
}

float PictureLayerImpl::MinimumRasterContentsScaleForWillChangeTransform()
    const {
  DCHECK(AffectedByWillChangeTransformHint());
  float native_scale = ideal_device_scale_ * ideal_page_scale_;
  float ideal_scale = ideal_contents_scale_key();
  // We want to use the same raster scale as much as possible during the
  // lifetime of a will-change:transform layer to avoid rerasterization.
  // Normally, we clamp the raster scale to be at least the native scale, to
  // make most HTML contents not too blurry (e.g. at least the texts are
  // legible) if the ideal scale increases above the native scale in the future.
  if (ideal_scale < native_scale * kMinScaleRatioForWillChangeTransform) {
    // However, if the native scale is too big compared to the ideal scale,
    // we want to use a smaller scale to avoid too many tiles using too much
    // memory. This is mainly to avoid problems in SVG apps that use large
    // integer geometries in elements under a very small overall scale to avoid
    // floating-point errors in geometries. The return value is smaller than
    // ideal_scale to reduce rerasterizations when the ideal scale changes to
    // be even smaller in the future.
    return ideal_scale * kMinScaleRatioForWillChangeTransform;
  }
  return native_scale;
}

bool PictureLayerImpl::CalculateRasterTranslation(
    gfx::Vector2dF& raster_translation) const {
  // If this setting is set, the client (e.g. the Chromium UI) is sure that it
  // can almost always align raster pixels to physical pixels, and doesn't care
  // about temporary misalignment, so don't bother raster translation.
  if (layer_tree_impl()->settings().layers_always_allowed_lcd_text)
    return true;

  // No need to use raster translation if there is no text.
  if (!raster_source_ || !raster_source_->GetDisplayItemList() ||
      !raster_source_->GetDisplayItemList()->has_draw_text_ops()) {
    return false;
  }

  const gfx::Transform& screen_transform = ScreenSpaceTransform();
  gfx::Transform draw_transform = DrawTransform();

  if (!screen_transform.IsScaleOrTranslation() ||
      !draw_transform.IsScaleOrTranslation()) {
    return false;
  }

  // It is only useful to align the content space to the target space if their
  // relative pixel ratio is some small rational number. Currently we only
  // align if the relative pixel ratio is 1:1 (i.e. the scale components of
  // both the screen transform and the draw transform are approximately the same
  // as |raster_contents_scale_|). Good match if the maximum alignment error on
  // a layer of size 10000px does not exceed 0.001px.
  static constexpr float kPixelErrorThreshold = 0.001f;
  static constexpr float kScaleErrorThreshold = kPixelErrorThreshold / 10000;
  auto is_raster_scale = [this](const gfx::Transform& transform) -> bool {
    // The matrix has the X scale at (0,0), and the Y scale at (1,1).
    gfx::Vector2dF scale_diff = transform.To2dScale() - raster_contents_scale_;
    return std::abs(scale_diff.x()) <= kScaleErrorThreshold &&
           std::abs(scale_diff.y()) <= kScaleErrorThreshold;
  };
  if (!is_raster_scale(screen_transform) || !is_raster_scale(draw_transform))
    return false;

  // Extract the fractional part of layer origin in the screen space and in the
  // target space.
  auto fraction = [](float f) -> float { return f - floorf(f); };
  gfx::Vector2dF screen_translation = screen_transform.To2dTranslation();
  float screen_x_fraction = fraction(screen_translation.x());
  float screen_y_fraction = fraction(screen_translation.y());
  gfx::Vector2dF draw_translation = draw_transform.To2dTranslation();
  float target_x_fraction = fraction(draw_translation.x());
  float target_y_fraction = fraction(draw_translation.y());

  // If the origin is different in the screen space and in the target space,
  // it means the render target is not aligned to physical pixels, and the
  // text content will be blurry regardless of raster translation.
  if (std::abs(screen_x_fraction - target_x_fraction) > kPixelErrorThreshold ||
      std::abs(screen_y_fraction - target_y_fraction) > kPixelErrorThreshold) {
    return false;
  }

  raster_translation = gfx::Vector2dF(target_x_fraction, target_y_fraction);
  return true;
}

float PictureLayerImpl::MinimumContentsScale() const {
  // If the contents scale is less than 1 / width (also for height),
  // then it will end up having less than one pixel of content in that
  // dimension.  Bump the minimum contents scale up in this case to prevent
  // this from happening.
  gfx::Size recorded_size = raster_source_->recorded_bounds().size();
  int min_dimension = std::min(recorded_size.width(), recorded_size.height());
  return min_dimension ? 1.f / min_dimension : 1.f;
}

float PictureLayerImpl::MaximumContentsScale() const {
  if (bounds().IsEmpty())
    return 0;
  // When mask tiling is disabled or the mask is single textured, masks can not
  // have tilings that would become larger than the max_texture_size since they
  // use a single tile for the entire tiling. Other layers can have tilings such
  // that dimension * scale does not overflow.
  float max_dimension = static_cast<float>(
      is_backdrop_filter_mask_ ? layer_tree_impl()->max_texture_size()
                               : std::numeric_limits<int>::max());
  int higher_dimension = std::max(bounds().width(), bounds().height());
  float max_scale = max_dimension / higher_dimension;

  // We require that multiplying the layer size by the contents scale and
  // ceiling produces a value <= |max_dimension|. Because for large layer
  // sizes floating point ambiguity may crop up, making the result larger or
  // smaller than expected, we use a slightly smaller floating point value for
  // the scale, to help ensure that the resulting content bounds will never end
  // up larger than |max_dimension|.
  return nextafterf(max_scale, 0.f);
}

void PictureLayerImpl::ResetRasterScale() {
  raster_page_scale_ = 0.f;
  raster_device_scale_ = 0.f;
  raster_source_scale_ = gfx::Vector2dF(0.f, 0.f);
  raster_contents_scale_ = gfx::Vector2dF(0.f, 0.f);
  low_res_raster_contents_scale_ = 0.f;
  directly_composited_image_default_raster_scale_ = 0.f;
}

bool PictureLayerImpl::CanHaveTilings() const {
  if (!raster_source_)
    return false;
  if (raster_source_->IsSolidColor())
    return false;
  if (!draws_content())
    return false;
  if (!raster_source_->HasRecordings())
    return false;
  // If the |raster_source_| has a recording it should have non-empty bounds.
  DCHECK(!raster_source_->size().IsEmpty());
  if (MaximumContentsScale() < MinimumContentsScale())
    return false;
  return true;
}

void PictureLayerImpl::SanityCheckTilingState() const {
#if DCHECK_IS_ON()
  if (!CanHaveTilings()) {
    DCHECK_EQ(0u, tilings_->num_tilings());
    return;
  }
  if (tilings_->num_tilings() == 0)
    return;

  // We should only have one high res tiling.
  DCHECK_EQ(1, tilings_->NumHighResTilings());
#endif
}

float PictureLayerImpl::MaximumTilingContentsScale() const {
  float max_contents_scale = tilings_->GetMaximumContentsScale();
  return std::max(max_contents_scale, MinimumContentsScale());
}

std::unique_ptr<PictureLayerTilingSet>
PictureLayerImpl::CreatePictureLayerTilingSet() {
  const LayerTreeSettings& settings = layer_tree_impl()->settings();
  return PictureLayerTilingSet::Create(
      IsActive() ? ACTIVE_TREE : PENDING_TREE, this,
      settings.tiling_interest_area_padding,
      layer_tree_impl()->use_gpu_rasterization()
          ? settings.gpu_rasterization_skewport_target_time_in_seconds
          : settings.skewport_target_time_in_seconds,
      settings.skewport_extrapolation_limit_in_screen_pixels,
      settings.max_preraster_distance_in_screen_pixels);
}

void PictureLayerImpl::UpdateIdealScales() {
  DCHECK(CanHaveTilings());

  float min_contents_scale = MinimumContentsScale();
  DCHECK_GT(min_contents_scale, 0.f);

  ideal_device_scale_ = layer_tree_impl()->device_scale_factor();
  ideal_page_scale_ = 1.f;
  ideal_contents_scale_ = GetIdealContentsScale();

  if (layer_tree_impl()->PageScaleTransformNode()) {
    DCHECK(layer_tree_impl()->settings().is_for_scalable_page);
    ideal_page_scale_ = IsAffectedByPageScale()
                            ? layer_tree_impl()->current_page_scale_factor()
                            : 1.f;
  }

  // This layer may be in a layer tree embedded in a hierarchy that has its own
  // page scale factor. We represent that here as 'external_page_scale_factor',
  // a value that affects raster scale in the same way that page_scale_factor
  // does, but doesn't affect any geometry calculations. In a normal main frame,
  // fenced frame, or OOPIF, only one of current or external page scale factor
  // is ever used but not both. The only exception to this is a main frame in a
  // a guest view. In these cases we may have a current_page_scale_factor (e.g.
  // due to a viewport <meta> tag) as well as an external_page_scale_factor
  // coming from the page scale of its embedder page.
  float external_page_scale_factor =
      layer_tree_impl() ? layer_tree_impl()->external_page_scale_factor() : 1.f;
  DCHECK(!layer_tree_impl() ||
         layer_tree_impl()->settings().is_for_scalable_page ||
         external_page_scale_factor == 1.f ||
         layer_tree_impl()->current_page_scale_factor() == 1.f);
  ideal_page_scale_ *= external_page_scale_factor;
  ideal_contents_scale_.Scale(external_page_scale_factor);

  ideal_contents_scale_.SetToMax(
      gfx::Vector2dF(min_contents_scale, min_contents_scale));
  ideal_contents_scale_.SetToMin(
      gfx::Vector2dF(kMaxIdealContentsScale, kMaxIdealContentsScale));
  ideal_source_scale_ = {ideal_contents_scale_.x() / ideal_page_scale_,
                         ideal_contents_scale_.y() / ideal_page_scale_};
}

void PictureLayerImpl::GetDebugBorderProperties(SkColor4f* color,
                                                float* width) const {
  float device_scale_factor =
      layer_tree_impl() ? layer_tree_impl()->device_scale_factor() : 1;

  if (IsDirectlyCompositedImage()) {
    *color = DebugColors::ImageLayerBorderColor();
    *width = DebugColors::ImageLayerBorderWidth(device_scale_factor);
  } else {
    *color = DebugColors::TiledContentLayerBorderColor();
    *width = DebugColors::TiledContentLayerBorderWidth(device_scale_factor);
  }
}

void PictureLayerImpl::GetAllPrioritizedTilesForTracing(
    std::vector<PrioritizedTile>* prioritized_tiles) const {
  if (!tilings_)
    return;
  tilings_->GetAllPrioritizedTilesForTracing(prioritized_tiles);
}

void PictureLayerImpl::AsValueInto(
    base::trace_event::TracedValue* state) const {
  LayerImpl::AsValueInto(state);
  state->SetDouble("ideal_contents_scale", ideal_contents_scale_key());
  state->SetDouble("geometry_contents_scale", MaximumTilingContentsScale());
  state->BeginArray("tilings");
  tilings_->AsValueInto(state);
  state->EndArray();

  MathUtil::AddToTracedValue("tile_priority_rect",
                             viewport_rect_for_tile_priority_in_content_space_,
                             state);
  MathUtil::AddToTracedValue("visible_rect", visible_layer_rect(), state);

  state->SetString(
      "lcd_text_disallowed_reason",
      LCDTextDisallowedReasonToString(lcd_text_disallowed_reason_));

  state->BeginArray("pictures");
  raster_source_->AsValueInto(state);
  state->EndArray();

  state->BeginArray("invalidation");
  invalidation_.AsValueInto(state);
  state->EndArray();

  state->BeginArray("coverage_tiles");
  for (auto iter =
           tilings_->Cover(gfx::Rect(bounds()), MaximumTilingContentsScale(),
                           ideal_contents_scale_key());
       iter; ++iter) {
    state->BeginDictionary();

    MathUtil::AddToTracedValue("geometry_rect", iter.geometry_rect(), state);

    if (*iter)
      viz::TracedValue::SetIDRef(*iter, state, "tile");

    state->EndDictionary();
  }
  state->EndArray();

  state->BeginDictionary("can_have_tilings_state");
  state->SetBoolean("can_have_tilings", CanHaveTilings());
  state->SetBoolean("raster_source_solid_color",
                    raster_source_->IsSolidColor());
  state->SetBoolean("draws_content", draws_content());
  state->SetBoolean("raster_source_has_recordings",
                    raster_source_->HasRecordings());
  state->SetDouble("max_contents_scale", MaximumTilingContentsScale());
  state->SetDouble("min_contents_scale", MinimumContentsScale());
  state->EndDictionary();

  state->BeginDictionary("raster_scales");
  state->SetDouble("page_scale", raster_page_scale_);
  state->SetDouble("device_scale", raster_device_scale_);
  state->BeginArray("source_scale");
  state->AppendDouble(raster_source_scale_.x());
  state->AppendDouble(raster_source_scale_.y());
  state->EndArray();
  state->BeginArray("contents_scale");
  state->AppendDouble(raster_contents_scale_.x());
  state->AppendDouble(raster_contents_scale_.y());
  state->EndArray();
  state->SetDouble("low_res_contents_scale", low_res_raster_contents_scale_);
  state->EndDictionary();

  state->BeginDictionary("ideal_scales");
  state->SetDouble("page_scale", ideal_page_scale_);
  state->SetDouble("device_scale", ideal_device_scale_);
  state->BeginArray("source_scale");
  state->AppendDouble(ideal_source_scale_.x());
  state->AppendDouble(ideal_source_scale_.y());
  state->EndArray();
  state->BeginArray("contents_scale");
  state->AppendDouble(ideal_contents_scale_.x());
  state->AppendDouble(ideal_contents_scale_.y());
  state->EndArray();
  state->EndDictionary();
}

size_t PictureLayerImpl::GPUMemoryUsageInBytes() const {
  return tilings_->GPUMemoryUsageInBytes();
}

void PictureLayerImpl::RunMicroBenchmark(MicroBenchmarkImpl* benchmark) {
  benchmark->RunOnLayer(this);
}

bool PictureLayerImpl::IsOnActiveOrPendingTree() const {
  return !layer_tree_impl()->IsRecycleTree();
}

bool PictureLayerImpl::HasValidTilePriorities() const {
  return IsOnActiveOrPendingTree() &&
         (contributes_to_drawn_render_surface() || raster_even_if_not_drawn());
}

PictureLayerImpl::ImageInvalidationResult
PictureLayerImpl::InvalidateRegionForImages(
    const PaintImageIdFlatSet& images_to_invalidate) {
  if (!discardable_image_map_ || discardable_image_map_->empty()) {
    return ImageInvalidationResult::kNoImages;
  }

  bool all_animated_image = true;
  auto* controller = layer_tree_impl()->image_animation_controller();
  InvalidationRegion image_invalidation;
  for (auto image_id : images_to_invalidate) {
    all_animated_image &= controller->IsRegistered(image_id);
    const auto& rects = discardable_image_map_->GetRectsForImage(image_id);
    for (const auto& r : rects) {
      image_invalidation.Union(r);
    }
  }
  Region invalidation;
  image_invalidation.Swap(&invalidation);

  if (invalidation.IsEmpty())
    return ImageInvalidationResult::kNoInvalidation;

  // Note: We can use a rect here since this is only used to track damage for a
  // frame and not raster invalidation.
  UnionUpdateRect(invalidation.bounds());
  if (all_animated_image) {
    has_animated_image_update_rect_ = true;
  } else {
    has_non_animated_image_update_rect_ = true;
  }

  invalidation_.Union(invalidation);
  tilings_->Invalidate(invalidation);
  // TODO(crbug.com/40335690): SetNeedsPushProperties() would be needed here if
  // PictureLayerImpl didn't always push properties every activation.
  return ImageInvalidationResult::kInvalidated;
}

void PictureLayerImpl::InvalidateRasterInducingScrolls(
    const base::flat_set<ElementId>& scrolls_to_invalidate) {
  if (!raster_source_ || !raster_source_->GetDisplayItemList()) {
    return;
  }
  const DisplayItemList::RasterInducingScrollMap& raster_inducing_scrolls =
      raster_source_->GetDisplayItemList()->raster_inducing_scrolls();
  Region invalidation;
  for (ElementId element_id : scrolls_to_invalidate) {
    auto it = raster_inducing_scrolls.find(element_id);
    if (it != raster_inducing_scrolls.end()) {
      UnionUpdateRect(it->second.visual_rect);
      has_non_animated_image_update_rect_ = true;
      invalidation.Union(it->second.visual_rect);
      needs_regenerate_discardable_image_map_ |=
          it->second.has_discardable_images;
    }
  }

  // Raster-inducing scroll may affect the discardable image map due to changed
  // scroll offsets.
  RegenerateDiscardableImageMapIfNeeded();

  if (!invalidation.IsEmpty()) {
    invalidation_.Union(invalidation);
    tilings_->Invalidate(invalidation);
  }
}

void PictureLayerImpl::SetPaintWorkletRecord(
    scoped_refptr<const PaintWorkletInput> input,
    PaintRecord record) {
  DCHECK(base::Contains(paint_worklet_records_, input));
  paint_worklet_records_[input].second = std::move(record);
}

void PictureLayerImpl::RegisterAnimatedImages() {
  if (!discardable_image_map_) {
    return;
  }

  auto* controller = layer_tree_impl()->image_animation_controller();
  for (const auto& data : discardable_image_map_->animated_images_metadata()) {
    // Only update the metadata from updated recordings received from a commit.
    if (layer_tree_impl()->IsSyncTree())
      controller->UpdateAnimatedImage(data);
    controller->RegisterAnimationDriver(data.paint_image_id, this);
  }
}

void PictureLayerImpl::UnregisterAnimatedImages() {
  if (!discardable_image_map_) {
    return;
  }

  auto* controller = layer_tree_impl()->image_animation_controller();
  for (const auto& data : discardable_image_map_->animated_images_metadata()) {
    controller->UnregisterAnimationDriver(data.paint_image_id, this);
  }
}

void PictureLayerImpl::SetPaintWorkletInputs(
    const std::vector<DiscardableImageMap::PaintWorkletInputWithImageId>&
        inputs) {
  // PaintWorklets are not supported when committing directly to the active
  // tree, so in that case the |inputs| should always be empty.
  DCHECK(layer_tree_impl()->IsPendingTree() || inputs.empty());

  bool had_paint_worklets = !paint_worklet_records_.empty();
  PaintWorkletRecordMap new_records;
  for (const auto& input_with_id : inputs) {
    const auto& input = input_with_id.first;
    const auto& paint_image_id = input_with_id.second;
    auto it = new_records.find(input);
    // We should never have multiple PaintImages sharing the same paint worklet.
    DCHECK(it == new_records.end() || it->second.first == paint_image_id);
    // Attempt to re-use an existing PaintRecord if possible.
    new_records[input] = std::make_pair(
        paint_image_id, std::move(paint_worklet_records_[input].second));
    // The move constructor of std::optional does not clear the source to
    // nullopt.
    paint_worklet_records_[input].second = std::nullopt;
  }
  paint_worklet_records_.swap(new_records);

  // The pending tree tracks which PictureLayerImpls have PaintWorkletInputs as
  // an optimization to avoid walking all picture layers.
  bool has_paint_worklets = !paint_worklet_records_.empty();
  if ((has_paint_worklets != had_paint_worklets) &&
      layer_tree_impl()->IsPendingTree()) {
    // TODO(xidachen): We don't need additional tracking on LayerTreeImpl. The
    // tracking in AnimatedPaintWorkletTracker should be enough.
    layer_tree_impl()->NotifyLayerHasPaintWorkletsChanged(this,
                                                          has_paint_worklets);
  }
  if (layer_tree_impl()->IsPendingTree()) {
    layer_tree_impl()
        ->paint_worklet_tracker()
        .UpdatePaintWorkletInputProperties(inputs, this);
  }
}

void PictureLayerImpl::InvalidatePaintWorklets(
    const PaintWorkletInput::PropertyKey& key,
    const PaintWorkletInput::PropertyValue& prev,
    const PaintWorkletInput::PropertyValue& next) {
  for (auto& entry : paint_worklet_records_) {
    const std::vector<PaintWorkletInput::PropertyKey>& prop_ids =
        entry.first->GetPropertyKeys();
    // If the PaintWorklet depends on the property whose value was changed by
    // the animation system, then invalidate its associated PaintRecord so that
    // we can repaint the PaintWorklet during impl side invalidation.
    if (base::Contains(prop_ids, key) &&
        entry.first->ValueChangeShouldCauseRepaint(prev, next)) {
      entry.second.second = std::nullopt;
    }
  }
}

PictureLayerImpl::TileUpdateSet PictureLayerImpl::TakeUpdatedTiles() {
  TileUpdateSet updates;
  updates.swap(updated_tiles_);
  return updates;
}

gfx::ContentColorUsage PictureLayerImpl::GetContentColorUsage() const {
  auto display_item_list = raster_source_->GetDisplayItemList();
  if (!display_item_list)
    return gfx::ContentColorUsage::kSRGB;

  return display_item_list->content_color_usage();
}

DamageReasonSet PictureLayerImpl::GetDamageReasons() const {
  DamageReasonSet reasons;
  if (has_animated_image_update_rect_) {
    reasons.Put(DamageReason::kAnimatedImage);
  }
  if (LayerPropertyChanged() || has_non_animated_image_update_rect_ ||
      !damage_rect_.IsEmpty()) {
    reasons.Put(DamageReason::kUntracked);
  }
  return reasons;
}

}  // namespace cc
