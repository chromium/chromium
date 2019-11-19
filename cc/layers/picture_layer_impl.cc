// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/picture_layer_impl.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/numerics/ranges.h"
#include "base/time/time.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
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
    : LayerImpl(tree_impl, id, /*will_always_push_properties=*/true),
      twin_layer_(nullptr),
      tilings_(CreatePictureLayerTilingSet()),
      ideal_page_scale_(0.f),
      ideal_device_scale_(0.f),
      ideal_source_scale_(0.f),
      ideal_contents_scale_(0.f),
      raster_page_scale_(0.f),
      raster_device_scale_(0.f),
      raster_source_scale_(0.f),
      raster_contents_scale_(0.f),
      low_res_raster_contents_scale_(0.f),
      is_backdrop_filter_mask_(false),
      was_screen_space_transform_animating_(false),
      only_used_low_res_last_append_quads_(false),
      nearest_neighbor_(false),
      use_transformed_rasterization_(false),
      is_directly_composited_image_(false),
      can_use_lcd_text_(true),
      tile_size_calculator_(this) {
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

const char* PictureLayerImpl::LayerTypeAsString() const {
  return "cc::PictureLayerImpl";
}

std::unique_ptr<LayerImpl> PictureLayerImpl::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return PictureLayerImpl::Create(tree_impl, id());
}

void PictureLayerImpl::PushPropertiesTo(LayerImpl* base_layer) {
  PictureLayerImpl* layer_impl = static_cast<PictureLayerImpl*>(base_layer);


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

  layer_impl->SetNearestNeighbor(nearest_neighbor_);
  layer_impl->SetUseTransformedRasterization(use_transformed_rasterization_);
  layer_impl->SetIsBackdropFilterMask(is_backdrop_filter_mask_);

  // Solid color layers have no tilings.
  DCHECK(!raster_source_->IsSolidColor() || tilings_->num_tilings() == 0);
  // The pending tree should only have a high res (and possibly low res) tiling.
  DCHECK_LE(tilings_->num_tilings(),
            layer_tree_impl()->create_low_res_tiling() ? 2u : 1u);

  layer_impl->set_gpu_raster_max_texture_size(gpu_raster_max_texture_size_);
  layer_impl->UpdateRasterSource(raster_source_, &invalidation_, tilings_.get(),
                                 &paint_worklet_records_);
  DCHECK(invalidation_.IsEmpty());

  // After syncing a solid color layer, the active layer has no tilings.
  DCHECK(!raster_source_->IsSolidColor() ||
         layer_impl->tilings_->num_tilings() == 0);

  layer_impl->raster_page_scale_ = raster_page_scale_;
  layer_impl->raster_device_scale_ = raster_device_scale_;
  layer_impl->raster_source_scale_ = raster_source_scale_;
  layer_impl->raster_contents_scale_ = raster_contents_scale_;
  layer_impl->low_res_raster_contents_scale_ = low_res_raster_contents_scale_;
  layer_impl->is_directly_composited_image_ = is_directly_composited_image_;
  // Simply push the value to the active tree without any extra invalidations,
  // since the pending tree tiles would have this handled. This is here to
  // ensure the state is consistent for future raster.
  layer_impl->can_use_lcd_text_ = can_use_lcd_text_;

  layer_impl->SanityCheckTilingState();
}

void PictureLayerImpl::AppendQuads(viz::RenderPass* render_pass,
                                   AppendQuadsData* append_quads_data) {
  // RenderSurfaceImpl::AppendQuads sets mask properties in the DrawQuad for
  // the masked surface, which will apply to both the backdrop filter and the
  // contents of the masked surface, so we should not append quads of the mask
  // layer in DstIn blend mode which would apply the mask in another codepath.
  if (is_backdrop_filter_mask_)
    return;

  // The bounds and the pile size may differ if the pile wasn't updated (ie.
  // PictureLayer::Update didn't happen). In that case the pile will be empty.
  DCHECK(raster_source_->GetSize().IsEmpty() ||
         bounds() == raster_source_->GetSize())
      << " bounds " << bounds().ToString() << " pile "
      << raster_source_->GetSize().ToString();

  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();

  if (raster_source_->IsSolidColor()) {
    // TODO(979672): This is still hard-coded at 1.0. This has some history:
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
    gfx::Rect scaled_recorded_viewport = gfx::ScaleToEnclosingRect(
        raster_source_->RecordedViewport(), max_contents_scale);
    geometry_rect.Intersect(scaled_recorded_viewport);
    visible_geometry_rect.Intersect(scaled_recorded_viewport);

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
    for (const auto& image_data : raster_source_->GetDisplayItemList()
                                      ->discardable_image_map()
                                      .animated_images_metadata()) {
      image_animation_map[image_data.paint_image_id] =
          controller->GetFrameIndexForImage(image_data.paint_image_id, tree);
    }

    auto* quad = render_pass->CreateAndAppendDrawQuad<viz::PictureDrawQuad>();
    quad->SetNew(shared_quad_state, geometry_rect, visible_geometry_rect,
                 needs_blending, texture_rect, texture_size, nearest_neighbor_,
                 viz::RGBA_8888, quad_content_rect, max_contents_scale,
                 std::move(image_animation_map),
                 raster_source_->GetDisplayItemList());
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
    for (PictureLayerTilingSet::CoverageIterator iter(
             tilings_.get(), max_contents_scale,
             shared_quad_state->visible_quad_layer_rect, ideal_contents_scale_);
         iter; ++iter) {
      SkColor color;
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
      debug_border_quad->SetNew(shared_quad_state,
                                geometry_rect,
                                visible_geometry_rect,
                                color,
                                width);
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

  size_t missing_tile_count = 0u;
  size_t on_demand_missing_tile_count = 0u;
  only_used_low_res_last_append_quads_ = true;
  gfx::Rect scaled_recorded_viewport = gfx::ScaleToEnclosingRect(
      raster_source_->RecordedViewport(), max_contents_scale);
  for (PictureLayerTilingSet::CoverageIterator iter(
           tilings_.get(), max_contents_scale,
           shared_quad_state->visible_quad_layer_rect, ideal_contents_scale_);
       iter; ++iter) {
    gfx::Rect geometry_rect = iter.geometry_rect();
    gfx::Rect visible_geometry_rect =
        scaled_occlusion.GetUnoccludedContentRect(geometry_rect);

    gfx::Rect offset_geometry_rect = geometry_rect;
    offset_geometry_rect.Offset(quad_offset);
    gfx::Rect offset_visible_geometry_rect = visible_geometry_rect;
    offset_visible_geometry_rect.Offset(quad_offset);

    bool needs_blending = !contents_opaque();
    if (visible_geometry_rect.IsEmpty())
      continue;

    int64_t visible_geometry_area =
        static_cast<int64_t>(visible_geometry_rect.width()) *
        visible_geometry_rect.height();
    append_quads_data->visible_layer_area += visible_geometry_area;

    bool has_draw_quad = false;
    if (*iter && iter->draw_info().IsReadyToDraw()) {
      const TileDrawInfo& draw_info = iter->draw_info();

      switch (draw_info.mode()) {
        case TileDrawInfo::RESOURCE_MODE: {
          gfx::RectF texture_rect = iter.texture_rect();

          // The raster_contents_scale_ is the best scale that the layer is
          // trying to produce, even though it may not be ideal. Since that's
          // the best the layer can promise in the future, consider those as
          // complete. But if a tile is ideal scale, we don't want to consider
          // it incomplete and trying to replace it with a tile at a worse
          // scale.
          if (iter->contents_scale_key() != raster_contents_scale_ &&
              iter->contents_scale_key() != ideal_contents_scale_ &&
              geometry_rect.Intersects(scaled_viewport_for_tile_priority)) {
            append_quads_data->num_incomplete_tiles++;
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
          float alpha =
              (SkColorGetA(draw_info.solid_color()) * (1.0f / 255.0f)) *
              shared_quad_state->opacity;
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

    if (!has_draw_quad) {
      // Checkerboard.
      SkColor color = SafeOpaqueBackgroundColor();
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
      // Intersect checkerboard rect with interest rect to generate rect where
      // we checkerboarded and has recording. The area where we don't have
      // recording is not necessarily a Rect, and its area is calculated using
      // subtraction.
      gfx::Rect visible_rect_has_recording = visible_geometry_rect;
      visible_rect_has_recording.Intersect(scaled_recorded_viewport);
      int64_t checkerboarded_has_recording_area =
          static_cast<int64_t>(visible_rect_has_recording.width()) *
          visible_rect_has_recording.height();
      append_quads_data->checkerboarded_needs_raster_content_area +=
          checkerboarded_has_recording_area;
      append_quads_data->checkerboarded_no_recording_content_area +=
          visible_geometry_area - checkerboarded_has_recording_area;
      continue;
    }

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
    TRACE_EVENT_INSTANT2("cc",
                         "PictureLayerImpl::AppendQuads checkerboard",
                         TRACE_EVENT_SCOPE_THREAD,
                         "missing_tile_count",
                         missing_tile_count,
                         "on_demand_missing_tile_count",
                         on_demand_missing_tile_count);
  }

  // Aggressively remove any tilings that are not seen to save memory. Note
  // that this is at the expense of doing cause more frequent re-painting. A
  // better scheme would be to maintain a tighter visible_layer_rect for the
  // finer tilings.
  CleanUpTilingsOnActiveLayer(last_append_quads_tilings_);
}

bool PictureLayerImpl::UpdateTiles() {
  if (!CanHaveTilings()) {
    ideal_page_scale_ = 0.f;
    ideal_device_scale_ = 0.f;
    ideal_contents_scale_ = 0.f;
    ideal_source_scale_ = 0.f;
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

  if (!raster_contents_scale_ || ShouldAdjustRasterScale()) {
    RecalculateRasterScales();
    AddTilingsForRasterScale();
  }

  if (layer_tree_impl()->IsActiveTree())
    AddLowResolutionTilingIfNeeded();

  DCHECK(raster_page_scale_);
  DCHECK(raster_device_scale_);
  DCHECK(raster_source_scale_);
  DCHECK(raster_contents_scale_);
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
      viewport_rect_for_tile_priority_in_content_space_, ideal_contents_scale_,
      current_frame_time_in_seconds, occlusion_in_content_space,
      can_require_tiles_for_activation);
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
    gfx::Transform view_to_layer(gfx::Transform::kSkipInitialization);
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
      padded_bounds.Inset(-padding_amount, -padding_amount);
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
    Region* new_invalidation,
    const PictureLayerTilingSet* pending_set,
    const PaintWorkletRecordMap* pending_paint_worklet_records) {
  // The bounds and the pile size may differ if the pile wasn't updated (ie.
  // PictureLayer::Update didn't happen). In that case the pile will be empty.
  DCHECK(raster_source->GetSize().IsEmpty() ||
         bounds() == raster_source->GetSize())
      << " bounds " << bounds().ToString() << " pile "
      << raster_source->GetSize().ToString();

  // We have an updated recording if the DisplayItemList in the new RasterSource
  // is different.
  const bool recording_updated =
      !raster_source_ || raster_source_->GetDisplayItemList() !=
                             raster_source->GetDisplayItemList();

  // Unregister for all images on the current raster source, if the recording
  // was updated.
  if (recording_updated) {
    UnregisterAnimatedImages();

    // When the display list changes, the set of PaintWorklets may also change.
    if (pending_paint_worklet_records) {
      paint_worklet_records_ = *pending_paint_worklet_records;
    } else {
      if (raster_source->GetDisplayItemList()) {
        SetPaintWorkletInputs(raster_source->GetDisplayItemList()
                                  ->discardable_image_map()
                                  .paint_worklet_inputs());
      } else {
        SetPaintWorkletInputs({});
      }
    }

    // If the MSAA sample count has changed, we need to re-raster the complete
    // layer.
    if (raster_source_ && raster_source_->GetDisplayItemList() &&
        raster_source->GetDisplayItemList() &&
        layer_tree_impl()->GetMSAASampleCountForRaster(
            raster_source_->GetDisplayItemList()) !=
            layer_tree_impl()->GetMSAASampleCountForRaster(
                raster_source->GetDisplayItemList())) {
      new_invalidation->Union(gfx::Rect(raster_source->GetSize()));
    }
  }

  // The |raster_source_| is initially null, so have to check for that for the
  // first frame.
  bool could_have_tilings = raster_source_.get() && CanHaveTilings();
  raster_source_.swap(raster_source);

  // Register images from the new raster source, if the recording was updated.
  // TODO(khushalsagar): UMA the number of animated images in layer?
  if (recording_updated)
    RegisterAnimatedImages();

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
  // TODO(crbug.com/843787): If the LayerTreeFrameSink is lost, and we activate,
  // this ends up running with the old LayerTreeFrameSink, or possibly with a
  // null LayerTreeFrameSink, which can give incorrect results or maybe crash.
  if (pending_set) {
    tilings_->UpdateTilingsToCurrentRasterSourceForActivation(
        raster_source_, pending_set, invalidation_, MinimumContentsScale(),
        MaximumContentsScale());
  } else {
    tilings_->UpdateTilingsToCurrentRasterSourceForCommit(
        raster_source_, invalidation_, MinimumContentsScale(),
        MaximumContentsScale());
    // We're in a commit, make sure to update the state of the checker image
    // tracker with the new async attribute data.
    layer_tree_impl()->UpdateImageDecodingHints(
        raster_source_->TakeDecodingModeMap());
  }
}

bool PictureLayerImpl::UpdateCanUseLCDTextAfterCommit() {
  DCHECK(layer_tree_impl()->IsSyncTree());

  // Once we disable lcd text, we don't re-enable it.
  if (!can_use_lcd_text_)
    return false;

  if (can_use_lcd_text_ == CanUseLCDText())
    return false;

  can_use_lcd_text_ = CanUseLCDText();
  // Synthetically invalidate everything.
  gfx::Rect bounds_rect(bounds());
  invalidation_ = Region(bounds_rect);
  tilings_->Invalidate(invalidation_);
  UnionUpdateRect(bounds_rect);
  return true;
}

void PictureLayerImpl::NotifyTileStateChanged(const Tile* tile) {
  if (layer_tree_impl()->IsActiveTree())
    damage_rect_.Union(tile->enclosing_layer_rect());
  if (tile->draw_info().NeedsRaster()) {
    PictureLayerTiling* tiling =
        tilings_->FindTilingWithScaleKey(tile->contents_scale_key());
    if (tiling)
      tiling->set_all_tiles_done(false);
  }
}

gfx::Rect PictureLayerImpl::GetDamageRect() const {
  return damage_rect_;
}

void PictureLayerImpl::ResetChangeTracking() {
  LayerImpl::ResetChangeTracking();
  damage_rect_.SetRect(0, 0, 0, 0);
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
  int flags = 0;

  // We don't handle solid color single texture masks for backdrop filters,
  // so we shouldn't bother analyzing those.
  // Otherwise, always analyze to maximize memory savings.
  if (!is_backdrop_filter_mask_)
    flags = Tile::USE_PICTURE_ANALYSIS;

  if (contents_opaque())
    flags |= Tile::IS_OPAQUE;

  return layer_tree_impl()->tile_manager()->CreateTile(
      info, id(), layer_tree_impl()->source_frame_number(), flags,
      can_use_lcd_text_);
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

gfx::Rect PictureLayerImpl::GetEnclosingRectInTargetSpace() const {
  return GetScaledEnclosingRectInTargetSpace(MaximumTilingContentsScale());
}

bool PictureLayerImpl::ShouldAnimate(PaintImage::Id paint_image_id) const {
  // If we are registered with the animation controller, which queries whether
  // the image should be animated, then we must have recordings with this image.
  DCHECK(raster_source_);
  DCHECK(raster_source_->GetDisplayItemList());
  DCHECK(
      !raster_source_->GetDisplayItemList()->discardable_image_map().empty());

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

  const auto& rects = raster_source_->GetDisplayItemList()
                          ->discardable_image_map()
                          .GetRectsForImage(paint_image_id);
  for (const auto& r : rects.container()) {
    if (r.Intersects(visible_layer_rect()))
      return true;
  }
  return false;
}

gfx::Size PictureLayerImpl::CalculateTileSize(const gfx::Size& content_bounds) {
  content_bounds_ = content_bounds;
  return tile_size_calculator_.CalculateTileSize();
}

void PictureLayerImpl::GetContentsResourceId(
    viz::ResourceId* resource_id,
    gfx::Size* resource_size,
    gfx::SizeF* resource_uv_size) const {
  // We need contents resource for backdrop filter masks only.
  if (!is_backdrop_filter_mask()) {
    *resource_id = 0;
    return;
  }

  // The bounds and the pile size may differ if the pile wasn't updated (ie.
  // PictureLayer::Update didn't happen). In that case the pile will be empty.
  DCHECK(raster_source_->GetSize().IsEmpty() ||
         bounds() == raster_source_->GetSize())
      << " bounds " << bounds().ToString() << " pile "
      << raster_source_->GetSize().ToString();
  float dest_scale = MaximumTilingContentsScale();
  gfx::Rect content_rect =
      gfx::ScaleToEnclosingRect(gfx::Rect(bounds()), dest_scale);
  PictureLayerTilingSet::CoverageIterator iter(
      tilings_.get(), dest_scale, content_rect, ideal_contents_scale_);

  // Mask resource not ready yet.
  if (!iter || !*iter) {
    *resource_id = 0;
    return;
  }

  // Masks only supported if they fit on exactly one tile.
  DCHECK(iter.geometry_rect() == content_rect)
      << "iter rect " << iter.geometry_rect().ToString() << " content rect "
      << content_rect.ToString();

  const TileDrawInfo& draw_info = iter->draw_info();
  if (!draw_info.IsReadyToDraw() ||
      draw_info.mode() != TileDrawInfo::RESOURCE_MODE) {
    *resource_id = 0;
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
      gfx::SizeF(iter->tiling()->tiling_data()->tiling_size());
  DCHECK_LE(requested_tile_size.width(), draw_info.resource_size().width());
  DCHECK_LE(requested_tile_size.height(), draw_info.resource_size().height());
  *resource_uv_size = gfx::SizeF(
      requested_tile_size.width() / draw_info.resource_size().width(),
      requested_tile_size.height() / draw_info.resource_size().height());
}

void PictureLayerImpl::SetNearestNeighbor(bool nearest_neighbor) {
  if (nearest_neighbor_ == nearest_neighbor)
    return;

  nearest_neighbor_ = nearest_neighbor;
  NoteLayerPropertyChanged();
}

void PictureLayerImpl::SetUseTransformedRasterization(bool use) {
  if (use_transformed_rasterization_ == use)
    return;

  use_transformed_rasterization_ = use;
  NoteLayerPropertyChanged();
}

PictureLayerTiling* PictureLayerImpl::AddTiling(
    const gfx::AxisTransform2d& contents_transform) {
  DCHECK(CanHaveTilings());
  DCHECK_GE(contents_transform.scale(), MinimumContentsScale());
  DCHECK_LE(contents_transform.scale(), MaximumContentsScale());
  DCHECK(raster_source_->HasRecordings());
  return tilings_->AddTiling(contents_transform, raster_source_);
}

void PictureLayerImpl::RemoveAllTilings() {
  tilings_->RemoveAllTilings();
  // If there are no tilings, then raster scales are no longer meaningful.
  ResetRasterScale();
}

void PictureLayerImpl::AddTilingsForRasterScale() {
  // Reset all resolution enums on tilings, we'll be setting new values in this
  // function.
  tilings_->MarkAllTilingsNonIdeal();

  PictureLayerTiling* high_res =
      tilings_->FindTilingWithScaleKey(raster_contents_scale_);
  // Note: This function is always invoked when raster scale is recomputed,
  // but not necessarily changed. This means raster translation update is also
  // always done when there are significant changes that triggered raster scale
  // recomputation.
  gfx::Vector2dF raster_translation =
      CalculateRasterTranslation(raster_contents_scale_);
  if (high_res &&
      high_res->raster_transform().translation() != raster_translation) {
    tilings_->Remove(high_res);
    high_res = nullptr;
  }
  if (!high_res) {
    // We always need a high res tiling, so create one if it doesn't exist.
    high_res = AddTiling(
        gfx::AxisTransform2d(raster_contents_scale_, raster_translation));
  } else if (high_res->may_contain_low_resolution_tiles()) {
    // If the tiling we find here was LOW_RESOLUTION previously, it may not be
    // fully rastered, so destroy the old tiles.
    high_res->Reset();
    // Reset the flag now that we'll make it high res, it will have fully
    // rastered content.
    high_res->reset_may_contain_low_resolution_tiles();
  }
  high_res->set_resolution(HIGH_RESOLUTION);

  if (layer_tree_impl()->IsPendingTree()) {
    // On the pending tree, drop any tilings that are non-ideal since we don't
    // need them to activate anyway.
    tilings_->RemoveNonIdealTilings();
  }

  SanityCheckTilingState();
}

bool PictureLayerImpl::ShouldAdjustRasterScale() const {
  if (is_directly_composited_image_) {
    float max_scale = std::max(1.f, MinimumContentsScale());
    if (raster_source_scale_ < std::min(ideal_source_scale_, max_scale))
      return true;
    if (raster_source_scale_ > 4 * ideal_source_scale_)
      return true;
    return false;
  }

  if (was_screen_space_transform_animating_ !=
      draw_properties().screen_space_transform_is_animating)
    return true;

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

  if (raster_contents_scale_ > MaximumContentsScale())
    return true;
  if (raster_contents_scale_ < MinimumContentsScale())
    return true;

  // Don't change the raster scale if any of the following are true:
  //  - We have an animating transform.
  //  - The raster scale is already ideal.
  if (draw_properties().screen_space_transform_is_animating ||
      raster_source_scale_ == ideal_source_scale_) {
    return false;
  }

  // Don't update will-change: transform layers if the raster contents scale is
  // at least the native scale (otherwise, we'd need to clamp it).
  if (has_will_change_transform_hint() &&
      raster_contents_scale_ >= raster_page_scale_ * raster_device_scale_) {
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
  if (raster_contents_scale_ == low_res_raster_contents_scale_)
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
  if (is_directly_composited_image_) {
    if (!raster_source_scale_)
      raster_source_scale_ = 1.f;

    float min_scale = MinimumContentsScale();
    float max_scale = std::max(1.f, MinimumContentsScale());
    float clamped_ideal_source_scale_ =
        base::ClampToRange(ideal_source_scale_, min_scale, max_scale);

    while (raster_source_scale_ < clamped_ideal_source_scale_)
      raster_source_scale_ *= 2.f;
    while (raster_source_scale_ > 4 * clamped_ideal_source_scale_)
      raster_source_scale_ /= 2.f;

    raster_source_scale_ =
        base::ClampToRange(raster_source_scale_, min_scale, max_scale);

    raster_page_scale_ = 1.f;
    raster_device_scale_ = 1.f;
    raster_contents_scale_ = raster_source_scale_;
    low_res_raster_contents_scale_ = raster_contents_scale_;
    return;
  }

  float old_raster_contents_scale = raster_contents_scale_;
  float old_raster_page_scale = raster_page_scale_;

  raster_device_scale_ = ideal_device_scale_;
  raster_page_scale_ = ideal_page_scale_;
  raster_source_scale_ = ideal_source_scale_;
  raster_contents_scale_ = ideal_contents_scale_;

  // During pinch we completely ignore the current ideal scale, and just use
  // a multiple of the previous scale.
  bool is_pinching = layer_tree_impl()->PinchGestureActive();
  if (is_pinching && old_raster_contents_scale) {
    // See ShouldAdjustRasterScale:
    // - When zooming out, preemptively create new tiling at lower resolution.
    // - When zooming in, approximate ideal using multiple of kMaxScaleRatio.
    bool zooming_out = old_raster_page_scale > ideal_page_scale_;
    float desired_contents_scale = old_raster_contents_scale;
    if (zooming_out) {
      while (desired_contents_scale > ideal_contents_scale_)
        desired_contents_scale /= kMaxScaleRatioDuringPinch;
    } else {
      while (desired_contents_scale < ideal_contents_scale_)
        desired_contents_scale *= kMaxScaleRatioDuringPinch;
    }
    raster_contents_scale_ = tilings_->GetSnappedContentsScaleKey(
        desired_contents_scale, kSnapToExistingTilingRatio);
    raster_page_scale_ =
        raster_contents_scale_ / raster_device_scale_ / raster_source_scale_;
  }

  // We rasterize at the maximum scale that will occur during the animation, if
  // the maximum scale is known. However we want to avoid excessive memory use.
  // If the scale is smaller than what we would choose otherwise, then it's
  // always better off for us memory-wise. But otherwise, we don't choose a
  // scale at which this layer's rastered content would become larger than the
  // viewport.
  if (draw_properties().screen_space_transform_is_animating) {
    bool can_raster_at_maximum_scale = false;
    bool should_raster_at_starting_scale = false;
    CombinedAnimationScale animation_scales =
        layer_tree_impl()->property_trees()->GetAnimationScales(
            transform_tree_index(), layer_tree_impl());
    float maximum_scale = animation_scales.maximum_animation_scale;
    float starting_scale = animation_scales.starting_animation_scale;
    if (maximum_scale != kNotScaled) {
      gfx::Size bounds_at_maximum_scale =
          gfx::ScaleToCeiledSize(raster_source_->GetSize(), maximum_scale);
      int64_t maximum_area =
          static_cast<int64_t>(bounds_at_maximum_scale.width()) *
          static_cast<int64_t>(bounds_at_maximum_scale.height());
      gfx::Size viewport = layer_tree_impl()->GetDeviceViewport().size();

      // Use the square of the maximum viewport dimension direction, to
      // compensate for viewports with different aspect ratios.
      int64_t max_viewport_dimension =
          std::max(static_cast<int64_t>(viewport.width()),
                   static_cast<int64_t>(viewport.height()));
      int64_t squared_viewport_area =
          max_viewport_dimension * max_viewport_dimension;

      if (maximum_area <= squared_viewport_area)
        can_raster_at_maximum_scale = true;
    }
    if (starting_scale != kNotScaled && starting_scale > maximum_scale) {
      gfx::Size bounds_at_starting_scale =
          gfx::ScaleToCeiledSize(raster_source_->GetSize(), starting_scale);
      int64_t start_area =
          static_cast<int64_t>(bounds_at_starting_scale.width()) *
          static_cast<int64_t>(bounds_at_starting_scale.height());
      gfx::Size viewport = layer_tree_impl()->GetDeviceViewport().size();
      int64_t viewport_area = static_cast<int64_t>(viewport.width()) *
                              static_cast<int64_t>(viewport.height());
      if (start_area <= viewport_area)
        should_raster_at_starting_scale = true;
    }
    // Use the computed scales for the raster scale directly, do not try to use
    // the ideal scale here. The current ideal scale may be way too large in the
    // case of an animation with scale, and will be constantly changing.
    if (should_raster_at_starting_scale)
      raster_contents_scale_ = starting_scale;
    else if (can_raster_at_maximum_scale)
      raster_contents_scale_ = maximum_scale;
    else
      raster_contents_scale_ = 1.f * ideal_page_scale_ * ideal_device_scale_;
  }

  // Clamp will-change: transform layers to be at least the native scale.
  if (has_will_change_transform_hint()) {
    float min_desired_scale = raster_device_scale_ * raster_page_scale_;
    if (raster_contents_scale_ < min_desired_scale) {
      raster_contents_scale_ = min_desired_scale;
      raster_page_scale_ = 1.f;
    }
  }

  raster_contents_scale_ =
      std::max(raster_contents_scale_, MinimumContentsScale());
  raster_contents_scale_ =
      std::min(raster_contents_scale_, MaximumContentsScale());
  DCHECK_GE(raster_contents_scale_, MinimumContentsScale());
  DCHECK_LE(raster_contents_scale_, MaximumContentsScale());

  // If this layer would create zero or one tiles at this content scale,
  // don't create a low res tiling.
  gfx::Size raster_bounds =
      gfx::ScaleToCeiledSize(raster_source_->GetSize(), raster_contents_scale_);
  gfx::Size tile_size = CalculateTileSize(raster_bounds);
  bool tile_covers_bounds = tile_size.width() >= raster_bounds.width() &&
                            tile_size.height() >= raster_bounds.height();
  if (tile_size.IsEmpty() || tile_covers_bounds) {
    low_res_raster_contents_scale_ = raster_contents_scale_;
    return;
  }

  float low_res_factor =
      layer_tree_impl()->settings().low_res_contents_scale_factor;
  low_res_raster_contents_scale_ =
      std::max(raster_contents_scale_ * low_res_factor, MinimumContentsScale());
  DCHECK_LE(low_res_raster_contents_scale_, raster_contents_scale_);
  DCHECK_GE(low_res_raster_contents_scale_, MinimumContentsScale());
  DCHECK_LE(low_res_raster_contents_scale_, MaximumContentsScale());
}

void PictureLayerImpl::CleanUpTilingsOnActiveLayer(
    const std::vector<PictureLayerTiling*>& used_tilings) {
  DCHECK(layer_tree_impl()->IsActiveTree());
  if (tilings_->num_tilings() == 0)
    return;

  float min_acceptable_high_res_scale = std::min(
      raster_contents_scale_, ideal_contents_scale_);
  float max_acceptable_high_res_scale = std::max(
      raster_contents_scale_, ideal_contents_scale_);

  PictureLayerImpl* twin = GetPendingOrActiveTwinLayer();
  if (twin && twin->CanHaveTilings()) {
    min_acceptable_high_res_scale =
        std::min({min_acceptable_high_res_scale, twin->raster_contents_scale_,
                  twin->ideal_contents_scale_});
    max_acceptable_high_res_scale =
        std::max({max_acceptable_high_res_scale, twin->raster_contents_scale_,
                  twin->ideal_contents_scale_});
  }

  PictureLayerTilingSet* twin_set = twin ? twin->tilings_.get() : nullptr;
  tilings_->CleanUpTilings(min_acceptable_high_res_scale,
                           max_acceptable_high_res_scale, used_tilings,
                           twin_set);
  DCHECK_GT(tilings_->num_tilings(), 0u);
  SanityCheckTilingState();
}

gfx::Vector2dF PictureLayerImpl::CalculateRasterTranslation(
    float raster_scale) {
  if (!use_transformed_rasterization_)
    return gfx::Vector2dF();

  gfx::Transform draw_transform = DrawTransform();
  // TODO(enne): for performance reasons, we should only have a raster
  // translation when the screen space transform is not animating.  We try to
  // avoid this elsewhere but it still happens: http://crbug.com/778440
  // TODO(enne): Also, we shouldn't ever get here if the draw transform is not
  // just a scale + translation, but we do sometimes: http://crbug.com/740113
  if (draw_properties().screen_space_transform_is_animating ||
      !draw_transform.IsScaleOrTranslation()) {
    // For now, while these problems are not well understood, avoid changing
    // the raster scale in these cases.
    return gfx::Vector2dF();
  }

  // It is only useful to align the content space to the target space if their
  // relative pixel ratio is some small rational number. Currently we only
  // align if the relative pixel ratio is 1:1.
  // Good match if the maximum alignment error on a layer of size 10000px
  // does not exceed 0.001px.
  static constexpr float kErrorThreshold = 0.0000001f;
  if (std::abs(draw_transform.matrix().getFloat(0, 0) - raster_scale) >
          kErrorThreshold ||
      std::abs(draw_transform.matrix().getFloat(1, 1) - raster_scale) >
          kErrorThreshold)
    return gfx::Vector2dF();

  // Extract the fractional part of layer origin in the target space.
  float origin_x = draw_transform.matrix().getFloat(0, 3);
  float origin_y = draw_transform.matrix().getFloat(1, 3);
  return gfx::Vector2dF(origin_x - floorf(origin_x),
                        origin_y - floorf(origin_y));
}

float PictureLayerImpl::MinimumContentsScale() const {
  float setting_min = layer_tree_impl()->settings().minimum_contents_scale;

  // If the contents scale is less than 1 / width (also for height),
  // then it will end up having less than one pixel of content in that
  // dimension.  Bump the minimum contents scale up in this case to prevent
  // this from happening.
  int min_dimension = std::min(raster_source_->GetSize().width(),
                               raster_source_->GetSize().height());
  if (!min_dimension)
    return setting_min;

  return std::max(1.f / min_dimension, setting_min);
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
  raster_source_scale_ = 0.f;
  raster_contents_scale_ = 0.f;
  low_res_raster_contents_scale_ = 0.f;
}

bool PictureLayerImpl::CanHaveTilings() const {
  if (raster_source_->IsSolidColor())
    return false;
  if (!DrawsContent())
    return false;
  if (!raster_source_->HasRecordings())
    return false;
  // If the |raster_source_| has a recording it should have non-empty bounds.
  DCHECK(!raster_source_->GetSize().IsEmpty());
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
  if (layer_tree_impl()->PageScaleTransformNode()) {
    ideal_page_scale_ = IsAffectedByPageScale()
                            ? layer_tree_impl()->current_page_scale_factor()
                            : 1.f;
    ideal_contents_scale_ = GetIdealContentsScale();
  } else {
    // This layer may be in a layer tree embedded in a hierarchy that has its
    // own page scale factor. We represent that here as
    // 'external_page_scale_factor', a value that affects raster scale in the
    // same way that page_scale_factor does, but doesn't affect any geometry
    // calculations.
    float external_page_scale_factor =
        layer_tree_impl() ? layer_tree_impl()->external_page_scale_factor()
                          : 1.f;
    DCHECK(!layer_tree_impl() || external_page_scale_factor == 1.f ||
           layer_tree_impl()->current_page_scale_factor() == 1.f);
    ideal_page_scale_ = external_page_scale_factor;
    ideal_contents_scale_ =
        GetIdealContentsScale() * external_page_scale_factor;
  }
  ideal_contents_scale_ = base::ClampToRange(
      ideal_contents_scale_, min_contents_scale, kMaxIdealContentsScale);
  ideal_source_scale_ =
      ideal_contents_scale_ / ideal_page_scale_ / ideal_device_scale_;
}

void PictureLayerImpl::GetDebugBorderProperties(
    SkColor* color,
    float* width) const {
  float device_scale_factor =
      layer_tree_impl() ? layer_tree_impl()->device_scale_factor() : 1;

  if (is_directly_composited_image_) {
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
  state->SetDouble("ideal_contents_scale", ideal_contents_scale_);
  state->SetDouble("geometry_contents_scale", MaximumTilingContentsScale());
  state->BeginArray("tilings");
  tilings_->AsValueInto(state);
  state->EndArray();

  MathUtil::AddToTracedValue("tile_priority_rect",
                             viewport_rect_for_tile_priority_in_content_space_,
                             state);
  MathUtil::AddToTracedValue("visible_rect", visible_layer_rect(), state);

  state->BeginArray("pictures");
  raster_source_->AsValueInto(state);
  state->EndArray();

  state->BeginArray("invalidation");
  invalidation_.AsValueInto(state);
  state->EndArray();

  state->BeginArray("coverage_tiles");
  for (PictureLayerTilingSet::CoverageIterator iter(
           tilings_.get(), MaximumTilingContentsScale(),
           gfx::Rect(raster_source_->GetSize()), ideal_contents_scale_);
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
  state->SetBoolean("draws_content", DrawsContent());
  state->SetBoolean("raster_source_has_recordings",
                    raster_source_->HasRecordings());
  state->SetDouble("max_contents_scale", MaximumTilingContentsScale());
  state->SetDouble("min_contents_scale", MinimumContentsScale());
  state->EndDictionary();

  state->BeginDictionary("raster_scales");
  state->SetDouble("page_scale", raster_page_scale_);
  state->SetDouble("device_scale", raster_device_scale_);
  state->SetDouble("source_scale", raster_source_scale_);
  state->SetDouble("contents_scale", raster_contents_scale_);
  state->SetDouble("low_res_contents_scale", low_res_raster_contents_scale_);
  state->EndDictionary();

  state->BeginDictionary("ideal_scales");
  state->SetDouble("page_scale", ideal_page_scale_);
  state->SetDouble("device_scale", ideal_device_scale_);
  state->SetDouble("source_scale", ideal_source_scale_);
  state->SetDouble("contents_scale", ideal_contents_scale_);
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
  if (!raster_source_ || !raster_source_->GetDisplayItemList() ||
      raster_source_->GetDisplayItemList()->discardable_image_map().empty()) {
    return ImageInvalidationResult::kNoImages;
  }

  InvalidationRegion image_invalidation;
  for (auto image_id : images_to_invalidate) {
    const auto& rects = raster_source_->GetDisplayItemList()
                            ->discardable_image_map()
                            .GetRectsForImage(image_id);
    for (const auto& r : rects.container())
      image_invalidation.Union(r);
  }
  Region invalidation;
  image_invalidation.Swap(&invalidation);

  if (invalidation.IsEmpty())
    return ImageInvalidationResult::kNoInvalidation;

  // Note: We can use a rect here since this is only used to track damage for a
  // frame and not raster invalidation.
  UnionUpdateRect(invalidation.bounds());

  invalidation_.Union(invalidation);
  tilings_->Invalidate(invalidation);
  // TODO(crbug.com/303943): SetNeedsPushProperties() would be needed here if
  // PictureLayerImpl didn't always push properties every activation.
  return ImageInvalidationResult::kInvalidated;
}

void PictureLayerImpl::SetPaintWorkletRecord(
    scoped_refptr<const PaintWorkletInput> input,
    sk_sp<PaintRecord> record) {
  DCHECK(paint_worklet_records_.find(input) != paint_worklet_records_.end());
  paint_worklet_records_[input].second = std::move(record);
}

void PictureLayerImpl::RegisterAnimatedImages() {
  if (!raster_source_ || !raster_source_->GetDisplayItemList())
    return;

  auto* controller = layer_tree_impl()->image_animation_controller();
  const auto& metadata = raster_source_->GetDisplayItemList()
                             ->discardable_image_map()
                             .animated_images_metadata();
  for (const auto& data : metadata) {
    // Only update the metadata from updated recordings received from a commit.
    if (layer_tree_impl()->IsSyncTree())
      controller->UpdateAnimatedImage(data);
    controller->RegisterAnimationDriver(data.paint_image_id, this);
  }
}

void PictureLayerImpl::UnregisterAnimatedImages() {
  if (!raster_source_ || !raster_source_->GetDisplayItemList())
    return;

  auto* controller = layer_tree_impl()->image_animation_controller();
  const auto& metadata = raster_source_->GetDisplayItemList()
                             ->discardable_image_map()
                             .animated_images_metadata();
  for (const auto& data : metadata)
    controller->UnregisterAnimationDriver(data.paint_image_id, this);
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
    const PaintWorkletInput::PropertyKey& key) {
  for (auto& entry : paint_worklet_records_) {
    const std::vector<PaintWorkletInput::PropertyKey>& prop_ids =
        entry.first->GetPropertyKeys();
    // If the PaintWorklet depends on the property whose value was changed by
    // the animation system, then invalidate its associated PaintRecord so that
    // we can repaint the PaintWorklet during impl side invalidation.
    if (base::Contains(prop_ids, key))
      entry.second.second = nullptr;
  }
}

std::unique_ptr<base::DictionaryValue> PictureLayerImpl::LayerAsJson() const {
  auto result = LayerImpl::LayerAsJson();
  auto dictionary = std::make_unique<base::DictionaryValue>();
  if (raster_source_) {
    dictionary->SetBoolean("IsSolidColor", raster_source_->IsSolidColor());
    auto list = std::make_unique<base::ListValue>();
    list->AppendInteger(raster_source_->GetSize().width());
    list->AppendInteger(raster_source_->GetSize().height());
    dictionary->Set("Size", std::move(list));
    dictionary->SetBoolean("HasRecordings", raster_source_->HasRecordings());

    const auto& display_list = raster_source_->GetDisplayItemList();
    size_t op_count = display_list ? display_list->TotalOpCount() : 0;
    size_t bytes_used = display_list ? display_list->BytesUsed() : 0;
    dictionary->SetInteger("OpCount", op_count);
    dictionary->SetInteger("BytesUsed", bytes_used);
  }
  result->Set("RasterSource", std::move(dictionary));
  return result;
}

}  // namespace cc
