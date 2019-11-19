// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/draw_property_utils.h"

#include <stddef.h>

#include <vector>

#include "base/containers/adapters.h"
#include "base/containers/stack.h"
#include "cc/base/math_util.h"
#include "cc/layers/draw_properties.h"
#include "cc/layers/layer.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/picture_layer.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/property_tree_builder.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "components/viz/common/display/de_jelly.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

namespace draw_property_utils {

namespace {

gfx::Rect ToEnclosingClipRect(const gfx::RectF& clip_rect) {
  constexpr float kClipError = 0.00001f;
  return gfx::ToEnclosingRectIgnoringError(clip_rect, kClipError);
}

bool IsRootLayer(const Layer* layer) {
  return !layer->parent();
}

bool IsRootLayer(const LayerImpl* layer) {
  return layer->layer_tree_impl()->IsRootLayer(layer);
}

void PostConcatSurfaceContentsScale(const EffectNode* effect_node,
                                    gfx::Transform* transform) {
  if (!effect_node) {
    // This can happen when PaintArtifactCompositor builds property trees as it
    // doesn't set effect ids on clip nodes.
    return;
  }
  DCHECK(effect_node->HasRenderSurface());
  transform->matrix().postScale(effect_node->surface_contents_scale.x(),
                                effect_node->surface_contents_scale.y(), 1.f);
}

bool ConvertRectBetweenSurfaceSpaces(const PropertyTrees* property_trees,
                                     int source_effect_id,
                                     int dest_effect_id,
                                     gfx::RectF clip_in_source_space,
                                     gfx::RectF* clip_in_dest_space) {
  const EffectNode* source_effect_node =
      property_trees->effect_tree.Node(source_effect_id);
  int source_transform_id = source_effect_node->transform_id;
  const EffectNode* dest_effect_node =
      property_trees->effect_tree.Node(dest_effect_id);
  int dest_transform_id = dest_effect_node->transform_id;
  gfx::Transform source_to_dest;
  if (source_transform_id > dest_transform_id) {
    if (property_trees->GetToTarget(source_transform_id, dest_effect_id,
                                    &source_to_dest)) {
      ConcatInverseSurfaceContentsScale(source_effect_node, &source_to_dest);
      *clip_in_dest_space =
          MathUtil::MapClippedRect(source_to_dest, clip_in_source_space);
    } else {
      return false;
    }
  } else {
    if (property_trees->GetFromTarget(dest_transform_id, source_effect_id,
                                      &source_to_dest)) {
      PostConcatSurfaceContentsScale(dest_effect_node, &source_to_dest);
      *clip_in_dest_space =
          MathUtil::ProjectClippedRect(source_to_dest, clip_in_source_space);
    } else {
      return false;
    }
  }
  return true;
}

ConditionalClip ComputeTargetRectInLocalSpace(
    gfx::RectF rect,
    const PropertyTrees* property_trees,
    int target_transform_id,
    int local_transform_id,
    const int target_effect_id) {
  gfx::Transform target_to_local;
  bool success = property_trees->GetFromTarget(
      local_transform_id, target_effect_id, &target_to_local);
  // If transform is not invertible, cannot apply clip.
  if (!success)
    return ConditionalClip{false, gfx::RectF()};

  if (target_transform_id > local_transform_id)
    return ConditionalClip{true,  // is_clipped.
                           MathUtil::MapClippedRect(target_to_local, rect)};

  return ConditionalClip{true,  // is_clipped.
                         MathUtil::ProjectClippedRect(target_to_local, rect)};
}

ConditionalClip ComputeLocalRectInTargetSpace(
    gfx::RectF rect,
    const PropertyTrees* property_trees,
    int current_transform_id,
    int target_transform_id,
    int target_effect_id) {
  gfx::Transform current_to_target;
  if (!property_trees->GetToTarget(current_transform_id, target_effect_id,
                                   &current_to_target)) {
    // If transform is not invertible, cannot apply clip.
    return ConditionalClip{false, gfx::RectF()};
  }

  if (current_transform_id > target_transform_id)
    return ConditionalClip{true,  // is_clipped.
                           MathUtil::MapClippedRect(current_to_target, rect)};

  return ConditionalClip{true,  // is_clipped.
                         MathUtil::ProjectClippedRect(current_to_target, rect)};
}

ConditionalClip ComputeCurrentClip(const ClipNode* clip_node,
                                   const PropertyTrees* property_trees,
                                   int target_transform_id,
                                   int target_effect_id) {
  if (clip_node->transform_id != target_transform_id) {
    return ComputeLocalRectInTargetSpace(clip_node->clip, property_trees,
                                         clip_node->transform_id,
                                         target_transform_id, target_effect_id);
  }

  const EffectTree& effect_tree = property_trees->effect_tree;
  gfx::RectF current_clip = clip_node->clip;
  gfx::Vector2dF surface_contents_scale =
      effect_tree.Node(target_effect_id)->surface_contents_scale;
  // The viewport clip should not be scaled.
  if (surface_contents_scale.x() > 0 && surface_contents_scale.y() > 0 &&
      clip_node->transform_id != TransformTree::kRootNodeId)
    current_clip.Scale(surface_contents_scale.x(), surface_contents_scale.y());
  return ConditionalClip{true /* is_clipped */, current_clip};
}

bool ApplyClipNodeToAccumulatedClip(const PropertyTrees* property_trees,
                                    bool include_expanding_clips,
                                    int target_id,
                                    int target_transform_id,
                                    const ClipNode* clip_node,
                                    gfx::RectF* accumulated_clip) {
  switch (clip_node->clip_type) {
    case ClipNode::ClipType::APPLIES_LOCAL_CLIP: {
      ConditionalClip current_clip = ComputeCurrentClip(
          clip_node, property_trees, target_transform_id, target_id);

      // If transform is not invertible, no clip will be applied.
      if (!current_clip.is_clipped)
        return false;

      *accumulated_clip =
          gfx::IntersectRects(*accumulated_clip, current_clip.clip_rect);
      return true;
    }
    case ClipNode::ClipType::EXPANDS_CLIP: {
      if (!include_expanding_clips)
        return true;

      // Bring the accumulated clip to the space of the expanding effect.
      const EffectNode* expanding_effect_node =
          property_trees->effect_tree.Node(
              clip_node->clip_expander->target_effect_id());
      gfx::RectF accumulated_clip_rect_in_expanding_space;
      bool success = ConvertRectBetweenSurfaceSpaces(
          property_trees, target_id, expanding_effect_node->id,
          *accumulated_clip, &accumulated_clip_rect_in_expanding_space);
      // If transform is not invertible, no clip will be applied.
      if (!success)
        return false;

      // Do the expansion.
      gfx::RectF expanded_clip_in_expanding_space =
          gfx::RectF(clip_node->clip_expander->MapRectReverse(
              ToEnclosingClipRect(accumulated_clip_rect_in_expanding_space),
              property_trees));

      // Put the expanded clip back into the original target space.
      success = ConvertRectBetweenSurfaceSpaces(
          property_trees, expanding_effect_node->id, target_id,
          expanded_clip_in_expanding_space, accumulated_clip);
      // If transform is not invertible, no clip will be applied.
      if (!success)
        return false;
      return true;
    }
  }
  NOTREACHED();
  return true;
}

ConditionalClip ComputeAccumulatedClip(PropertyTrees* property_trees,
                                       bool include_expanding_clips,
                                       int local_clip_id,
                                       int target_id) {
  ClipRectData* cached_data =
      property_trees->FetchClipRectFromCache(local_clip_id, target_id);
  if (cached_data->target_id != EffectTree::kInvalidNodeId) {
    // Cache hit
    return cached_data->clip;
  }
  cached_data->target_id = target_id;

  const ClipTree& clip_tree = property_trees->clip_tree;
  const ClipNode* clip_node = clip_tree.Node(local_clip_id);
  const EffectTree& effect_tree = property_trees->effect_tree;
  const EffectNode* target_node = effect_tree.Node(target_id);
  int target_transform_id = target_node->transform_id;

  bool cache_hit = false;
  ConditionalClip cached_clip = ConditionalClip{false, gfx::RectF()};
  ConditionalClip unclipped = ConditionalClip{false, gfx::RectF()};

  // Collect all the clips that need to be accumulated.
  base::stack<const ClipNode*, std::vector<const ClipNode*>> parent_chain;

  // If target is not direct ancestor of clip, this will find least common
  // ancestor between the target and the clip. Or, if the target has a
  // contributing layer that escapes clip, this will find the nearest ancestor
  // that doesn't.
  while (target_node->clip_id > clip_node->id ||
         effect_tree.GetRenderSurface(target_node->id)
             ->has_contributing_layer_that_escapes_clip()) {
    target_node = effect_tree.Node(target_node->target_id);
  }

  // Collect clip nodes up to the least common ancestor or till we get a cache
  // hit.
  while (target_node->clip_id < clip_node->id) {
    if (parent_chain.size() > 0) {
      // Search the cache.
      for (size_t i = 0; i < clip_node->cached_clip_rects->size(); ++i) {
        auto& data = clip_node->cached_clip_rects[i];
        if (data.target_id == target_id) {
          cache_hit = true;
          cached_clip = data.clip;
        }
      }
    }
    parent_chain.push(clip_node);
    clip_node = clip_tree.parent(clip_node);
  }

  if (parent_chain.size() == 0) {
    // No accumulated clip nodes.
    cached_data->clip = unclipped;
    return unclipped;
  }

  clip_node = parent_chain.top();
  parent_chain.pop();

  gfx::RectF accumulated_clip;
  if (cache_hit && cached_clip.is_clipped) {
    // Apply the first clip in parent_chain to the cached clip.
    accumulated_clip = cached_clip.clip_rect;
    bool success = ApplyClipNodeToAccumulatedClip(
        property_trees, include_expanding_clips, target_id, target_transform_id,
        clip_node, &accumulated_clip);
    if (!success) {
      // Singular transform
      cached_data->clip = unclipped;
      return unclipped;
    }
  } else {
    // No cache hit or the cached clip has no clip to apply. We need to find
    // the first clip that applies clip as there is no clip to expand.
    while (clip_node->clip_type != ClipNode::ClipType::APPLIES_LOCAL_CLIP &&
           parent_chain.size() > 0) {
      clip_node = parent_chain.top();
      parent_chain.pop();
    }

    if (clip_node->clip_type != ClipNode::ClipType::APPLIES_LOCAL_CLIP) {
      // No clip to apply.
      cached_data->clip = unclipped;
      return unclipped;
    }
    ConditionalClip current_clip = ComputeCurrentClip(
        clip_node, property_trees, target_transform_id, target_id);
    if (!current_clip.is_clipped) {
      // Singular transform
      cached_data->clip = unclipped;
      return unclipped;
    }
    accumulated_clip = current_clip.clip_rect;
  }

  // Apply remaining clips
  while (parent_chain.size() > 0) {
    clip_node = parent_chain.top();
    parent_chain.pop();
    bool success = ApplyClipNodeToAccumulatedClip(
        property_trees, include_expanding_clips, target_id, target_transform_id,
        clip_node, &accumulated_clip);
    if (!success) {
      // Singular transform
      cached_data->clip = unclipped;
      return unclipped;
    }
  }

  ConditionalClip clip = ConditionalClip{
      true /* is_clipped */,
      accumulated_clip.IsEmpty() ? gfx::RectF() : accumulated_clip};
  cached_data->clip = clip;
  return clip;
}

bool HasSingularTransform(int transform_tree_index, const TransformTree& tree) {
  const TransformNode* node = tree.Node(transform_tree_index);
  return !node->is_invertible || !node->ancestors_are_invertible;
}

int LowestCommonAncestor(int clip_id_1,
                         int clip_id_2,
                         const ClipTree* clip_tree) {
  const ClipNode* clip_node_1 = clip_tree->Node(clip_id_1);
  const ClipNode* clip_node_2 = clip_tree->Node(clip_id_2);
  while (clip_node_1->id != clip_node_2->id) {
    if (clip_node_1->id < clip_node_2->id)
      clip_node_2 = clip_tree->parent(clip_node_2);
    else
      clip_node_1 = clip_tree->parent(clip_node_1);
  }
  return clip_node_1->id;
}

void SetHasContributingLayerThatEscapesClip(int lca_clip_id,
                                            int target_effect_id,
                                            EffectTree* effect_tree) {
  const EffectNode* effect_node = effect_tree->Node(target_effect_id);
  // Find all ancestor targets starting from effect_node who are clipped by
  // a descendant of lowest ancestor clip and set their
  // has_contributing_layer_that_escapes_clip to true.
  while (effect_node->clip_id > lca_clip_id) {
    RenderSurfaceImpl* render_surface =
        effect_tree->GetRenderSurface(effect_node->id);
    DCHECK(render_surface);
    render_surface->set_has_contributing_layer_that_escapes_clip(true);
    effect_node = effect_tree->Node(effect_node->target_id);
  }
}

template <typename LayerType>
int TransformTreeIndexForBackfaceVisibility(LayerType* layer,
                                            const TransformTree& tree) {
  if (!layer->use_parent_backface_visibility() || !layer->has_transform_node())
    return layer->transform_tree_index();
  return tree.Node(layer->transform_tree_index())->parent_id;
}

bool IsTargetSpaceTransformBackFaceVisible(
    Layer* layer,
    int transform_tree_index,
    const PropertyTrees* property_trees) {
  // We do not skip back face invisible layers on main thread as target space
  // transform will not be available here.
  return false;
}

bool IsTargetSpaceTransformBackFaceVisible(
    LayerImpl* layer,
    int transform_tree_index,
    const PropertyTrees* property_trees) {
  gfx::Transform to_target;
  property_trees->GetToTarget(transform_tree_index,
                              layer->render_target_effect_tree_index(),
                              &to_target);
  return to_target.IsBackFaceVisible();
}

template <typename LayerType>
bool IsLayerBackFaceVisible(LayerType* layer,
                            int transform_tree_index,
                            const PropertyTrees* property_trees) {
  return IsTargetSpaceTransformBackFaceVisible(layer, transform_tree_index,
                                               property_trees);
}

inline bool TransformToScreenIsKnown(Layer* layer,
                                     int transform_tree_index,
                                     const TransformTree& tree) {
  const TransformNode* node = tree.Node(transform_tree_index);
  return !node->to_screen_is_potentially_animated;
}

inline bool TransformToScreenIsKnown(LayerImpl* layer,
                                     int transform_tree_index,
                                     const TransformTree& tree) {
  return true;
}

template <typename LayerType>
bool LayerNeedsUpdate(LayerType* layer,
                      bool layer_is_drawn,
                      const PropertyTrees* property_trees) {
  // Layers can be skipped if any of these conditions are met.
  //   - is not drawn due to it or one of its ancestors being hidden (or having
  //     no copy requests).
  //   - does not draw content.
  //   - is transparent.
  //   - has empty bounds
  //   - the layer is not double-sided, but its back face is visible.
  //
  // Some additional conditions need to be computed at a later point after the
  // recursion is finished.
  //   - the intersection of render_surface content and layer clip_rect is empty
  //   - the visible_layer_rect is empty
  //
  // Note, if the layer should not have been drawn due to being fully
  // transparent, we would have skipped the entire subtree and never made it
  // into this function, so it is safe to omit this check here.
  if (!layer_is_drawn)
    return false;

  if (!layer->DrawsContent() || layer->bounds().IsEmpty())
    return false;

  // The layer should not be drawn if (1) it is not double-sided and (2) the
  // back of the layer is known to be facing the screen.
  const TransformTree& tree = property_trees->transform_tree;
  if (layer->should_check_backface_visibility()) {
    int backface_transform_id =
        TransformTreeIndexForBackfaceVisibility(layer, tree);
    // A layer with singular transform is not drawn. So, we can assume that its
    // backface is not visible.
    if (TransformToScreenIsKnown(layer, backface_transform_id, tree) &&
        !HasSingularTransform(backface_transform_id, tree) &&
        IsLayerBackFaceVisible(layer, backface_transform_id, property_trees))
      return false;
  }

  return true;
}

template <typename LayerType>
inline bool LayerShouldBeSkippedForDrawPropertiesComputation(
    LayerType* layer,
    const TransformTree& transform_tree,
    const EffectTree& effect_tree) {
  const EffectNode* effect_node = effect_tree.Node(layer->effect_tree_index());
  if (effect_node->HasRenderSurface() && effect_node->subtree_has_copy_request)
    return false;

  // If the layer transform is not invertible, it should be skipped. In case the
  // transform is animating and singular, we should not skip it.
  const TransformNode* transform_node =
      transform_tree.Node(layer->transform_tree_index());
  return !transform_node->node_and_ancestors_are_animated_or_invertible ||
         effect_node->hidden_by_backface_visibility || !effect_node->is_drawn;
}

gfx::Rect LayerDrawableContentRect(
    const LayerImpl* layer,
    const gfx::Rect& layer_bounds_in_target_space,
    const gfx::Rect& clip_rect) {
  if (layer->is_clipped())
    return IntersectRects(layer_bounds_in_target_space, clip_rect);

  return layer_bounds_in_target_space;
}

void SetSurfaceIsClipped(const ClipTree& clip_tree,
                         RenderSurfaceImpl* render_surface) {
  bool is_clipped;
  if (render_surface->EffectTreeIndex() == EffectTree::kContentsRootNodeId) {
    // Root render surface is always clipped.
    is_clipped = true;
  } else if (render_surface->has_contributing_layer_that_escapes_clip()) {
    // We cannot clip a surface that has a contribuitng layer which escapes the
    // clip.
    is_clipped = false;
  } else if (render_surface->ClipTreeIndex() ==
             render_surface->render_target()->ClipTreeIndex()) {
    // There is no clip between the render surface and its target, so
    // the surface need not be clipped.
    is_clipped = false;
  } else {
    // If the clips between the render surface and its target only expand the
    // clips and do not apply any new clip, we need not clip the render surface.
    const ClipNode* clip_node = clip_tree.Node(render_surface->ClipTreeIndex());
    is_clipped = clip_node->clip_type != ClipNode::ClipType::EXPANDS_CLIP;
  }
  render_surface->SetIsClipped(is_clipped);
}

void SetSurfaceDrawOpacity(const EffectTree& tree,
                           RenderSurfaceImpl* render_surface) {
  // Draw opacity of a surface is the product of opacities between the surface
  // (included) and its target surface (excluded).
  const EffectNode* node = tree.Node(render_surface->EffectTreeIndex());
  float draw_opacity = tree.EffectiveOpacity(node);
  for (node = tree.parent(node); node && !node->HasRenderSurface();
       node = tree.parent(node)) {
    draw_opacity *= tree.EffectiveOpacity(node);
  }
  render_surface->SetDrawOpacity(draw_opacity);
}

float LayerDrawOpacity(const LayerImpl* layer, const EffectTree& tree) {
  if (!layer->render_target())
    return 0.f;

  const EffectNode* target_node =
      tree.Node(layer->render_target()->EffectTreeIndex());
  const EffectNode* node = tree.Node(layer->effect_tree_index());
  if (node == target_node)
    return 1.f;

  float draw_opacity = 1.f;
  while (node != target_node) {
    draw_opacity *= tree.EffectiveOpacity(node);
    node = tree.parent(node);
  }
  return draw_opacity;
}

template <typename LayerType>
gfx::Transform ScreenSpaceTransformInternal(LayerType* layer,
                                            const TransformTree& tree) {
  gfx::Transform xform(1, 0, 0, 1, layer->offset_to_transform_parent().x(),
                       layer->offset_to_transform_parent().y());
  gfx::Transform ssxform = tree.ToScreen(layer->transform_tree_index());
  xform.ConcatTransform(ssxform);
  return xform;
}

void SetSurfaceClipRect(const ClipNode* parent_clip_node,
                        PropertyTrees* property_trees,
                        RenderSurfaceImpl* render_surface) {
  if (!render_surface->is_clipped()) {
    render_surface->SetClipRect(gfx::Rect());
    return;
  }

  const EffectTree& effect_tree = property_trees->effect_tree;
  const ClipTree& clip_tree = property_trees->clip_tree;
  const EffectNode* effect_node =
      effect_tree.Node(render_surface->EffectTreeIndex());
  const EffectNode* target_node = effect_tree.Node(effect_node->target_id);
  bool include_expanding_clips = false;
  if (render_surface->EffectTreeIndex() == EffectTree::kContentsRootNodeId) {
    render_surface->SetClipRect(
        ToEnclosingClipRect(clip_tree.Node(effect_node->clip_id)->clip));
  } else {
    ConditionalClip accumulated_clip_rect =
        ComputeAccumulatedClip(property_trees, include_expanding_clips,
                               effect_node->clip_id, target_node->id);
    render_surface->SetClipRect(
        ToEnclosingClipRect(accumulated_clip_rect.clip_rect));
  }
}

void SetSurfaceDrawTransform(const PropertyTrees* property_trees,
                             RenderSurfaceImpl* render_surface) {
  const TransformTree& transform_tree = property_trees->transform_tree;
  const EffectTree& effect_tree = property_trees->effect_tree;
  const TransformNode* transform_node =
      transform_tree.Node(render_surface->TransformTreeIndex());
  const EffectNode* effect_node =
      effect_tree.Node(render_surface->EffectTreeIndex());
  // The draw transform of root render surface is identity tranform.
  if (render_surface->EffectTreeIndex() == EffectTree::kContentsRootNodeId) {
    render_surface->SetDrawTransform(gfx::Transform());
    return;
  }

  gfx::Transform render_surface_transform;
  const EffectNode* target_effect_node =
      effect_tree.Node(effect_node->target_id);
  property_trees->GetToTarget(transform_node->id, target_effect_node->id,
                              &render_surface_transform);

  ConcatInverseSurfaceContentsScale(effect_node, &render_surface_transform);
  render_surface->SetDrawTransform(render_surface_transform);
}

gfx::Rect LayerVisibleRect(PropertyTrees* property_trees, LayerImpl* layer) {
  const EffectNode* effect_node =
      property_trees->effect_tree.Node(layer->effect_tree_index());
  int effect_ancestor_with_cache_render_surface =
      effect_node->closest_ancestor_with_cached_render_surface_id;
  int effect_ancestor_with_copy_request =
      effect_node->closest_ancestor_with_copy_request_id;
  int lower_effect_closest_ancestor =
      std::max(effect_ancestor_with_cache_render_surface,
               effect_ancestor_with_copy_request);
  bool non_root_copy_request_or_cache_render_surface =
      lower_effect_closest_ancestor > EffectTree::kContentsRootNodeId;
  gfx::Rect layer_content_rect = gfx::Rect(layer->bounds());

  gfx::RectF accumulated_clip_in_root_space;
  if (non_root_copy_request_or_cache_render_surface) {
    bool include_expanding_clips = true;
    ConditionalClip accumulated_clip = ComputeAccumulatedClip(
        property_trees, include_expanding_clips, layer->clip_tree_index(),
        lower_effect_closest_ancestor);
    if (!accumulated_clip.is_clipped)
      return layer_content_rect;
    accumulated_clip_in_root_space = accumulated_clip.clip_rect;
  } else {
    const ClipNode* clip_node =
        property_trees->clip_tree.Node(layer->clip_tree_index());
    accumulated_clip_in_root_space =
        clip_node->cached_accumulated_rect_in_screen_space;
  }

  const EffectNode* root_effect_node =
      non_root_copy_request_or_cache_render_surface
          ? property_trees->effect_tree.Node(lower_effect_closest_ancestor)
          : property_trees->effect_tree.Node(EffectTree::kContentsRootNodeId);
  ConditionalClip accumulated_clip_in_layer_space =
      ComputeTargetRectInLocalSpace(
          accumulated_clip_in_root_space, property_trees,
          root_effect_node->transform_id, layer->transform_tree_index(),
          root_effect_node->id);
  if (!accumulated_clip_in_layer_space.is_clipped) {
    return layer_content_rect;
  }
  gfx::RectF clip_in_layer_space = accumulated_clip_in_layer_space.clip_rect;
  clip_in_layer_space.Offset(-layer->offset_to_transform_parent());

  gfx::Rect visible_rect = ToEnclosingClipRect(clip_in_layer_space);
  if (layer->layer_tree_impl()->settings().allow_de_jelly_effect) {
    float padding_amount = viz::MaxDeJellyHeight();
    if (layer->IsAffectedByPageScale()) {
      padding_amount /= layer->layer_tree_impl()->current_page_scale_factor();
    }
    visible_rect.Inset(0.0f, -padding_amount);
  }
  visible_rect.Intersect(layer_content_rect);
  return visible_rect;
}

ConditionalClip LayerClipRect(PropertyTrees* property_trees, LayerImpl* layer) {
  const EffectTree* effect_tree = &property_trees->effect_tree;
  const EffectNode* effect_node = effect_tree->Node(layer->effect_tree_index());
  const EffectNode* target_node =
      effect_node->HasRenderSurface()
          ? effect_node
          : effect_tree->Node(effect_node->target_id);
  bool include_expanding_clips = false;
  return ComputeAccumulatedClip(property_trees, include_expanding_clips,
                                layer->clip_tree_index(), target_node->id);
}

std::pair<gfx::RRectF, bool> GetRoundedCornerRRect(
    const PropertyTrees* property_trees,
    int effect_tree_index,
    bool for_render_surface) {
  static const std::pair<gfx::RRectF, bool> kEmptyRoundedCornerInfo(
      gfx::RRectF(), false);
  const EffectTree* effect_tree = &property_trees->effect_tree;
  const EffectNode* effect_node = effect_tree->Node(effect_tree_index);
  const int target_id = effect_node->target_id;

  // Return empty rrect if this node has a render surface but the function call
  // was made for a non render surface.
  if (effect_node->HasRenderSurface() && !for_render_surface)
    return kEmptyRoundedCornerInfo;

  // Traverse the parent chain up to the render target to find a node which has
  // a rounded corner bounds set.
  const EffectNode* node = effect_node;
  bool found_rounded_corner = false;
  while (node) {
    if (!node->rounded_corner_bounds.IsEmpty()) {
      found_rounded_corner = true;
      break;
    }

    // If the iteration has reached a node in the parent chain that has a render
    // surface, then break. If this iteration is for a render surface to begin
    // with, then ensure |node| is a parent of |effect_node|.
    if (node->HasRenderSurface() &&
        (!for_render_surface || effect_node != node)) {
      break;
    }

    // Simply break if we reached a node that is the render target.
    if (node->id == target_id)
      break;

    node = effect_tree->parent(node);
  }

  // While traversing up the parent chain we did not find any node with a
  // rounded corner.
  if (!node || !found_rounded_corner)
    return kEmptyRoundedCornerInfo;

  gfx::Transform to_target;
  if (!property_trees->GetToTarget(node->transform_id, target_id, &to_target))
    return kEmptyRoundedCornerInfo;

  auto result =
      std::make_pair(node->rounded_corner_bounds, node->is_fast_rounded_corner);

  if (!to_target.TransformRRectF(&result.first))
    return kEmptyRoundedCornerInfo;

  return result;
}

void UpdateRenderTarget(EffectTree* effect_tree) {
  for (int i = EffectTree::kContentsRootNodeId;
       i < static_cast<int>(effect_tree->size()); ++i) {
    EffectNode* node = effect_tree->Node(i);
    if (i == EffectTree::kContentsRootNodeId) {
      // Render target of the node corresponding to root is itself.
      node->target_id = EffectTree::kContentsRootNodeId;
    } else if (effect_tree->parent(node)->HasRenderSurface()) {
      node->target_id = node->parent_id;
    } else {
      node->target_id = effect_tree->parent(node)->target_id;
    }
  }
}

void ComputeClips(PropertyTrees* property_trees) {
  DCHECK(!property_trees->transform_tree.needs_update());
  ClipTree* clip_tree = &property_trees->clip_tree;
  if (!clip_tree->needs_update())
    return;
  const int target_effect_id = EffectTree::kContentsRootNodeId;
  const int target_transform_id = TransformTree::kRootNodeId;
  const bool include_expanding_clips = true;
  for (int i = ClipTree::kViewportNodeId;
       i < static_cast<int>(clip_tree->size()); ++i) {
    ClipNode* clip_node = clip_tree->Node(i);
    // Clear the clip rect cache
    clip_node->cached_clip_rects->clear();
    if (clip_node->id == ClipTree::kViewportNodeId) {
      clip_node->cached_accumulated_rect_in_screen_space = clip_node->clip;
      continue;
    }
    ClipNode* parent_clip_node = clip_tree->parent(clip_node);
    DCHECK(parent_clip_node);
    gfx::RectF accumulated_clip =
        parent_clip_node->cached_accumulated_rect_in_screen_space;
    bool success = ApplyClipNodeToAccumulatedClip(
        property_trees, include_expanding_clips, target_effect_id,
        target_transform_id, clip_node, &accumulated_clip);
    DCHECK(success);
    clip_node->cached_accumulated_rect_in_screen_space = accumulated_clip;
  }
  clip_tree->set_needs_update(false);
}

void ComputeSurfaceDrawProperties(PropertyTrees* property_trees,
                                  RenderSurfaceImpl* render_surface) {
  SetSurfaceIsClipped(property_trees->clip_tree, render_surface);
  SetSurfaceDrawOpacity(property_trees->effect_tree, render_surface);
  SetSurfaceDrawTransform(property_trees, render_surface);

  render_surface->SetRoundedCornerRRect(
      GetRoundedCornerRRect(property_trees, render_surface->EffectTreeIndex(),
                            /*for_render_surface*/ true)
          .first);
  render_surface->SetScreenSpaceTransform(
      property_trees->ToScreenSpaceTransformWithoutSurfaceContentsScale(
          render_surface->TransformTreeIndex(),
          render_surface->EffectTreeIndex()));

  const ClipNode* clip_node =
      property_trees->clip_tree.Node(render_surface->ClipTreeIndex());
  SetSurfaceClipRect(clip_node, property_trees, render_surface);
}

void AddSurfaceToRenderSurfaceList(RenderSurfaceImpl* render_surface,
                                   RenderSurfaceList* render_surface_list,
                                   PropertyTrees* property_trees) {
  // |render_surface| must appear after its target, so first make sure its
  // target is in the list.
  RenderSurfaceImpl* target = render_surface->render_target();
  bool is_root =
      render_surface->EffectTreeIndex() == EffectTree::kContentsRootNodeId;
  if (!is_root && !target->is_render_surface_list_member()) {
    AddSurfaceToRenderSurfaceList(target, render_surface_list, property_trees);
  }
  render_surface->ClearAccumulatedContentRect();
  render_surface_list->push_back(render_surface);
  render_surface->set_is_render_surface_list_member(true);
  if (is_root) {
    // The root surface does not contribute to any other surface, it has no
    // target.
    render_surface->set_contributes_to_drawn_surface(false);
  } else {
    bool contributes_to_drawn_surface =
        property_trees->effect_tree.ContributesToDrawnSurface(
            render_surface->EffectTreeIndex());
    render_surface->set_contributes_to_drawn_surface(
        contributes_to_drawn_surface);
  }

  ComputeSurfaceDrawProperties(property_trees, render_surface);

  // Ignore occlusion from outside the surface when surface contents need to be
  // fully drawn. Layers with copy-request need to be complete.  We could be
  // smarter about layers with filters that move pixels and exclude regions
  // where both layers and the filters are occluded, but this seems like
  // overkill.
  // TODO(senorblanco): make this smarter for the SkImageFilter case (check for
  // pixel-moving filters)
  const FilterOperations& filters = render_surface->Filters();
  bool is_occlusion_immune = render_surface->HasCopyRequest() ||
                             render_surface->ShouldCacheRenderSurface() ||
                             filters.HasReferenceFilter() ||
                             filters.HasFilterThatMovesPixels();
  if (is_occlusion_immune) {
    render_surface->SetNearestOcclusionImmuneAncestor(render_surface);
  } else if (is_root) {
    render_surface->SetNearestOcclusionImmuneAncestor(nullptr);
  } else {
    render_surface->SetNearestOcclusionImmuneAncestor(
        render_surface->render_target()->nearest_occlusion_immune_ancestor());
  }
}

bool SkipForInvertibility(const LayerImpl* layer,
                          PropertyTrees* property_trees) {
  const TransformNode* transform_node =
      property_trees->transform_tree.Node(layer->transform_tree_index());
  const EffectNode* effect_node =
      property_trees->effect_tree.Node(layer->effect_tree_index());
  bool non_root_copy_request =
      effect_node->closest_ancestor_with_copy_request_id >
      EffectTree::kContentsRootNodeId;
  gfx::Transform from_target;
  // If there is a copy request, we check the invertibility of the transform
  // between the node corresponding to the layer and the node corresponding to
  // the copy request. Otherwise, we are interested in the invertibility of
  // screen space transform which is already cached on the transform node.
  return non_root_copy_request
             ? !property_trees->GetFromTarget(
                   layer->transform_tree_index(),
                   effect_node->closest_ancestor_with_copy_request_id,
                   &from_target)
             : !transform_node->ancestors_are_invertible;
}

void ComputeInitialRenderSurfaceList(LayerTreeImpl* layer_tree_impl,
                                     PropertyTrees* property_trees,
                                     RenderSurfaceList* render_surface_list) {
  EffectTree& effect_tree = property_trees->effect_tree;
  for (int i = EffectTree::kContentsRootNodeId;
       i < static_cast<int>(effect_tree.size()); ++i) {
    if (RenderSurfaceImpl* render_surface = effect_tree.GetRenderSurface(i)) {
      render_surface->set_is_render_surface_list_member(false);
      render_surface->reset_num_contributors();
    }
  }

  RenderSurfaceImpl* root_surface =
      effect_tree.GetRenderSurface(EffectTree::kContentsRootNodeId);
  // The root surface always gets added to the render surface  list.
  AddSurfaceToRenderSurfaceList(root_surface, render_surface_list,
                                property_trees);
  // For all non-skipped layers, add their target to the render surface list if
  // it's not already been added, and add their content rect to the target
  // surface's accumulated content rect.
  for (LayerImpl* layer : *layer_tree_impl) {
    DCHECK(layer);
    layer->EnsureValidPropertyTreeIndices();

    layer->set_contributes_to_drawn_render_surface(false);
    layer->set_raster_even_if_not_drawn(false);

    bool is_root = layer_tree_impl->IsRootLayer(layer);

    bool skip_draw_properties_computation =
        LayerShouldBeSkippedForDrawPropertiesComputation(
            layer, property_trees->transform_tree, property_trees->effect_tree);

    bool skip_for_invertibility = SkipForInvertibility(layer, property_trees);

    bool skip_layer = !is_root && (skip_draw_properties_computation ||
                                   skip_for_invertibility);

    layer->set_raster_even_if_not_drawn(skip_for_invertibility &&
                                        !skip_draw_properties_computation);
    if (skip_layer)
      continue;

    bool layer_is_drawn =
        property_trees->effect_tree.Node(layer->effect_tree_index())->is_drawn;
    bool layer_should_be_drawn =
        LayerNeedsUpdate(layer, layer_is_drawn, property_trees);
    if (!layer_should_be_drawn)
      continue;

    RenderSurfaceImpl* render_target = layer->render_target();
    if (!render_target->is_render_surface_list_member()) {
      AddSurfaceToRenderSurfaceList(render_target, render_surface_list,
                                    property_trees);
    }

    layer->set_contributes_to_drawn_render_surface(true);

    // The layer contributes its drawable content rect to its render target.
    render_target->AccumulateContentRectFromContributingLayer(layer);
    render_target->increment_num_contributors();
  }
}

void ComputeSurfaceContentRects(PropertyTrees* property_trees,
                                RenderSurfaceList* render_surface_list,
                                int max_texture_size) {
  // Walk the list backwards, accumulating each surface's content rect into its
  // target's content rect.
  for (RenderSurfaceImpl* render_surface :
       base::Reversed(*render_surface_list)) {
    if (render_surface->EffectTreeIndex() == EffectTree::kContentsRootNodeId) {
      // The root surface's content rect is always the entire viewport.
      render_surface->SetContentRectToViewport();
      continue;
    }

    // Now all contributing drawable content rect has been accumulated to this
    // render surface, calculate the content rect.
    render_surface->CalculateContentRectFromAccumulatedContentRect(
        max_texture_size);

    // Now the render surface's content rect is calculated correctly, it could
    // contribute to its render target.
    RenderSurfaceImpl* render_target = render_surface->render_target();
    DCHECK(render_target->is_render_surface_list_member());
    render_target->AccumulateContentRectFromContributingRenderSurface(
        render_surface);
    render_target->increment_num_contributors();
  }
}

void ComputeListOfNonEmptySurfaces(LayerTreeImpl* layer_tree_impl,
                                   PropertyTrees* property_trees,
                                   RenderSurfaceList* initial_surface_list,
                                   RenderSurfaceList* final_surface_list) {
  // Walk the initial surface list forwards. The root surface and each
  // surface with a non-empty content rect go into the final render surface
  // layer list. Surfaces with empty content rects or whose target isn't in
  // the final list do not get added to the final list.
  bool removed_surface = false;
  for (RenderSurfaceImpl* surface : *initial_surface_list) {
    bool is_root =
        surface->EffectTreeIndex() == EffectTree::kContentsRootNodeId;
    RenderSurfaceImpl* target_surface = surface->render_target();
    if (!is_root && (surface->content_rect().IsEmpty() ||
                     !target_surface->is_render_surface_list_member())) {
      surface->set_is_render_surface_list_member(false);
      removed_surface = true;
      target_surface->decrement_num_contributors();
      continue;
    }
    final_surface_list->push_back(surface);
  }
  if (removed_surface) {
    for (LayerImpl* layer : *layer_tree_impl) {
      if (layer->contributes_to_drawn_render_surface()) {
        RenderSurfaceImpl* render_target = layer->render_target();
        if (!render_target->is_render_surface_list_member()) {
          layer->set_contributes_to_drawn_render_surface(false);
          render_target->decrement_num_contributors();
        }
      }
    }
  }
}

void CalculateRenderSurfaceLayerList(LayerTreeImpl* layer_tree_impl,
                                     PropertyTrees* property_trees,
                                     RenderSurfaceList* render_surface_list,
                                     const int max_texture_size) {
  RenderSurfaceList initial_render_surface_list;

  // First compute a list that might include surfaces that later turn out to
  // have an empty content rect. After surface content rects are computed,
  // produce a final list that omits empty surfaces.
  ComputeInitialRenderSurfaceList(layer_tree_impl, property_trees,
                                  &initial_render_surface_list);
  ComputeSurfaceContentRects(property_trees, &initial_render_surface_list,
                             max_texture_size);
  ComputeListOfNonEmptySurfaces(layer_tree_impl, property_trees,
                                &initial_render_surface_list,
                                render_surface_list);
}

void RecordRenderSurfaceReasonsForTracing(
    const PropertyTrees* property_trees,
    const RenderSurfaceList* render_surface_list) {
  static const auto* tracing_enabled =
      TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED("cc");
  if (!*tracing_enabled ||
      // Don't output single root render surface.
      render_surface_list->size() <= 1)
    return;

  TRACE_EVENT_INSTANT1("cc", "RenderSurfaceReasonCount",
                       TRACE_EVENT_SCOPE_THREAD, "total",
                       render_surface_list->size());

  // kTest is the last value which is not included for tracing.
  constexpr auto kNumReasons = static_cast<size_t>(RenderSurfaceReason::kTest);
  int reason_counts[kNumReasons] = {0};
  for (const auto* render_surface : *render_surface_list) {
    const auto* effect_node =
        property_trees->effect_tree.Node(render_surface->EffectTreeIndex());
    reason_counts[static_cast<size_t>(effect_node->render_surface_reason)]++;
  }
  for (size_t i = 0; i < kNumReasons; i++) {
    if (!reason_counts[i])
      continue;
    TRACE_EVENT_INSTANT1(
        "cc", "RenderSurfaceReasonCount", TRACE_EVENT_SCOPE_THREAD,
        RenderSurfaceReasonToString(static_cast<RenderSurfaceReason>(i)),
        reason_counts[i]);
  }
}

void UpdateElasticOverscroll(
    PropertyTrees* property_trees,
    TransformNode* overscroll_elasticity_transform_node,
    const gfx::Vector2dF& elastic_overscroll) {
  if (!overscroll_elasticity_transform_node) {
    DCHECK(elastic_overscroll.IsZero());
    return;
  }

  if (overscroll_elasticity_transform_node->scroll_offset ==
      gfx::ScrollOffset(elastic_overscroll))
    return;

  overscroll_elasticity_transform_node->scroll_offset =
      gfx::ScrollOffset(elastic_overscroll);
  overscroll_elasticity_transform_node->needs_local_transform_update = true;
  property_trees->transform_tree.set_needs_update(true);
}

void ComputeDrawPropertiesOfVisibleLayers(const LayerImplList* layer_list,
                                          PropertyTrees* property_trees) {
  // Compute transforms
  for (LayerImpl* layer : *layer_list) {
    const TransformNode* transform_node =
        property_trees->transform_tree.Node(layer->transform_tree_index());

    layer->draw_properties().screen_space_transform =
        ScreenSpaceTransformInternal(layer, property_trees->transform_tree);
    layer->draw_properties().target_space_transform = DrawTransform(
        layer, property_trees->transform_tree, property_trees->effect_tree);
    layer->draw_properties().screen_space_transform_is_animating =
        transform_node->to_screen_is_potentially_animated;
    auto rounded_corner_info =
        GetRoundedCornerRRect(property_trees, layer->effect_tree_index(),
                              /*from_render_surface*/ false);
    layer->draw_properties().rounded_corner_bounds = rounded_corner_info.first;
    layer->draw_properties().is_fast_rounded_corner =
        rounded_corner_info.second;
  }

  // Compute effects and determine if render surfaces have contributing layers
  // that escape clip.
  for (LayerImpl* layer : *layer_list) {
    layer->draw_properties().opacity =
        LayerDrawOpacity(layer, property_trees->effect_tree);
    RenderSurfaceImpl* render_target = layer->render_target();
    int lca_clip_id = LowestCommonAncestor(layer->clip_tree_index(),
                                           render_target->ClipTreeIndex(),
                                           &property_trees->clip_tree);
    if (lca_clip_id != render_target->ClipTreeIndex()) {
      SetHasContributingLayerThatEscapesClip(lca_clip_id,
                                             render_target->EffectTreeIndex(),
                                             &property_trees->effect_tree);
    }
  }

  // Compute clips and visible rects
  for (LayerImpl* layer : *layer_list) {
    ConditionalClip clip = LayerClipRect(property_trees, layer);
    // is_clipped should be set before visible rect computation as it is used
    // there.
    layer->draw_properties().is_clipped = clip.is_clipped;
    layer->draw_properties().clip_rect = ToEnclosingClipRect(clip.clip_rect);
    layer->draw_properties().visible_layer_rect =
        LayerVisibleRect(property_trees, layer);
  }

  // Compute drawable content rects
  for (LayerImpl* layer : *layer_list) {
    gfx::Rect bounds_in_target_space = MathUtil::MapEnclosingClippedRect(
        layer->draw_properties().target_space_transform,
        gfx::Rect(layer->bounds()));
    layer->draw_properties().drawable_content_rect = LayerDrawableContentRect(
        layer, bounds_in_target_space, layer->draw_properties().clip_rect);
  }
}

}  // namespace

void ConcatInverseSurfaceContentsScale(const EffectNode* effect_node,
                                       gfx::Transform* transform) {
  DCHECK(effect_node->HasRenderSurface());
  if (effect_node->surface_contents_scale.x() != 0.0 &&
      effect_node->surface_contents_scale.y() != 0.0)
    transform->Scale(1.0 / effect_node->surface_contents_scale.x(),
                     1.0 / effect_node->surface_contents_scale.y());
}

void FindLayersThatNeedUpdates(LayerTreeHost* layer_tree_host,
                               LayerList* update_layer_list) {
  const PropertyTrees* property_trees = layer_tree_host->property_trees();
  const TransformTree& transform_tree = property_trees->transform_tree;
  const EffectTree& effect_tree = property_trees->effect_tree;
  for (auto* layer : *layer_tree_host) {
    if (!IsRootLayer(layer) && LayerShouldBeSkippedForDrawPropertiesComputation(
                                   layer, transform_tree, effect_tree))
      continue;

    bool layer_is_drawn =
        effect_tree.Node(layer->effect_tree_index())->is_drawn;

    if (LayerNeedsUpdate(layer, layer_is_drawn, property_trees)) {
      update_layer_list->push_back(layer);
    }
  }
}

void FindLayersThatNeedUpdates(LayerTreeImpl* layer_tree_impl,
                               std::vector<LayerImpl*>* visible_layer_list) {
  const PropertyTrees* property_trees = layer_tree_impl->property_trees();
  const TransformTree& transform_tree = property_trees->transform_tree;
  const EffectTree& effect_tree = property_trees->effect_tree;

  for (auto* layer_impl : *layer_tree_impl) {
    DCHECK(layer_impl);
    DCHECK(layer_impl->layer_tree_impl());
    layer_impl->EnsureValidPropertyTreeIndices();

    if (!IsRootLayer(layer_impl) &&
        LayerShouldBeSkippedForDrawPropertiesComputation(
            layer_impl, transform_tree, effect_tree))
      continue;

    bool layer_is_drawn =
        effect_tree.Node(layer_impl->effect_tree_index())->is_drawn;

    if (LayerNeedsUpdate(layer_impl, layer_is_drawn, property_trees))
      visible_layer_list->push_back(layer_impl);
  }
}

void ComputeTransforms(TransformTree* transform_tree) {
  if (!transform_tree->needs_update()) {
#if DCHECK_IS_ON()
    // If the transform tree does not need an update, no TransformNode should
    // need a local transform update.
    for (int i = TransformTree::kContentsRootNodeId;
         i < static_cast<int>(transform_tree->size()); ++i) {
      DCHECK(!transform_tree->Node(i)->needs_local_transform_update);
    }
#endif
    return;
  }
  for (int i = TransformTree::kContentsRootNodeId;
       i < static_cast<int>(transform_tree->size()); ++i)
    transform_tree->UpdateTransforms(i);
  transform_tree->set_needs_update(false);
}

void ComputeEffects(EffectTree* effect_tree) {
  if (!effect_tree->needs_update())
    return;
  for (int i = EffectTree::kContentsRootNodeId;
       i < static_cast<int>(effect_tree->size()); ++i)
    effect_tree->UpdateEffects(i);
  effect_tree->set_needs_update(false);
}

void UpdatePropertyTrees(LayerTreeHost* layer_tree_host) {
  DCHECK(layer_tree_host);
  auto* property_trees = layer_tree_host->property_trees();
  DCHECK(property_trees);
  if (property_trees->transform_tree.needs_update()) {
    property_trees->clip_tree.set_needs_update(true);
    property_trees->effect_tree.set_needs_update(true);
  }
  ComputeTransforms(&property_trees->transform_tree);
  ComputeEffects(&property_trees->effect_tree);
  // Computation of clips uses ToScreen which is updated while computing
  // transforms. So, ComputeTransforms should be before ComputeClips.
  ComputeClips(property_trees);
}

void UpdatePropertyTreesAndRenderSurfaces(LayerImpl* root_layer,
                                          PropertyTrees* property_trees) {
  if (property_trees->transform_tree.needs_update()) {
    property_trees->clip_tree.set_needs_update(true);
    property_trees->effect_tree.set_needs_update(true);
  }
  UpdateRenderTarget(&property_trees->effect_tree);

  ComputeTransforms(&property_trees->transform_tree);
  ComputeEffects(&property_trees->effect_tree);
  // Computation of clips uses ToScreen which is updated while computing
  // transforms. So, ComputeTransforms should be before ComputeClips.
  ComputeClips(property_trees);
}

gfx::Transform DrawTransform(const LayerImpl* layer,
                             const TransformTree& transform_tree,
                             const EffectTree& effect_tree) {
  // TransformTree::ToTarget computes transform between the layer's transform
  // node and surface's transform node and scales it by the surface's content
  // scale.
  gfx::Transform xform;
  transform_tree.property_trees()->GetToTarget(
      layer->transform_tree_index(), layer->render_target_effect_tree_index(),
      &xform);
  xform.Translate(layer->offset_to_transform_parent().x(),
                  layer->offset_to_transform_parent().y());
  return xform;
}

gfx::Transform ScreenSpaceTransform(const Layer* layer,
                                    const TransformTree& tree) {
  return ScreenSpaceTransformInternal(layer, tree);
}

gfx::Transform ScreenSpaceTransform(const LayerImpl* layer,
                                    const TransformTree& tree) {
  return ScreenSpaceTransformInternal(layer, tree);
}

void UpdatePageScaleFactor(PropertyTrees* property_trees,
                           TransformNode* page_scale_node,
                           float page_scale_factor) {
  // TODO(wjmaclean): Once Issue #845097 is resolved, we can change the nullptr
  // check below to a DCHECK.
  if (property_trees->transform_tree.page_scale_factor() == page_scale_factor ||
      !page_scale_node) {
    return;
  }

  property_trees->transform_tree.set_page_scale_factor(page_scale_factor);

  page_scale_node->local.MakeIdentity();
  page_scale_node->local.Scale(page_scale_factor, page_scale_factor);

  page_scale_node->needs_local_transform_update = true;
  property_trees->transform_tree.set_needs_update(true);
}

void CalculateDrawProperties(
    LayerTreeImpl* layer_tree_impl,
    RenderSurfaceList* output_render_surface_list,
    LayerImplList* output_update_layer_list_for_testing) {
  output_render_surface_list->clear();

  LayerImplList visible_layer_list;
  // Since page scale and elastic overscroll are SyncedProperties, changes
  // on the active tree immediately affect the pending tree, so instead of
  // trying to update property trees whenever these values change, we
  // update property trees before using them.

  // We should never be setting a non-unit page scale factor on an oopif
  // subframe ... if we attempt this log it and fail.
  // TODO(wjmaclean): Remove as part of conditions for closing the bug.
  // https://crbug.com/845097
  PropertyTrees* property_trees = layer_tree_impl->property_trees();
  if (layer_tree_impl->current_page_scale_factor() !=
          property_trees->transform_tree.page_scale_factor() &&
      !layer_tree_impl->PageScaleTransformNode()) {
    LOG(ERROR) << "Setting PageScale on subframe: new psf = "
               << layer_tree_impl->page_scale_factor() << ", old psf = "
               << property_trees->transform_tree.page_scale_factor()
               << ", in_oopif = "
               << layer_tree_impl->settings().is_layer_tree_for_subframe;
    NOTREACHED();
  }

  UpdatePageScaleFactor(property_trees,
                        layer_tree_impl->PageScaleTransformNode(),
                        layer_tree_impl->current_page_scale_factor());
  UpdateElasticOverscroll(property_trees,
                          layer_tree_impl->OverscrollElasticityTransformNode(),
                          layer_tree_impl->current_elastic_overscroll());
  // Similarly, the device viewport and device transform are shared
  // by both trees.
  property_trees->clip_tree.SetViewportClip(
      gfx::RectF(layer_tree_impl->GetDeviceViewport()));
  property_trees->transform_tree.SetRootScaleAndTransform(
      layer_tree_impl->device_scale_factor(), layer_tree_impl->DrawTransform());
  UpdatePropertyTreesAndRenderSurfaces(layer_tree_impl->root_layer(),
                                       property_trees);

  {
    TRACE_EVENT0("cc", "draw_property_utils::FindLayersThatNeedUpdates");
    FindLayersThatNeedUpdates(layer_tree_impl, &visible_layer_list);
  }

  {
    TRACE_EVENT1("cc",
                 "draw_property_utils::ComputeDrawPropertiesOfVisibleLayers",
                 "visible_layers", visible_layer_list.size());
    ComputeDrawPropertiesOfVisibleLayers(&visible_layer_list, property_trees);
  }

  {
    TRACE_EVENT0("cc", "CalculateRenderSurfaceLayerList");
    CalculateRenderSurfaceLayerList(layer_tree_impl, property_trees,
                                    output_render_surface_list,
                                    layer_tree_impl->max_texture_size());
  }
  RecordRenderSurfaceReasonsForTracing(property_trees,
                                       output_render_surface_list);

  // A root layer render_surface should always exist after
  // CalculateDrawProperties.
  DCHECK(property_trees->effect_tree.GetRenderSurface(
      EffectTree::kContentsRootNodeId));

  if (output_update_layer_list_for_testing)
    *output_update_layer_list_for_testing = std::move(visible_layer_list);
}

}  // namespace draw_property_utils

}  // namespace cc
