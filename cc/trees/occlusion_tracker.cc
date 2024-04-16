// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/occlusion_tracker.h"

#include <stddef.h>

#include <algorithm>

#include "cc/base/math_util.h"
#include "cc/base/region.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

OcclusionTracker::OcclusionTracker(const gfx::Rect& screen_space_clip_rect)
    : screen_space_clip_rect_(screen_space_clip_rect) {}

OcclusionTracker::~OcclusionTracker() = default;

Occlusion OcclusionTracker::GetCurrentOcclusionForLayer(
    const gfx::Transform& draw_transform) const {
  DCHECK(!stack_.empty());
  const StackObject& back = stack_.back();
  return Occlusion(draw_transform, back.occlusion_from_outside_target,
                   back.occlusion_from_inside_target);
}

Occlusion OcclusionTracker::GetCurrentOcclusionForContributingSurface(
    const gfx::Transform& draw_transform) const {
  DCHECK(!stack_.empty());
  if (stack_.size() < 2 || stack_.back().ignores_parent_occlusion)
    return Occlusion();
  // A contributing surface doesn't get occluded by things inside its own
  // surface, so only things outside the surface can occlude it. That occlusion
  // is found just below the top of the stack (if it exists).
  const StackObject& second_last = stack_[stack_.size() - 2];
  return Occlusion(draw_transform, second_last.occlusion_from_outside_target,
                   second_last.occlusion_from_inside_target);
}

const RenderSurfaceImpl*
OcclusionTracker::OcclusionSurfaceForContributingSurface() const {
  if (stack_.size() < 2 || stack_.back().ignores_parent_occlusion)
    return nullptr;
  // A contributing surface doesn't get occluded by things inside its own
  // surface, so only things outside the surface can occlude it. That occlusion
  // is found just below the top of the stack (if it exists).
  return stack_[stack_.size() - 2].target;
}

void OcclusionTracker::EnterLayer(
    const EffectTreeLayerListIterator::Position& iterator) {
  RenderSurfaceImpl* render_target = iterator.target_render_surface;

  if (iterator.state == EffectTreeLayerListIterator::State::kLayer) {
    EnterRenderTarget(render_target);
  } else if (iterator.state ==
             EffectTreeLayerListIterator::State::kTargetSurface) {
    FinishedRenderTarget(render_target);
  }
}

void OcclusionTracker::LeaveLayer(
    const EffectTreeLayerListIterator::Position& iterator) {
  RenderSurfaceImpl* render_target = iterator.target_render_surface;

  if (iterator.state == EffectTreeLayerListIterator::State::kLayer) {
    MarkOccludedBehindLayer(iterator.current_layer);
  }
  // TODO(danakj): This should be done when entering the contributing surface,
  // but in a way that the surface's own occlusion won't occlude itself.
  else if (iterator.state ==
           EffectTreeLayerListIterator::State::kContributingSurface) {
    LeaveToRenderTarget(render_target);
  }
}

static gfx::Rect ScreenSpaceClipRectInTargetSurface(
    const RenderSurfaceImpl* target_surface,
    const gfx::Rect& screen_space_clip_rect) {
  gfx::Transform inverse_screen_space_transform;
  if (!target_surface->screen_space_transform().GetInverse(
          &inverse_screen_space_transform))
    return target_surface->content_rect();

  return MathUtil::ProjectEnclosingClippedRect(inverse_screen_space_transform,
                                               screen_space_clip_rect);
}

static SimpleEnclosedRegion TransformSurfaceOpaqueRegion(
    const SimpleEnclosedRegion& region,
    bool have_clip_rect,
    const gfx::Rect& clip_rect_in_new_target,
    const gfx::Transform& transform) {
  if (region.IsEmpty())
    return region;

  // Verify that rects within the |surface| will remain rects in its target
  // surface after applying |transform|. If this is true, then apply |transform|
  // to each rect within |region| in order to transform the entire Region.

  // TODO(danakj): Find a rect interior to each transformed quad.
  if (!transform.NonDegeneratePreserves2dAxisAlignment())
    return SimpleEnclosedRegion();

  SimpleEnclosedRegion transformed_region;
  for (size_t i = 0; i < region.GetRegionComplexity(); ++i) {
    gfx::Rect transformed_rect =
        MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(transform,
                                                            region.GetRect(i));
    if (have_clip_rect)
      transformed_rect.Intersect(clip_rect_in_new_target);
    transformed_region.Union(transformed_rect);
  }
  return transformed_region;
}

void OcclusionTracker::EnterRenderTarget(
    const RenderSurfaceImpl* new_target_surface) {
  DCHECK(new_target_surface);
  if (!stack_.empty() && stack_.back().target == new_target_surface)
    return;

  const RenderSurfaceImpl* old_target_surface = nullptr;
  const RenderSurfaceImpl* old_occlusion_immune_ancestor = nullptr;
  if (!stack_.empty()) {
    old_target_surface = stack_.back().target;
    old_occlusion_immune_ancestor =
        old_target_surface->nearest_occlusion_immune_ancestor();
  }
  const RenderSurfaceImpl* new_occlusion_immune_ancestor =
      new_target_surface->nearest_occlusion_immune_ancestor();

  stack_.emplace_back(new_target_surface);

  // We copy the screen occlusion into the new RenderSurfaceImpl subtree, but we
  // never copy in the occlusion from inside the target, since we are looking
  // at a new RenderSurfaceImpl target.

  // If entering an unoccluded subtree, do not carry forward the outside
  // occlusion calculated so far.
  bool entering_unoccluded_subtree =
      new_occlusion_immune_ancestor &&
      new_occlusion_immune_ancestor != old_occlusion_immune_ancestor;

  gfx::Transform inverse_new_target_screen_space_transform;
  bool have_transform_from_screen_to_new_target =
      new_target_surface->screen_space_transform().GetInverse(
          &inverse_new_target_screen_space_transform);

  bool entering_root_target =
      new_target_surface->render_target() == new_target_surface;

  bool copy_outside_occlusion_forward =
      stack_.size() > 1 && !entering_unoccluded_subtree &&
      have_transform_from_screen_to_new_target && !entering_root_target;
  if (!copy_outside_occlusion_forward) {
    stack_.back().ignores_parent_occlusion = true;
    return;
  }

  size_t last_index = stack_.size() - 1;
  gfx::Transform old_target_to_new_target_transform =
      inverse_new_target_screen_space_transform *
      old_target_surface->screen_space_transform();
  stack_[last_index].occlusion_from_outside_target =
      TransformSurfaceOpaqueRegion(
          stack_[last_index - 1].occlusion_from_outside_target, false,
          gfx::Rect(), old_target_to_new_target_transform);
  stack_[last_index].occlusion_from_outside_target.Union(
      TransformSurfaceOpaqueRegion(
          stack_[last_index - 1].occlusion_from_inside_target, false,
          gfx::Rect(), old_target_to_new_target_transform));
}

// A blend mode is occluding if a fully opaque source can fully occlude the
// destination and the result is also fully opaque.
static bool IsOccludingBlendMode(SkBlendMode blend_mode) {
  return blend_mode == SkBlendMode::kSrc || blend_mode == SkBlendMode::kSrcOver;
}

void OcclusionTracker::FinishedRenderTarget(
    const RenderSurfaceImpl* finished_target_surface) {
  // Make sure we know about the target surface.
  EnterRenderTarget(finished_target_surface);

  bool is_hidden =
      finished_target_surface->OwningEffectNode()->screen_space_opacity == 0.f;

  // Readbacks always happen on render targets so we only need to check
  // for readbacks here.
  bool target_is_only_for_copy_request_or_force_render_surface =
      is_hidden && finished_target_surface->CopyOfOutputRequired();

  // If the occlusion within the surface can not be applied to things outside of
  // the surface's subtree, then clear the occlusion here so it won't be used.
  if (finished_target_surface->HasMaskingContributingSurface() ||
      finished_target_surface->draw_opacity() < 1 ||
      !IsOccludingBlendMode(finished_target_surface->BlendMode()) ||
      target_is_only_for_copy_request_or_force_render_surface ||
      finished_target_surface->Filters().HasFilterThatAffectsOpacity() ||
      finished_target_surface->OwningEffectNode()
          ->view_transition_element_resource_id.IsValid()) {
    stack_.back().occlusion_from_outside_target.Clear();
    stack_.back().occlusion_from_inside_target.Clear();
  }
}

static void ReduceOcclusionBelowSurface(
    const RenderSurfaceImpl* contributing_surface,
    const gfx::Rect& surface_rect,
    const gfx::Transform& surface_transform,
    SimpleEnclosedRegion* occlusion_from_inside_target) {
  if (surface_rect.IsEmpty())
    return;

  gfx::Rect target_rect =
      MathUtil::MapEnclosingClippedRect(surface_transform, surface_rect);
  if (contributing_surface->is_clipped()) {
    target_rect.Intersect(contributing_surface->clip_rect());
  }
  if (target_rect.IsEmpty())
    return;

  gfx::Rect affected_area_in_target =
      contributing_surface->BackdropFilters().MapRectReverse(target_rect,
                                                             SkMatrix::I());
  // Unite target_rect because we only care about positive outsets.
  affected_area_in_target.Union(target_rect);

  SimpleEnclosedRegion affected_occlusion = *occlusion_from_inside_target;
  affected_occlusion.Intersect(affected_area_in_target);

  occlusion_from_inside_target->Subtract(affected_area_in_target);
  for (size_t i = 0; i < affected_occlusion.GetRegionComplexity(); ++i) {
    gfx::Rect occlusion_rect = affected_occlusion.GetRect(i);

    // Shrink the rect by expanding the non-opaque pixels outside the rect.

    // The left outset of the filters moves pixels on the right side of
    // the occlusion_rect into it, shrinking its right edge.
    int shrink_left =
        occlusion_rect.x() == affected_area_in_target.x()
            ? 0
            : affected_area_in_target.right() - target_rect.right();
    int shrink_top =
        occlusion_rect.y() == affected_area_in_target.y()
            ? 0
            : affected_area_in_target.bottom() - target_rect.bottom();
    int shrink_right = occlusion_rect.right() == affected_area_in_target.right()
                           ? 0
                           : target_rect.x() - affected_area_in_target.x();
    int shrink_bottom =
        occlusion_rect.bottom() == affected_area_in_target.bottom()
            ? 0
            : target_rect.y() - affected_area_in_target.y();

    occlusion_rect.Inset(gfx::Insets::TLBR(shrink_top, shrink_left,
                                           shrink_bottom, shrink_right));

    occlusion_from_inside_target->Union(occlusion_rect);
  }
}

void OcclusionTracker::LeaveToRenderTarget(
    const RenderSurfaceImpl* new_target_surface) {
  DCHECK(!stack_.empty());
  size_t last_index = stack_.size() - 1;
  DCHECK(new_target_surface);
  bool surface_will_be_at_top_after_pop =
      stack_.size() > 1 && stack_[last_index - 1].target == new_target_surface;

  // We merge the screen occlusion from the current RenderSurfaceImpl subtree
  // out to its parent target RenderSurfaceImpl. The target occlusion can be
  // merged out as well but needs to be transformed to the new target.

  const RenderSurfaceImpl* old_surface = stack_[last_index].target;

  SimpleEnclosedRegion old_occlusion_from_inside_target_in_new_target =
      TransformSurfaceOpaqueRegion(
          stack_[last_index].occlusion_from_inside_target,
          old_surface->is_clipped(), old_surface->clip_rect(),
          old_surface->draw_transform());

  SimpleEnclosedRegion old_occlusion_from_outside_target_in_new_target =
      TransformSurfaceOpaqueRegion(
          stack_[last_index].occlusion_from_outside_target, false, gfx::Rect(),
          old_surface->draw_transform());

  gfx::Rect unoccluded_surface_rect;
  if (old_surface->BackdropFilters().HasFilterThatMovesPixels()) {
    Occlusion surface_occlusion = GetCurrentOcclusionForContributingSurface(
        old_surface->draw_transform());
    unoccluded_surface_rect =
        surface_occlusion.GetUnoccludedContentRect(old_surface->content_rect());
  }

  bool is_root = new_target_surface->render_target() == new_target_surface;
  if (surface_will_be_at_top_after_pop) {
    // Merge the top of the stack down.
    stack_[last_index - 1].occlusion_from_inside_target.Union(
        old_occlusion_from_inside_target_in_new_target);
    // TODO(danakj): Strictly this should subtract the inside target occlusion
    // before union.
    if (!is_root) {
      stack_[last_index - 1].occlusion_from_outside_target.Union(
          old_occlusion_from_outside_target_in_new_target);
    }
    stack_.pop_back();
  } else {
    // Replace the top of the stack with the new pushed surface.
    stack_.back().target = new_target_surface;
    stack_.back().occlusion_from_inside_target =
        old_occlusion_from_inside_target_in_new_target;
    if (!is_root) {
      stack_.back().occlusion_from_outside_target =
          old_occlusion_from_outside_target_in_new_target;
    } else {
      stack_.back().occlusion_from_outside_target.Clear();
    }
  }

  if (!old_surface->BackdropFilters().HasFilterThatMovesPixels())
    return;

  ReduceOcclusionBelowSurface(old_surface, unoccluded_surface_rect,
                              old_surface->draw_transform(),
                              &stack_.back().occlusion_from_inside_target);
  ReduceOcclusionBelowSurface(old_surface, unoccluded_surface_rect,
                              old_surface->draw_transform(),
                              &stack_.back().occlusion_from_outside_target);
}

void OcclusionTracker::MarkOccludedBehindLayer(const LayerImpl* layer) {
  DCHECK(!stack_.empty());
  DCHECK_EQ(layer->render_target(), stack_.back().target);

  if (layer->draw_opacity() < 1)
    return;

  if (layer->Is3dSorted())
    return;

  if (!layer->draw_properties().mask_filter_info.IsEmpty())
    return;

  SimpleEnclosedRegion opaque_layer_region = layer->VisibleOpaqueRegion();
  if (opaque_layer_region.IsEmpty())
    return;

  // If the blend mode is not occluding and the effect doesn't have a render
  // surface, then the layer should not occlude. An example of this would
  // otherwise be wrong is that this layer is a non-render-surface mask layer
  // with kDstIn blend mode.
  const auto* effect_node =
      layer->layer_tree_impl()->property_trees()->effect_tree().Node(
          layer->effect_tree_index());
  if (!effect_node->HasRenderSurface() &&
      !IsOccludingBlendMode(effect_node->blend_mode))
    return;

  DCHECK(layer->visible_layer_rect().Contains(opaque_layer_region.bounds()));

  gfx::Transform draw_transform = layer->DrawTransform();
  // TODO(danakj): Find a rect interior to each transformed quad.
  if (!draw_transform.NonDegeneratePreserves2dAxisAlignment())
    return;

  gfx::Rect clip_rect_in_target = ScreenSpaceClipRectInTargetSurface(
      layer->render_target(), screen_space_clip_rect_);
  if (layer->is_clipped()) {
    clip_rect_in_target.Intersect(layer->clip_rect());
  } else {
    clip_rect_in_target.Intersect(layer->render_target()->content_rect());
  }

  for (size_t i = 0; i < opaque_layer_region.GetRegionComplexity(); ++i) {
    gfx::Rect transformed_rect =
        MathUtil::MapEnclosedRectWith2dAxisAlignedTransform(
            draw_transform, opaque_layer_region.GetRect(i));
    transformed_rect.Intersect(clip_rect_in_target);
    if (transformed_rect.width() < minimum_tracking_size_.width() &&
        transformed_rect.height() < minimum_tracking_size_.height())
      continue;
    stack_.back().occlusion_from_inside_target.Union(transformed_rect);
  }
}

Region OcclusionTracker::ComputeVisibleRegionInScreen(
    const LayerTreeImpl* layer_tree) const {
  DCHECK(layer_tree->RootRenderSurface() == stack_.back().target);
  const SimpleEnclosedRegion& occluded =
      stack_.back().occlusion_from_inside_target;
  Region visible_region(screen_space_clip_rect_);
  for (size_t i = 0; i < occluded.GetRegionComplexity(); ++i)
    visible_region.Subtract(occluded.GetRect(i));
  return visible_region;
}

}  // namespace cc
