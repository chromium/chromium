// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/damage_tracker.h"

#include <stddef.h>

#include <algorithm>

#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "cc/base/math_util.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/render_surface_impl.h"
#include "cc/paint/filter_operations.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "components/viz/common/viz_utils.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace cc {

std::unique_ptr<DamageTracker> DamageTracker::Create() {
  return base::WrapUnique(new DamageTracker());
}

DamageTracker::DamageTracker() = default;
DamageTracker::~DamageTracker() = default;

void DamageTracker::UpdateDamageTracking(LayerTreeImpl* layer_tree_impl) {
  //
  // This function computes the "damage rect" of each target surface, and
  // updates the state that is used to correctly track damage across frames. The
  // damage rect is the region of the surface that may have changed and needs to
  // be redrawn. This can be used to scissor what is actually drawn, to save GPU
  // computation and bandwidth.
  //
  // The surface's damage rect is computed as the union of all possible changes
  // that have happened to the surface since the last frame was drawn. This
  // includes:
  //   - any changes for existing layers/surfaces that contribute to the target
  //     surface
  //   - layers/surfaces that existed in the previous frame, but no longer exist
  //
  // The basic algorithm for computing the damage region is as follows:
  //
  //   1. compute damage caused by changes in contributing layers or surfaces
  //       for each contributing layer or render surface:
  //           add the layer's or surface's damage to the target surface.
  //
  //   2. compute damage caused by the target surface's mask, if it exists.
  //
  //   3. compute damage caused by old layers/surfaces that no longer exist
  //       for each leftover layer or render surface:
  //           add the old layer/surface bounds to the target surface damage.
  //
  //   4. combine all partial damage rects to get the full damage rect.
  //
  // Additional important points:
  //
  // - This algorithm requires that descendant surfaces compute their damage
  //   before ancestor surfaces. Further, since contributing surfaces with
  //   backdrop filters can expand the damage caused by contributors
  //   underneath them (that is, before them in draw order), the exact damage
  //   caused by these contributors must be computed before computing the damage
  //   caused by the contributing surface. This is implemented by visiting
  //   layers in draw order, computing the damage caused by each one to their
  //   target; during this walk, as soon as all of a surface's contributors have
  //   been visited, the surface's own damage is computed and then added to its
  //   target's accumulated damage.
  //
  // - Changes to layers/surfaces indicate "damage" to the target surface; If a
  //   layer is not changed, it does NOT mean that the layer can skip drawing.
  //   All layers that overlap the damaged region still need to be drawn. For
  //   example, if a layer changed its opacity, then layers underneath must be
  //   re-drawn as well, even if they did not change.
  //
  // - If a layer/surface property changed, the old bounds and new bounds may
  //   overlap... i.e. some of the exposed region may not actually be exposing
  //   anything. But this does not artificially inflate the damage rect. If the
  //   layer changed, its entire old bounds would always need to be redrawn,
  //   regardless of how much it overlaps with the layer's new bounds, which
  //   also need to be entirely redrawn.
  //
  // - See comments in the rest of the code to see what exactly is considered a
  //   "change" in a layer/surface.
  //
  // - To correctly manage exposed rects, SortedRectMap is maintained:
  //
  //      1. All existing rects from the previous frame are marked as
  //         not updated.
  //      2. The map contains all the layer bounds that contributed to
  //         the previous frame (even outside the previous damaged area). If a
  //         layer changes or does not exist anymore, those regions are then
  //         exposed and damage the target surface. As the algorithm progresses,
  //         entries are updated in the map until only leftover layers
  //         that no longer exist stay marked not updated.
  //
  //      3. After the damage rect is computed, the leftover not marked regions
  //         in a map are used to compute are damaged by deleted layers and
  //         erased from map.
  //

  ViewTransitionElementResourceIdToRenderSurfaceMap id_to_render_surface_map;
  InitializeUpdateDamageTracking(layer_tree_impl, id_to_render_surface_map);

  EffectTree& effect_tree =
      layer_tree_impl->property_trees()->effect_tree_mutable();
  int current_target_effect_id = kContentsRootPropertyNodeId;
  DCHECK(effect_tree.GetRenderSurface(current_target_effect_id));
  for (LayerImpl* layer : *layer_tree_impl) {
    if (!layer->contributes_to_drawn_render_surface())
      continue;

    int next_target_effect_id = layer->render_target_effect_tree_index();
    if (next_target_effect_id != current_target_effect_id) {
      int lowest_common_ancestor_id =
          effect_tree.LowestCommonAncestorWithRenderSurface(
              current_target_effect_id, next_target_effect_id);
      while (current_target_effect_id != lowest_common_ancestor_id) {
        // Moving to a non-descendant target surface. This implies that the
        // current target doesn't have any more contributors, since only
        // descendants can contribute to a target, and the each's target's
        // content (including content contributed by descendants) is contiguous
        // in draw order.
        RenderSurfaceImpl* current_target =
            effect_tree.GetRenderSurface(current_target_effect_id);
        current_target->damage_tracker()->ComputeSurfaceDamage(current_target);
        RenderSurfaceImpl* parent_target = current_target->render_target();
        parent_target->damage_tracker()->AccumulateDamageFromRenderSurface(
            current_target);
        current_target_effect_id =
            effect_tree.Node(current_target_effect_id)->target_id;
      }
      current_target_effect_id = next_target_effect_id;
    }

    RenderSurfaceImpl* target_surface = layer->render_target();

    // We skip damage from the HUD layer because (a) the HUD layer damages the
    // whole frame and (b) we don't want HUD layer damage to be shown by the
    // HUD damage rect visualization.
    if (layer != layer_tree_impl->hud_layer()) {
      target_surface->damage_tracker()->AccumulateDamageFromLayer(
          layer, id_to_render_surface_map);
    }
  }

  DCHECK_GE(current_target_effect_id, kContentsRootPropertyNodeId);
  RenderSurfaceImpl* current_target =
      effect_tree.GetRenderSurface(current_target_effect_id);
  while (true) {
    current_target->damage_tracker()->ComputeSurfaceDamage(current_target);
    if (current_target->EffectTreeIndex() == kContentsRootPropertyNodeId)
      break;
    RenderSurfaceImpl* next_target = current_target->render_target();
    next_target->damage_tracker()->AccumulateDamageFromRenderSurface(
        current_target);
    current_target = next_target;
  }
}

void DamageTracker::InitializeUpdateDamageTracking(
    LayerTreeImpl* layer_tree_impl,
    ViewTransitionElementResourceIdToRenderSurfaceMap&
        id_to_render_surface_map) {
  for (RenderSurfaceImpl* render_surface :
       layer_tree_impl->GetRenderSurfaceList()) {
    render_surface->damage_tracker()->PrepareForUpdate();

    // Build ViewTransitionElementResourceId to RenderSurface Map. This will be
    // used for ViewTransitionContentLayerImpl.
    auto resource_id =
        render_surface->OwningEffectNode()->view_transition_element_resource_id;

    if (resource_id.IsValid()) {
      DCHECK(!base::Contains(id_to_render_surface_map, resource_id));
      id_to_render_surface_map.emplace(resource_id, render_surface);
    }
  }
}

void DamageTracker::ComputeSurfaceDamage(RenderSurfaceImpl* render_surface) {
  // All damage from contributing layers and surfaces must already have been
  // added to damage_for_this_update_ through calls to AccumulateDamageFromLayer
  // and AccumulateDamageFromRenderSurface.

  // These functions cannot be bypassed with early-exits, even if we know what
  // the damage will be for this frame, because we need to update the damage
  // tracker state to correctly track the next frame.
  DamageAccumulator damage_from_leftover_rects = TrackDamageFromLeftoverRects();
  // True if any layer is removed.
  has_damage_from_contributing_content_ |=
      !damage_from_leftover_rects.IsEmpty();

  gfx::Rect expanded_damage_rect;
  bool valid = damage_from_leftover_rects.GetAsRect(&expanded_damage_rect);
  bool expanded = false;
  // Iterate through the surfaces rendering to the current target back to
  // front, intersect their surface rects with the damage from leftover rects.
  // Update surfaces' |intersects_damage_under| flags accordingly and expand the
  // damage by surface rects for surfaces with pixel-moving backdrop filters
  // when appropriate.
  for (auto& contributing_surface : contributing_surfaces_) {
    RenderSurfaceImpl* surface = contributing_surface.render_surface;
    bool has_pixel_moving_backdrop_filters =
        surface->BackdropFilters().HasFilterThatMovesPixels();
    if (!surface->intersects_damage_under() ||
        has_pixel_moving_backdrop_filters) {
      if (!valid || contributing_surface.rect_in_target_space.Intersects(
                        expanded_damage_rect)) {
        surface->set_intersects_damage_under(true);
        if (has_pixel_moving_backdrop_filters) {
          expanded_damage_rect.Union(contributing_surface.rect_in_target_space);
          expanded = true;
        }
      }
    }
  }
  if (expanded) {
    damage_for_this_update_.Union(expanded_damage_rect,
                                  damage_from_leftover_rects.reasons());
  }

  contributing_surfaces_.clear();

  // Need to merge all non-empty damage reasons in both branches of damage
  // computation, so compute reasons on the side.
  DamageReasonSet reasons = damage_for_this_update_.reasons();
  if (render_surface->SurfacePropertyChanged()) {
    reasons.Put(DamageReason::kUntracked);
  }
  if (!damage_from_leftover_rects.IsEmpty()) {
    reasons.PutAll(damage_from_leftover_rects.reasons());
  }
  if (render_surface->SurfacePropertyChanged() &&
      !render_surface->AncestorPropertyChanged()) {
    damage_for_this_update_ = DamageAccumulator();
    damage_for_this_update_.Union(render_surface->content_rect(), {});
  } else {
    // TODO(shawnsingh): can we clamp this damage to the surface's content rect?
    // (affects performance, but not correctness)
    damage_for_this_update_.Union(damage_from_leftover_rects, {});

    gfx::Rect damage_rect;
    bool is_rect_valid = damage_for_this_update_.GetAsRect(&damage_rect);
    if (is_rect_valid && !damage_rect.IsEmpty()) {
      damage_rect = render_surface->Filters().MapRect(
          damage_rect,
          gfx::TransformToFlattenedSkMatrix(render_surface->SurfaceScale()));
      damage_for_this_update_ = DamageAccumulator();
      damage_for_this_update_.Union(damage_rect, {});
    }
  }
  damage_for_this_update_.UnionReasons(reasons);

  // True if there is surface property change from descendant (clip_rect or
  // content_rect).
  if (render_surface->SurfacePropertyChanged()) {
    has_damage_from_contributing_content_ |= !damage_for_this_update_.IsEmpty();
  }

  // Damage accumulates until we are notified that we actually did draw on that
  // frame.
  current_damage_.Union(damage_for_this_update_,
                        damage_for_this_update_.reasons());
}

bool DamageTracker::GetDamageRectIfValid(gfx::Rect* rect) {
  return current_damage_.GetAsRect(rect);
}

DamageReasonSet DamageTracker::GetDamageReasons() {
  return current_damage_.reasons();
}

DamageTracker::LayerRectMapData& DamageTracker::RectDataForLayer(
    int layer_id,
    bool* layer_is_new) {
  LayerRectMapData data(layer_id);

  auto it = std::lower_bound(rect_history_for_layers_.begin(),
                             rect_history_for_layers_.end(), data);

  if (it == rect_history_for_layers_.end() || it->layer_id_ != layer_id) {
    *layer_is_new = true;
    it = rect_history_for_layers_.insert(it, data);
  }

  return *it;
}

DamageTracker::SurfaceRectMapData& DamageTracker::RectDataForSurface(
    ElementId surface_id,
    bool* surface_is_new) {
  SurfaceRectMapData data(surface_id);

  auto it = std::lower_bound(rect_history_for_surfaces_.begin(),
                             rect_history_for_surfaces_.end(), data);

  if (it == rect_history_for_surfaces_.end() || it->surface_id_ != surface_id) {
    *surface_is_new = true;
    it = rect_history_for_surfaces_.insert(it, data);
  }

  return *it;
}

void DamageTracker::PrepareForUpdate() {
  mailbox_id_++;
  damage_for_this_update_ = DamageAccumulator();
  has_damage_from_contributing_content_ = false;
  contributing_surfaces_.clear();
  current_view_transition_content_surfaces_by_id_.swap(
      previous_view_transition_content_surfaces_by_id_);
  current_view_transition_content_surfaces_by_id_.clear();
}

DamageTracker::DamageAccumulator DamageTracker::TrackDamageFromLeftoverRects() {
  // After computing damage for all active layers, any leftover items in the
  // current rect history correspond to layers/surfaces that no longer exist.
  // So, these regions are now exposed on the target surface.

  DamageAccumulator damage;
  auto layer_cur_pos = rect_history_for_layers_.begin();
  auto layer_copy_pos = layer_cur_pos;
  auto surface_cur_pos = rect_history_for_surfaces_.begin();
  auto surface_copy_pos = surface_cur_pos;

  // Loop below basically implements std::remove_if loop with and extra
  // processing (adding deleted rect to damage) for deleted items.
  // cur_pos iterator runs through all elements of the vector, but copy_pos
  // always points to the element after the last not deleted element. If new
  // not deleted element found then it is copied to the *copy_pos and copy_pos
  // moved to the next position.
  // If there are no deleted elements then copy_pos iterator is in sync with
  // cur_pos and no copy happens.
  while (layer_cur_pos < rect_history_for_layers_.end()) {
    if (layer_cur_pos->mailbox_id_ == mailbox_id_) {
      if (layer_cur_pos != layer_copy_pos)
        *layer_copy_pos = *layer_cur_pos;

      ++layer_copy_pos;
    } else {
      damage.Union(layer_cur_pos->rect_, {DamageReason::kUntracked});
    }

    ++layer_cur_pos;
  }

  while (surface_cur_pos < rect_history_for_surfaces_.end()) {
    if (surface_cur_pos->mailbox_id_ == mailbox_id_) {
      if (surface_cur_pos != surface_copy_pos)
        *surface_copy_pos = *surface_cur_pos;

      ++surface_copy_pos;
    } else {
      damage.Union(surface_cur_pos->rect_, {DamageReason::kUntracked});
    }

    ++surface_cur_pos;
  }

  if (layer_copy_pos != rect_history_for_layers_.end())
    rect_history_for_layers_.erase(layer_copy_pos,
                                   rect_history_for_layers_.end());
  if (surface_copy_pos != rect_history_for_surfaces_.end())
    rect_history_for_surfaces_.erase(surface_copy_pos,
                                     rect_history_for_surfaces_.end());

  // If the vector has excessive storage, shrink it
  if (rect_history_for_layers_.capacity() > rect_history_for_layers_.size() * 4)
    SortedRectMapForLayers(rect_history_for_layers_)
        .swap(rect_history_for_layers_);
  if (rect_history_for_surfaces_.capacity() >
      rect_history_for_surfaces_.size() * 4)
    SortedRectMapForSurfaces(rect_history_for_surfaces_)
        .swap(rect_history_for_surfaces_);

  return damage;
}

void DamageTracker::AccumulateDamageFromLayer(
    LayerImpl* layer,
    ViewTransitionElementResourceIdToRenderSurfaceMap&
        id_to_render_surface_map) {
  // There are two ways that a layer can damage a region of the target surface:
  //   1. Property change (e.g. opacity, position, transforms):
  //        - the entire region of the layer itself damages the surface.
  //        - the old layer region also damages the surface, because this region
  //          is now exposed.
  //        - note that in many cases the old and new layer rects may overlap,
  //          which is fine.
  //
  //   2. Repaint/update: If a region of the layer that was repainted/updated,
  //      that region damages the surface.
  //
  // Property changes take priority over update rects.
  //
  // This method is called when we want to consider how a layer contributes to
  // its target RenderSurface, even if that layer owns the target RenderSurface
  // itself. To consider how a layer's target surface contributes to the
  // ancestor surface, AccumulateDamageFromRenderSurface() must be called
  // instead.
  bool layer_is_new = false;
  LayerRectMapData& data = RectDataForLayer(layer->id(), &layer_is_new);
  gfx::Rect old_visible_rect_in_target_space = data.rect_;

  gfx::Rect visible_rect_in_target_space =
      layer->GetEnclosingVisibleRectInTargetSpace();
  data.Update(visible_rect_in_target_space, mailbox_id_);

  gfx::Rect view_transition_content_surface_damage_rect;
  if (layer->ViewTransitionResourceId().IsValid()) {
    // Always call this function for view transition so the view transition
    // content surface with vt id can be tracked at
    // |current_view_transition_content_surfaces_by_id_|.
    view_transition_content_surface_damage_rect =
        GetViewTransitionContentSurfaceDamageInSharedElementLayerSpace(
            layer, id_to_render_surface_map);
  }

  if (layer_is_new || layer->LayerPropertyChanged()) {
    DamageReasonSet reasons = layer->GetDamageReasons();
    if (layer_is_new) {
      reasons.Put(DamageReason::kUntracked);
    }
    // If a layer is new or has changed, then its entire layer rect affects the
    // target surface.
    damage_for_this_update_.Union(visible_rect_in_target_space, reasons);

    // The layer's old region is now exposed on the target surface, too.
    // Note old_visible_rect_in_target_space is already in target space.
    damage_for_this_update_.Union(old_visible_rect_in_target_space, {});
  } else {
    DamageReasonSet reasons = layer->GetDamageReasons();
    // If the layer properties haven't changed, then the the target surface is
    // only affected by the layer's damaged area, which could be empty.
    gfx::Rect damage_rect =
        gfx::UnionRects(layer->update_rect(), layer->GetDamageRect());
    // if this is a view transition layer, the damage from the corresponding
    // live content surface should propagate to the layer's parent surface.
    // |view_transition_content_surface_damage_rect| is in the layer's space.
    damage_rect.Union(view_transition_content_surface_damage_rect);
    if (view_transition_content_surface_damage_rect.Intersects(
            gfx::Rect(layer->bounds()))) {
      reasons.Put(DamageReason::kUntracked);
    }

    damage_rect.Intersect(gfx::Rect(layer->bounds()));

    if (!damage_rect.IsEmpty()) {
      gfx::Rect damage_visible_rect_in_target_space =
          MathUtil::MapEnclosingClippedRect(layer->DrawTransform(),
                                            damage_rect);
      damage_for_this_update_.Union(damage_visible_rect_in_target_space, {});
    }
    damage_for_this_update_.UnionReasons(reasons);
  }

  // Property changes on effect or transform nodes that are shared by the
  // render target are not considered damage to that target itself.  This
  // is the case where the render target itself changes opacity or moves.
  // The damage goes to the target's target instead.  This is not perfect,
  // as the target and layer could share an effect but not a transform,
  // but there's no tracking on the layer to differentiate that the
  // LayerPropertyChangedFromPropertyTrees is for the effect not the transform.
  bool property_change_on_non_target_node = false;
  if (layer->LayerPropertyChangedFromPropertyTrees()) {
    auto effect_id = layer->render_target()->EffectTreeIndex();
    const auto* effect_node =
        layer->layer_tree_impl()->property_trees()->effect_tree().Node(
            effect_id);
    auto transform_id = effect_node->transform_id;
    property_change_on_non_target_node =
        layer->effect_tree_index() != effect_id ||
        layer->transform_tree_index() != transform_id;
  }

  if (layer_is_new || !layer->update_rect().IsEmpty() ||
      layer->LayerPropertyChangedNotFromPropertyTrees() ||
      !layer->GetDamageRect().IsEmpty() || property_change_on_non_target_node ||
      !view_transition_content_surface_damage_rect.IsEmpty()) {
    has_damage_from_contributing_content_ |= !damage_for_this_update_.IsEmpty();
  }
}

void DamageTracker::AccumulateDamageFromRenderSurface(
    RenderSurfaceImpl* render_surface) {
  // There are two ways a "descendant surface" can damage regions of the "target
  // surface":
  //   1. Property change:
  //        - a surface's geometry can change because of
  //            - changes to descendants (i.e. the subtree) that affect the
  //              surface's content rect
  //            - changes to ancestor layers that propagate their property
  //              changes to their entire subtree.
  //        - just like layers, both the old surface rect and new surface rect
  //          will damage the target surface in this case.
  //
  //   2. Damage rect: This surface may have been damaged by its own layer_list
  //      as well, and that damage should propagate to the target surface.
  //

  bool surface_is_new = false;
  SurfaceRectMapData& data =
      RectDataForSurface(render_surface->id(), &surface_is_new);
  gfx::Rect old_surface_rect = data.rect_;

  gfx::Rect surface_rect_in_target_space =
      gfx::ToEnclosingRect(render_surface->DrawableContentRect());
  data.Update(surface_rect_in_target_space, mailbox_id_);
  contributing_surfaces_.emplace_back(render_surface,
                                      surface_rect_in_target_space);

  // If the render surface has pixel-moving backdrop filters and the surface
  // rect intersects current accumulated damage, expand the damage by surface
  // rect.
  gfx::Rect damage_on_target;
  bool valid = damage_for_this_update_.GetAsRect(&damage_on_target);
  bool intersects_damage_under =
      !valid || damage_on_target.Intersects(surface_rect_in_target_space);
  if (render_surface->BackdropFilters().HasFilterThatMovesPixels() &&
      intersects_damage_under) {
    damage_for_this_update_.Union(surface_rect_in_target_space,
                                  {DamageReason::kUntracked});
  }

  if (surface_is_new || render_surface->SurfacePropertyChanged() ||
      render_surface->AncestorPropertyChanged()) {
    DamageReasonSet reasons =
        render_surface->damage_tracker()->GetDamageReasons();
    if (surface_is_new) {
      reasons.Put(DamageReason::kUntracked);
    }
    // The entire surface contributes damage.
    damage_for_this_update_.Union(surface_rect_in_target_space, reasons);

    // The surface's old region is now exposed on the target surface, too.
    damage_for_this_update_.Union(old_surface_rect, {});

    intersects_damage_under = true;
  } else {
    // Only the surface's damage_rect will damage the target surface.
    gfx::Rect damage_rect_in_local_space;
    bool is_valid_rect = render_surface->damage_tracker()->GetDamageRectIfValid(
        &damage_rect_in_local_space);
    if (is_valid_rect && !damage_rect_in_local_space.IsEmpty()) {
      // If there was damage, transform it to target space, and possibly
      // contribute its reflection if needed.
      const gfx::Transform& draw_transform = render_surface->draw_transform();

      gfx::Rect damage_rect_in_target_space = MathUtil::MapEnclosingClippedRect(
          draw_transform, damage_rect_in_local_space);
      damage_rect_in_target_space.Intersect(surface_rect_in_target_space);
      damage_for_this_update_.Union(
          damage_rect_in_target_space,
          render_surface->damage_tracker()->GetDamageReasons());
    } else if (!is_valid_rect) {
      damage_for_this_update_.Union(
          surface_rect_in_target_space,
          render_surface->damage_tracker()->GetDamageReasons());
    }
  }

  render_surface->set_intersects_damage_under(intersects_damage_under);
  // True if any changes from contributing render surface.
  has_damage_from_contributing_content_ |= !damage_for_this_update_.IsEmpty();
}

bool DamageTracker::DamageAccumulator::GetAsRect(gfx::Rect* rect) {
  if (!is_valid_rect_)
    return false;

  base::CheckedNumeric<int> width = right_;
  width -= x_;
  base::CheckedNumeric<int> height = bottom_;
  height -= y_;
  if (!width.IsValid() || !height.IsValid()) {
    is_valid_rect_ = false;
    return false;
  }

  rect->set_x(x_);
  rect->set_y(y_);
  rect->set_width(width.ValueOrDie());
  rect->set_height(height.ValueOrDie());
  return true;
}

gfx::Rect
DamageTracker::GetViewTransitionContentSurfaceDamageInSharedElementLayerSpace(
    LayerImpl* layer,
    ViewTransitionElementResourceIdToRenderSurfaceMap&
        id_to_render_surface_map) {
  DCHECK(layer->ViewTransitionResourceId().IsValid());

  auto vt_resource_id = layer->ViewTransitionResourceId();

  // Get the corrosponding view transition content surface with the same view
  // transition element resource id.
  RenderSurfaceImpl* view_transition_content_surface = nullptr;
  auto shared_surface_it = id_to_render_surface_map.find(vt_resource_id);
  if (shared_surface_it != id_to_render_surface_map.end()) {
    view_transition_content_surface = shared_surface_it->second;

    // A live content surface is found. Add this id to the current id list.
    // Don't add this id if no surface is found.
    DCHECK(!base::Contains(current_view_transition_content_surfaces_by_id_,
                           vt_resource_id));
    current_view_transition_content_surfaces_by_id_.push_back(vt_resource_id);
  }

  // In the case of missing a content surface, the damage will be added as
  // follows:
  // At the very first frame where the live content surface is missing, the
  // whole layer should be damaged. But after the second frame, mark no damage
  // on the layer since it's still missing, no change from the previous frame.
  // When the live content surface shows up again, it becomes a new surface and
  // new surface will cause full damage.

  // In the case of the view transition layer appearing/disappearing, it should
  // be sufficiently handled by the logic in AccumulateDamageFromLayer
  // and ComputeSurfaceDamage.

  // The start and end of view transition will need to damage the original
  // location of the content surface, but that should already be done because
  // the topology of the render surfaces changes.

  //
  // Check the render surface itself without considering view transition id for
  // full damage.
  //
  gfx::Rect layer_drawable_bounds = gfx::Rect(layer->bounds());
  if (view_transition_content_surface) {
    bool surface_is_new = false;
    auto& data = RectDataForSurface(view_transition_content_surface->id(),
                                    &surface_is_new);
    data.Update(layer_drawable_bounds, mailbox_id_);
    if (surface_is_new ||
        view_transition_content_surface->SurfacePropertyChanged() ||
        view_transition_content_surface->AncestorPropertyChanged()) {
      // The whole view transition layer is considered damaged.
      return layer_drawable_bounds;
    }
  }

  //
  // Check the new/missing status of view_transition_content_surface with
  // vt_resource_id for full damage.
  //

  // (1) If this is a new content surface with vt_resource_id.
  // (2) If the content surface with vt_resource_id was there in the previous
  // frame but missing in the current frame.
  if (base::Contains(previous_view_transition_content_surfaces_by_id_,
                     vt_resource_id) !=
      base::Contains(current_view_transition_content_surfaces_by_id_,
                     vt_resource_id)) {
    // The whole view transition layer is considered damaged.
    return layer_drawable_bounds;
  }

  //
  // No damage if no corresponding content render surface.
  //
  if (!view_transition_content_surface) {
    // If we had this content surface in the previous frame but missing in this
    // frame, it's already handled above. If there is no corrosponding content
    // surface in the current frame, same as the previous frame, consider no
    // damage for this render surface.
    return gfx::Rect();
  }

  //
  // Calculate the damage propagated from view_transition_content_surface to
  // this vt layer.
  //
  gfx::Rect damage_rect_in_local_space;
  bool is_valid_rect =
      view_transition_content_surface->damage_tracker()->GetDamageRectIfValid(
          &damage_rect_in_local_space);
  if (!is_valid_rect) {
    // The whole view transition layer is considered damaged.
    return layer_drawable_bounds;
  } else if (damage_rect_in_local_space.IsEmpty()) {
    return gfx::Rect();
  } else {
    gfx::Rect render_surface_content_rect =
        view_transition_content_surface->content_rect();
    gfx::Transform view_transition_transform = viz::GetViewTransitionTransform(
        layer_drawable_bounds, render_surface_content_rect);

    // Convert the damage from the view transition content surface space to the
    // shared element layer space.
    return MathUtil::MapEnclosingClippedRect(view_transition_transform,
                                             damage_rect_in_local_space);
  }
}

}  // namespace cc
