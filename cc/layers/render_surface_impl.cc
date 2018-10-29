// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/render_surface_impl.h"

#include <stddef.h>

#include <algorithm>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "cc/base/math_util.h"
#include "cc/debug/debug_colors.h"
#include "cc/layers/append_quads_data.h"
#include "cc/paint/filter_operations.h"
#include "cc/trees/damage_tracker.h"
#include "cc/trees/draw_property_utils.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/occlusion.h"
#include "cc/trees/transform_node.h"
#include "components/viz/common/quads/content_draw_quad_base.h"
#include "components/viz/common/quads/debug_border_draw_quad.h"
#include "components/viz/common/quads/render_pass.h"
#include "components/viz/common/quads/render_pass_draw_quad.h"
#include "components/viz/common/quads/shared_quad_state.h"
#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "third_party/skia/include/core/SkImageFilter.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/transform.h"

namespace cc {

RenderSurfaceImpl::RenderSurfaceImpl(LayerTreeImpl* layer_tree_impl,
                                     uint64_t stable_id)
    : layer_tree_impl_(layer_tree_impl),
      stable_id_(stable_id),
      effect_tree_index_(EffectTree::kInvalidNodeId),
      num_contributors_(0),
      has_contributing_layer_that_escapes_clip_(false),
      surface_property_changed_(false),
      ancestor_property_changed_(false),
      contributes_to_drawn_surface_(false),
      is_render_surface_list_member_(false),
      nearest_occlusion_immune_ancestor_(nullptr) {
  damage_tracker_ = DamageTracker::Create();
}

RenderSurfaceImpl::~RenderSurfaceImpl() = default;

RenderSurfaceImpl* RenderSurfaceImpl::render_target() {
  EffectTree& effect_tree = layer_tree_impl_->property_trees()->effect_tree;
  EffectNode* node = effect_tree.Node(EffectTreeIndex());
  if (node->target_id != EffectTree::kRootNodeId)
    return effect_tree.GetRenderSurface(node->target_id);
  else
    return this;
}

const RenderSurfaceImpl* RenderSurfaceImpl::render_target() const {
  const EffectTree& effect_tree =
      layer_tree_impl_->property_trees()->effect_tree;
  const EffectNode* node = effect_tree.Node(EffectTreeIndex());
  if (node->target_id != EffectTree::kRootNodeId)
    return effect_tree.GetRenderSurface(node->target_id);
  else
    return this;
}

RenderSurfaceImpl::DrawProperties::DrawProperties() {
  draw_opacity = 1.f;
  is_clipped = false;
}

RenderSurfaceImpl::DrawProperties::~DrawProperties() = default;

gfx::RectF RenderSurfaceImpl::DrawableContentRect() const {
  if (content_rect().IsEmpty())
    return gfx::RectF();

  gfx::Rect surface_content_rect = content_rect();
  const FilterOperations& filters = Filters();
  if (!filters.IsEmpty()) {
    surface_content_rect =
        filters.MapRect(surface_content_rect, SurfaceScale().matrix());
  }
  gfx::RectF drawable_content_rect = MathUtil::MapClippedRect(
      draw_transform(), gfx::RectF(surface_content_rect));
  if (!filters.IsEmpty() && is_clipped()) {
    // Filter could move pixels around, but still need to be clipped.
    drawable_content_rect.Intersect(gfx::RectF(clip_rect()));
  }

  // If the rect has a NaN coordinate, we return empty rect to avoid crashes in
  // functions (for example, gfx::ToEnclosedRect) that are called on this rect.
  if (std::isnan(drawable_content_rect.x()) ||
      std::isnan(drawable_content_rect.y()) ||
      std::isnan(drawable_content_rect.right()) ||
      std::isnan(drawable_content_rect.bottom()))
    return gfx::RectF();

  return drawable_content_rect;
}

SkBlendMode RenderSurfaceImpl::BlendMode() const {
  return OwningEffectNode()->blend_mode;
}

bool RenderSurfaceImpl::UsesDefaultBlendMode() const {
  return BlendMode() == SkBlendMode::kSrcOver;
}

SkColor RenderSurfaceImpl::GetDebugBorderColor() const {
  return DebugColors::SurfaceBorderColor();
}

float RenderSurfaceImpl::GetDebugBorderWidth() const {
  return DebugColors::SurfaceBorderWidth(
      layer_tree_impl_ ? layer_tree_impl_->device_scale_factor() : 1);
}

LayerImpl* RenderSurfaceImpl::MaskLayer() {
  int mask_layer_id = OwningEffectNode()->mask_layer_id;
  return layer_tree_impl_->LayerById(mask_layer_id);
}

bool RenderSurfaceImpl::HasMask() const {
  return OwningEffectNode()->mask_layer_id != Layer::INVALID_ID;
}

bool RenderSurfaceImpl::HasMaskingContributingSurface() const {
  return OwningEffectNode()->has_masking_child;
}

const FilterOperations& RenderSurfaceImpl::Filters() const {
  return OwningEffectNode()->filters;
}

gfx::PointF RenderSurfaceImpl::FiltersOrigin() const {
  return OwningEffectNode()->filters_origin;
}

gfx::Transform RenderSurfaceImpl::SurfaceScale() const {
  gfx::Transform surface_scale;
  surface_scale.Scale(OwningEffectNode()->surface_contents_scale.x(),
                      OwningEffectNode()->surface_contents_scale.y());
  return surface_scale;
}

const FilterOperations& RenderSurfaceImpl::BackdropFilters() const {
  return OwningEffectNode()->backdrop_filters;
}

bool RenderSurfaceImpl::TrilinearFiltering() const {
  return OwningEffectNode()->trilinear_filtering;
}

bool RenderSurfaceImpl::HasCopyRequest() const {
  return OwningEffectNode()->has_copy_request;
}

bool RenderSurfaceImpl::ShouldCacheRenderSurface() const {
  return OwningEffectNode()->cache_render_surface;
}

int RenderSurfaceImpl::TransformTreeIndex() const {
  return OwningEffectNode()->transform_id;
}

int RenderSurfaceImpl::ClipTreeIndex() const {
  return OwningEffectNode()->clip_id;
}

int RenderSurfaceImpl::EffectTreeIndex() const {
  return effect_tree_index_;
}

const EffectNode* RenderSurfaceImpl::OwningEffectNode() const {
  return layer_tree_impl_->property_trees()->effect_tree.Node(
      EffectTreeIndex());
}

void RenderSurfaceImpl::SetClipRect(const gfx::Rect& clip_rect) {
  if (clip_rect == draw_properties_.clip_rect)
    return;

  surface_property_changed_ = true;
  draw_properties_.clip_rect = clip_rect;
}

void RenderSurfaceImpl::SetContentRect(const gfx::Rect& content_rect) {
  if (content_rect == draw_properties_.content_rect)
    return;

  surface_property_changed_ = true;
  draw_properties_.content_rect = content_rect;
}

void RenderSurfaceImpl::SetContentRectForTesting(const gfx::Rect& rect) {
  SetContentRect(rect);
}

gfx::Rect RenderSurfaceImpl::CalculateExpandedClipForFilters(
    const gfx::Transform& target_to_surface) {
  gfx::Rect clip_in_surface_space =
      MathUtil::ProjectEnclosingClippedRect(target_to_surface, clip_rect());
  gfx::Rect expanded_clip_in_surface_space =
      Filters().MapRectReverse(clip_in_surface_space, SurfaceScale().matrix());
  gfx::Rect expanded_clip_in_target_space = MathUtil::MapEnclosingClippedRect(
      draw_transform(), expanded_clip_in_surface_space);
  return expanded_clip_in_target_space;
}

gfx::Rect RenderSurfaceImpl::CalculateClippedAccumulatedContentRect() {
  if (ShouldCacheRenderSurface() || HasCopyRequest() || !is_clipped())
    return accumulated_content_rect();

  if (accumulated_content_rect().IsEmpty())
    return gfx::Rect();

  // Calculate projection from the target surface rect to local
  // space. Non-invertible draw transforms means no able to bring clipped rect
  // in target space back to local space, early out without clip.
  gfx::Transform target_to_surface(gfx::Transform::kSkipInitialization);
  if (!draw_transform().GetInverse(&target_to_surface))
    return accumulated_content_rect();

  // Clip rect is in target space. Bring accumulated content rect to
  // target space in preparation for clipping.
  gfx::Rect accumulated_rect_in_target_space =
      MathUtil::MapEnclosingClippedRect(draw_transform(),
                                        accumulated_content_rect());
  // If accumulated content rect is contained within clip rect, early out
  // without clipping.
  if (clip_rect().Contains(accumulated_rect_in_target_space))
    return accumulated_content_rect();

  gfx::Rect clipped_accumulated_rect_in_target_space;
  if (Filters().HasFilterThatMovesPixels()) {
    clipped_accumulated_rect_in_target_space =
        CalculateExpandedClipForFilters(target_to_surface);
  } else {
    clipped_accumulated_rect_in_target_space = clip_rect();
  }
  clipped_accumulated_rect_in_target_space.Intersect(
      accumulated_rect_in_target_space);

  if (clipped_accumulated_rect_in_target_space.IsEmpty())
    return gfx::Rect();

  gfx::Rect clipped_accumulated_rect_in_local_space =
      MathUtil::ProjectEnclosingClippedRect(
          target_to_surface, clipped_accumulated_rect_in_target_space);
  // Bringing clipped accumulated rect back to local space may result
  // in inflation due to axis-alignment.
  clipped_accumulated_rect_in_local_space.Intersect(accumulated_content_rect());
  return clipped_accumulated_rect_in_local_space;
}

void RenderSurfaceImpl::CalculateContentRectFromAccumulatedContentRect(
    int max_texture_size) {
  // Root render surface use viewport, and does not calculate content rect.
  DCHECK_NE(render_target(), this);

  // Surface's content rect is the clipped accumulated content rect. By default
  // use accumulated content rect, and then try to clip it.
  gfx::Rect surface_content_rect = CalculateClippedAccumulatedContentRect();

  // The RenderSurfaceImpl backing texture cannot exceed the maximum supported
  // texture size.
  surface_content_rect.set_width(
      std::min(surface_content_rect.width(), max_texture_size));
  surface_content_rect.set_height(
      std::min(surface_content_rect.height(), max_texture_size));

  SetContentRect(surface_content_rect);
}

void RenderSurfaceImpl::SetContentRectToViewport() {
  // Only root render surface use viewport as content rect.
  DCHECK_EQ(render_target(), this);
  gfx::Rect viewport = gfx::ToEnclosingRect(
      layer_tree_impl_->property_trees()->clip_tree.ViewportClip());
  SetContentRect(viewport);
}

void RenderSurfaceImpl::ClearAccumulatedContentRect() {
  accumulated_content_rect_ = gfx::Rect();
}

void RenderSurfaceImpl::AccumulateContentRectFromContributingLayer(
    LayerImpl* layer) {
  DCHECK(layer->DrawsContent());
  DCHECK_EQ(this, layer->render_target());

  // Root render surface doesn't accumulate content rect, it always uses
  // viewport for content rect.
  if (render_target() == this)
    return;

  accumulated_content_rect_.Union(layer->drawable_content_rect());
}

void RenderSurfaceImpl::AccumulateContentRectFromContributingRenderSurface(
    RenderSurfaceImpl* contributing_surface) {
  DCHECK_NE(this, contributing_surface);
  DCHECK_EQ(this, contributing_surface->render_target());

  // Root render surface doesn't accumulate content rect, it always uses
  // viewport for content rect.
  if (render_target() == this)
    return;

  // The content rect of contributing surface is in its own space. Instead, we
  // will use contributing surface's DrawableContentRect which is in target
  // space (local space for this render surface) as required.
  accumulated_content_rect_.Union(
      gfx::ToEnclosedRect(contributing_surface->DrawableContentRect()));
}

bool RenderSurfaceImpl::SurfacePropertyChanged() const {
  // Surface property changes are tracked as follows:
  //
  // - surface_property_changed_ is flagged when the clip_rect or content_rect
  //   change. As of now, these are the only two properties that can be affected
  //   by descendant layers.
  //
  // - all other property changes come from the surface's property tree nodes
  //   (or some ancestor node that propagates its change to one of these nodes).
  //
  return surface_property_changed_ || AncestorPropertyChanged();
}

bool RenderSurfaceImpl::SurfacePropertyChangedOnlyFromDescendant() const {
  return surface_property_changed_ && !AncestorPropertyChanged();
}

bool RenderSurfaceImpl::AncestorPropertyChanged() const {
  const PropertyTrees* property_trees = layer_tree_impl_->property_trees();
  return ancestor_property_changed_ || property_trees->full_tree_damaged ||
         property_trees->transform_tree.Node(TransformTreeIndex())
             ->transform_changed ||
         property_trees->effect_tree.Node(EffectTreeIndex())->effect_changed;
}

void RenderSurfaceImpl::NoteAncestorPropertyChanged() {
  ancestor_property_changed_ = true;
}

bool RenderSurfaceImpl::HasDamageFromeContributingContent() const {
  return damage_tracker_->has_damage_from_contributing_content();
}

gfx::Rect RenderSurfaceImpl::GetDamageRect() const {
  gfx::Rect damage_rect;
  bool is_valid_rect = damage_tracker_->GetDamageRectIfValid(&damage_rect);
  if (!is_valid_rect)
    return content_rect();
  return damage_rect;
}

void RenderSurfaceImpl::ResetPropertyChangedFlags() {
  surface_property_changed_ = false;
  ancestor_property_changed_ = false;
}

std::unique_ptr<viz::RenderPass> RenderSurfaceImpl::CreateRenderPass() {
  std::unique_ptr<viz::RenderPass> pass =
      viz::RenderPass::Create(num_contributors_);
  gfx::Rect damage_rect = GetDamageRect();
  damage_rect.Intersect(content_rect());
  pass->SetNew(id(), content_rect(), damage_rect,
               draw_properties_.screen_space_transform);
  pass->filters = Filters();
  pass->backdrop_filters = BackdropFilters();
  pass->generate_mipmap = TrilinearFiltering();
  pass->cache_render_pass = ShouldCacheRenderSurface();
  pass->has_damage_from_contributing_content =
      HasDamageFromeContributingContent();
  return pass;
}

void RenderSurfaceImpl::AppendQuads(DrawMode draw_mode,
                                    viz::RenderPass* render_pass,
                                    AppendQuadsData* append_quads_data) {
  gfx::Rect unoccluded_content_rect =
      occlusion_in_content_space().GetUnoccludedContentRect(content_rect());
  if (unoccluded_content_rect.IsEmpty())
    return;

  const PropertyTrees* property_trees = layer_tree_impl_->property_trees();
  int sorting_context_id =
      property_trees->transform_tree.Node(TransformTreeIndex())
          ->sorting_context_id;
  bool contents_opaque = false;
  viz::SharedQuadState* shared_quad_state =
      render_pass->CreateAndAppendSharedQuadState();
  shared_quad_state->SetAll(
      draw_transform(), content_rect(), content_rect(),
      draw_properties_.clip_rect, draw_properties_.is_clipped, contents_opaque,
      draw_properties_.draw_opacity, BlendMode(), sorting_context_id);

  if (layer_tree_impl_->debug_state().show_debug_borders.test(
          DebugBorderType::RENDERPASS)) {
    auto* debug_border_quad =
        render_pass->CreateAndAppendDrawQuad<viz::DebugBorderDrawQuad>();
    debug_border_quad->SetNew(shared_quad_state, content_rect(),
                              unoccluded_content_rect, GetDebugBorderColor(),
                              GetDebugBorderWidth());
  }

  viz::ResourceId mask_resource_id = 0;
  gfx::Size mask_texture_size;
  gfx::RectF mask_uv_rect;
  gfx::Vector2dF surface_contents_scale =
      OwningEffectNode()->surface_contents_scale;
  PictureLayerImpl* mask_layer = static_cast<PictureLayerImpl*>(MaskLayer());
  // Resourceless mode does not support masks.
  if (draw_mode != DRAW_MODE_RESOURCELESS_SOFTWARE && mask_layer &&
      mask_layer->DrawsContent() && !mask_layer->bounds().IsEmpty()) {
    // The software renderer applies mask layer and blending in the wrong
    // order but kDstIn doesn't commute with masking. It is okay to not
    // support this configuration because kDstIn was introduced to replace
    // mask layers.
    DCHECK(BlendMode() != SkBlendMode::kDstIn)
        << "kDstIn blend mode with mask layer is unsupported.";
    TRACE_EVENT1("cc", "RenderSurfaceImpl::AppendQuads",
                 "mask_layer_gpu_memory_usage",
                 mask_layer->GPUMemoryUsageInBytes());

    int64_t visible_geometry_area =
        static_cast<int64_t>(unoccluded_content_rect.width()) *
        unoccluded_content_rect.height();
    append_quads_data->num_mask_layers++;
    append_quads_data->visible_mask_layer_area += visible_geometry_area;
    if (mask_layer->is_rounded_corner_mask()) {
      append_quads_data->num_rounded_corner_mask_layers++;
      append_quads_data->visible_rounded_corner_mask_layer_area +=
          visible_geometry_area;
    }

    if (mask_layer->mask_type() == Layer::LayerMaskType::MULTI_TEXTURE_MASK) {
      TileMaskLayer(render_pass, shared_quad_state, unoccluded_content_rect);
      return;
    }
    gfx::SizeF mask_uv_size;
    mask_layer->GetContentsResourceId(&mask_resource_id, &mask_texture_size,
                                      &mask_uv_size);
    gfx::SizeF unclipped_mask_target_size = gfx::ScaleSize(
        gfx::SizeF(OwningEffectNode()->unscaled_mask_target_size),
        surface_contents_scale.x(), surface_contents_scale.y());
    // Convert content_rect from target space to normalized mask UV space.
    // Where |unclipped_mask_target_size| maps to |mask_uv_size|.
    mask_uv_rect = gfx::ScaleRect(
        gfx::RectF(content_rect()),
        mask_uv_size.width() / unclipped_mask_target_size.width(),
        mask_uv_size.height() / unclipped_mask_target_size.height());
  }

  gfx::RectF tex_coord_rect(gfx::Rect(content_rect().size()));
  auto* quad = render_pass->CreateAndAppendDrawQuad<viz::RenderPassDrawQuad>();
  quad->SetNew(shared_quad_state, content_rect(), unoccluded_content_rect, id(),
               mask_resource_id, mask_uv_rect, mask_texture_size,
               surface_contents_scale, FiltersOrigin(), tex_coord_rect,
               !layer_tree_impl_->settings().enable_edge_anti_aliasing);
}

void RenderSurfaceImpl::TileMaskLayer(
    viz::RenderPass* render_pass,
    viz::SharedQuadState* shared_quad_state,
    const gfx::Rect& unoccluded_content_rect) {
  DCHECK(MaskLayer());
  DCHECK(Filters().IsEmpty());

  LayerImpl* mask_layer = MaskLayer();
  gfx::Vector2dF owning_layer_to_surface_contents_scale =
      OwningEffectNode()->surface_contents_scale;

  // Use the picture layer's AppendQuads logic to generate TileDrawQuads. These
  // DrawQuads are used to generate tiled RenderPassDrawQuad for the mask.
  std::unique_ptr<viz::RenderPass> temp_render_pass = viz::RenderPass::Create();
  AppendQuadsData temp_append_quads_data;
  mask_layer->AppendQuads(temp_render_pass.get(), &temp_append_quads_data);

  auto* temp_quad = temp_render_pass->quad_list.front();
  if (!temp_quad)
    return;

  // There are two spaces we are dealing with here:
  // 1. quad space: This is the space where the draw quads generated by the
  // PictureLayerImpl's logic are in. In other words, this is the space where
  // the |temp_quad|'s rect is in.
  // 2. surface space: This is the contents space of |this| render surface.
  // Since |mask_layer|'s target is the render surface it's masking, the surface
  // space is also the target space for the quads generated by
  // PictureLayerImpl's logic.

  gfx::Transform quad_space_to_surface_space_transform =
      temp_quad->shared_quad_state->quad_to_target_transform;
  // This transform should be a 2d scale + offset, so would be invertible.
  gfx::Transform surface_space_to_quad_space_transform;
  bool invertible = quad_space_to_surface_space_transform.GetInverse(
      &surface_space_to_quad_space_transform);
  DCHECK(invertible) << "RenderSurfaceImpl::TileMaskLayer created quads with "
                        "non-invertible transform.";

  // While converting from the TileDrawQuads to RenderPassDrawQuads, we keep the
  // quad rects in the same space, and modify every other rect that is not in
  // quad space accordingly.

  // The |shared_quad_state| being passed in is generated with |this| render
  // surface's draw properties. It holds a transform from the surface contents
  // space to the surface target space. We want to change the origin space to
  // match the |mask_layer|'s quad space, so we must include the transform from
  // the quad space to the surface contents space. Then the transform is from
  // the |mask_layer|'s quad space to our target space.
  shared_quad_state->quad_to_target_transform.PreconcatTransform(
      quad_space_to_surface_space_transform);

  // Next, we need to modify the rects on |shared_quad_state| that are in
  // surface's "quad space" (surface space) to quad space.
  shared_quad_state->quad_layer_rect = MathUtil::ProjectEnclosingClippedRect(
      surface_space_to_quad_space_transform,
      shared_quad_state->quad_layer_rect);
  shared_quad_state->visible_quad_layer_rect =
      MathUtil::ProjectEnclosingClippedRect(
          surface_space_to_quad_space_transform,
          shared_quad_state->visible_quad_layer_rect);

  // The |shared_quad_state|'s |quad_layer_rect| and |visible_quad_layer_rect|
  // is set from content_rect(). content_rect() defines the size of the source
  // texture to be masked. PictureLayerImpl's generated |quad_layer_rect| and
  // |visible_quad_layer_rect| is from the mask layer's |bounds| and
  // |visible_layer_rect|. These rect defines the size of the mask texture. The
  // intersection of the two rects is the rect we can draw.
  shared_quad_state->quad_layer_rect.Intersect(
      temp_quad->shared_quad_state->quad_layer_rect);
  shared_quad_state->visible_quad_layer_rect.Intersect(
      temp_quad->shared_quad_state->visible_quad_layer_rect);

  // Cache content_rect() and |unoccluded_content_rect| in quad space.
  gfx::Rect content_rect_in_quad_space = MathUtil::MapEnclosingClippedRect(
      surface_space_to_quad_space_transform, content_rect());
  gfx::Rect unoccluded_content_rect_in_quad_space =
      MathUtil::MapEnclosingClippedRect(surface_space_to_quad_space_transform,
                                        unoccluded_content_rect);

  // Generate RenderPassDrawQuads based on the temporary quads created by
  // |mask_layer|.
  for (auto* temp_quad : temp_render_pass->quad_list) {
    gfx::Rect temp_quad_rect = temp_quad->rect;
    // If the |temp_quad_rect| is entirely outside render surface's
    // content_rect(), ignore the quad.
    if (!temp_quad_rect.Intersects(content_rect_in_quad_space))
      continue;

    // We only care about the quads that are inside the content_rect().
    gfx::Rect quad_rect =
        gfx::IntersectRects(temp_quad_rect, content_rect_in_quad_space);

    gfx::Rect visible_quad_rect =
        gfx::IntersectRects(quad_rect, unoccluded_content_rect_in_quad_space);
    if (visible_quad_rect.IsEmpty())
      continue;

    // |tex_coord_rect| is non-normalized sub-rect of the render surface's
    // texture that is being masked. Its origin is (0,0) and it is in surface
    // space. For example the |tex_coord_rect| for the entire texture would be
    // (0,0 content_rect.width X content_rect.height).

    // In order to calculate the |tex_coord_rect|, we calculate what quad's rect
    // would be masking in the surface contents space, then remove the offset.
    gfx::RectF tex_coord_rect = MathUtil::MapClippedRect(
        quad_space_to_surface_space_transform, gfx::RectF(quad_rect));
    tex_coord_rect.Offset(-content_rect().OffsetFromOrigin());

    switch (temp_quad->material) {
      case viz::DrawQuad::TILED_CONTENT: {
        DCHECK_EQ(1U, temp_quad->resources.count);
        // When the |temp_quad| is actually a texture, we need to calculate
        // |mask_uv_rect|. The |mask_uv_rect| is the normalized sub-rect for
        // applying the mask's texture. To get |mask_uv_rect|, we need the newly
        // calculated |quad_rect| in the texture's space, then normalized by the
        // texture's size.

        // We are applying the |temp_quad|'s texture as a mask, so we start with
        // the |tex_coord_rect| of the |temp_quad|.
        gfx::RectF temp_tex_coord_rect =
            viz::TileDrawQuad::MaterialCast(temp_quad)->tex_coord_rect;

        // The |quad_rect| is in the same space as |temp_quad_rect|. Calculate
        // the scale transform between the texture space and the quad space.
        float scale_x = temp_tex_coord_rect.width() / temp_quad_rect.width();
        float scale_y = temp_tex_coord_rect.height() / temp_quad_rect.height();
        // Start by setting up the correct size of mask_uv_rect in texture
        // space.
        gfx::RectF mask_uv_rect(quad_rect.width() * scale_x,
                                quad_rect.height() * scale_y);

        // Now figure out what is the correct offset. Start with the original
        // temp_tex_coord_rect's offset.
        mask_uv_rect.Offset(temp_tex_coord_rect.OffsetFromOrigin());
        // Next figure out what offset to apply by checking the difference in
        // offset between |temp_quad_rect| and |quad_rect| which is
        // intersected by the content_rect().
        gfx::Vector2dF offset(quad_rect.OffsetFromOrigin());
        offset -= temp_quad_rect.OffsetFromOrigin();
        // Convert the difference in offset into texture space.
        offset.Scale(scale_x, scale_y);
        mask_uv_rect.Offset(offset);

        // |mask_uv_rect| is normalized to [0..1] by the |mask_texture_size|.
        gfx::Size mask_texture_size =
            viz::TileDrawQuad::MaterialCast(temp_quad)->texture_size;
        mask_uv_rect.Scale(1.f / mask_texture_size.width(),
                           1.f / mask_texture_size.height());

        auto* quad =
            render_pass->CreateAndAppendDrawQuad<viz::RenderPassDrawQuad>();
        quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect, id(),
                     temp_quad->resources.ids[0], mask_uv_rect,
                     mask_texture_size, owning_layer_to_surface_contents_scale,
                     FiltersOrigin(), tex_coord_rect,
                     !layer_tree_impl_->settings().enable_edge_anti_aliasing);
      } break;
      case viz::DrawQuad::SOLID_COLOR: {
        SkColor temp_color =
            viz::SolidColorDrawQuad::MaterialCast(temp_quad)->color;
        // Check the alpha channel of the color. We apply the mask by
        // multiplying with the alpha channel, so if the alpha channel is
        // transparent, we skip this quad.
        if (SkColorGetA(temp_color) == SK_AlphaTRANSPARENT)
          continue;
        SkAlpha solid = SK_AlphaOPAQUE;
        DCHECK_EQ(SkColorGetA(temp_color), solid);

        auto* quad =
            render_pass->CreateAndAppendDrawQuad<viz::RenderPassDrawQuad>();
        quad->SetNew(shared_quad_state, quad_rect, visible_quad_rect, id(), 0,
                     gfx::RectF(), gfx::Size(),
                     owning_layer_to_surface_contents_scale, FiltersOrigin(),
                     tex_coord_rect,
                     !layer_tree_impl_->settings().enable_edge_anti_aliasing);
      } break;
      case viz::DrawQuad::DEBUG_BORDER:
        NOTIMPLEMENTED();
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

}  // namespace cc
