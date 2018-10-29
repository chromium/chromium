// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/effect_node.h"
#include "base/trace_event/traced_value.h"
#include "cc/layers/layer.h"
#include "cc/trees/property_tree.h"

namespace cc {

EffectNode::EffectNode()
    : id(EffectTree::kInvalidNodeId),
      parent_id(EffectTree::kInvalidNodeId),
      stable_id(INVALID_STABLE_ID),
      opacity(1.f),
      screen_space_opacity(1.f),
      blend_mode(SkBlendMode::kSrcOver),
      has_render_surface(false),
      cache_render_surface(false),
      has_copy_request(false),
      hidden_by_backface_visibility(false),
      double_sided(false),
      trilinear_filtering(false),
      is_drawn(true),
      subtree_hidden(false),
      has_potential_filter_animation(false),
      has_potential_opacity_animation(false),
      is_currently_animating_filter(false),
      is_currently_animating_opacity(false),
      has_masking_child(false),
      effect_changed(false),
      subtree_has_copy_request(false),
      transform_id(0),
      clip_id(0),
      target_id(1),
      mask_layer_id(Layer::INVALID_ID),
      closest_ancestor_with_cached_render_surface_id(-1),
      closest_ancestor_with_copy_request_id(-1) {}

EffectNode::EffectNode(const EffectNode& other) = default;

bool EffectNode::operator==(const EffectNode& other) const {
  return id == other.id && parent_id == other.parent_id &&
         stable_id == other.stable_id && opacity == other.opacity &&
         screen_space_opacity == other.screen_space_opacity &&
         has_render_surface == other.has_render_surface &&
         cache_render_surface == other.cache_render_surface &&
         has_copy_request == other.has_copy_request &&
         filters == other.filters &&
         backdrop_filters == other.backdrop_filters &&
         filters_origin == other.filters_origin &&
         blend_mode == other.blend_mode &&
         surface_contents_scale == other.surface_contents_scale &&
         unscaled_mask_target_size == other.unscaled_mask_target_size &&
         hidden_by_backface_visibility == other.hidden_by_backface_visibility &&
         double_sided == other.double_sided &&
         trilinear_filtering == other.trilinear_filtering &&
         is_drawn == other.is_drawn && subtree_hidden == other.subtree_hidden &&
         has_potential_filter_animation ==
             other.has_potential_filter_animation &&
         has_potential_opacity_animation ==
             other.has_potential_opacity_animation &&
         is_currently_animating_filter == other.is_currently_animating_filter &&
         is_currently_animating_opacity ==
             other.is_currently_animating_opacity &&
         has_masking_child == other.has_masking_child &&
         effect_changed == other.effect_changed &&
         subtree_has_copy_request == other.subtree_has_copy_request &&
         transform_id == other.transform_id && clip_id == other.clip_id &&
         target_id == other.target_id && mask_layer_id == other.mask_layer_id &&
         closest_ancestor_with_cached_render_surface_id ==
             other.closest_ancestor_with_cached_render_surface_id &&
         closest_ancestor_with_copy_request_id ==
             other.closest_ancestor_with_copy_request_id;
}

void EffectNode::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  value->SetInteger("stable_id", stable_id);
  value->SetDouble("opacity", opacity);
  value->SetString("blend_mode", SkBlendMode_Name(blend_mode));
  value->SetBoolean("has_render_surface", has_render_surface);
  value->SetBoolean("cache_render_surface", cache_render_surface);
  value->SetBoolean("has_copy_request", has_copy_request);
  value->SetBoolean("double_sided", double_sided);
  value->SetBoolean("trilinear_filtering", trilinear_filtering);
  value->SetBoolean("is_drawn", is_drawn);
  value->SetBoolean("has_potential_filter_animation",
                    has_potential_filter_animation);
  value->SetBoolean("has_potential_opacity_animation",
                    has_potential_opacity_animation);
  value->SetBoolean("effect_changed", effect_changed);
  value->SetInteger("subtree_has_copy_request", subtree_has_copy_request);
  value->SetInteger("transform_id", transform_id);
  value->SetInteger("clip_id", clip_id);
  value->SetInteger("target_id", target_id);
  value->SetInteger("mask_layer_id", mask_layer_id);
  value->SetInteger("closest_ancestor_with_cached_render_surface_id",
                    closest_ancestor_with_cached_render_surface_id);
  value->SetInteger("closest_ancestor_with_copy_request_id",
                    closest_ancestor_with_copy_request_id);
}

}  // namespace cc
