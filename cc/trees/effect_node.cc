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
      backdrop_filter_quality(1.f),
      blend_mode(SkBlendMode::kSrcOver),
      cache_render_surface(false),
      has_copy_request(false),
      hidden_by_backface_visibility(false),
      double_sided(true),
      trilinear_filtering(false),
      is_drawn(true),
      subtree_hidden(false),
      has_potential_filter_animation(false),
      has_potential_backdrop_filter_animation(false),
      has_potential_opacity_animation(false),
      is_currently_animating_filter(false),
      is_currently_animating_backdrop_filter(false),
      is_currently_animating_opacity(false),
      has_masking_child(false),
      effect_changed(false),
      subtree_has_copy_request(false),
      is_fast_rounded_corner(false),
      render_surface_reason(RenderSurfaceReason::kNone),
      transform_id(0),
      clip_id(0),
      target_id(1),
      closest_ancestor_with_cached_render_surface_id(-1),
      closest_ancestor_with_copy_request_id(-1) {}

EffectNode::EffectNode(const EffectNode& other) = default;

EffectNode::~EffectNode() = default;

bool EffectNode::operator==(const EffectNode& other) const {
  return id == other.id && parent_id == other.parent_id &&
         stable_id == other.stable_id && opacity == other.opacity &&
         screen_space_opacity == other.screen_space_opacity &&
         backdrop_filter_quality == other.backdrop_filter_quality &&
         cache_render_surface == other.cache_render_surface &&
         has_copy_request == other.has_copy_request &&
         filters == other.filters &&
         backdrop_filters == other.backdrop_filters &&
         backdrop_filter_bounds == other.backdrop_filter_bounds &&
         backdrop_mask_element_id == other.backdrop_mask_element_id &&
         filters_origin == other.filters_origin &&
         rounded_corner_bounds == other.rounded_corner_bounds &&
         is_fast_rounded_corner == other.is_fast_rounded_corner &&
         // The specific reason is just for tracing/testing/debugging, so just
         // check whether a render surface is needed.
         HasRenderSurface() == other.HasRenderSurface() &&
         blend_mode == other.blend_mode &&
         surface_contents_scale == other.surface_contents_scale &&
         hidden_by_backface_visibility == other.hidden_by_backface_visibility &&
         double_sided == other.double_sided &&
         trilinear_filtering == other.trilinear_filtering &&
         is_drawn == other.is_drawn && subtree_hidden == other.subtree_hidden &&
         has_potential_filter_animation ==
             other.has_potential_filter_animation &&
         has_potential_backdrop_filter_animation ==
             other.has_potential_backdrop_filter_animation &&
         has_potential_opacity_animation ==
             other.has_potential_opacity_animation &&
         is_currently_animating_filter == other.is_currently_animating_filter &&
         is_currently_animating_backdrop_filter ==
             other.is_currently_animating_backdrop_filter &&
         is_currently_animating_opacity ==
             other.is_currently_animating_opacity &&
         has_masking_child == other.has_masking_child &&
         effect_changed == other.effect_changed &&
         subtree_has_copy_request == other.subtree_has_copy_request &&
         transform_id == other.transform_id && clip_id == other.clip_id &&
         target_id == other.target_id &&
         closest_ancestor_with_cached_render_surface_id ==
             other.closest_ancestor_with_cached_render_surface_id &&
         closest_ancestor_with_copy_request_id ==
             other.closest_ancestor_with_copy_request_id;
}

const char* RenderSurfaceReasonToString(RenderSurfaceReason reason) {
  switch (reason) {
    case RenderSurfaceReason::kNone:
      return "none";
    case RenderSurfaceReason::kRoot:
      return "root";
    case RenderSurfaceReason::k3dTransformFlattening:
      return "3d transform flattening";
    case RenderSurfaceReason::kBlendMode:
      return "blend mode";
    case RenderSurfaceReason::kBlendModeDstIn:
      return "blend mode kDstIn";
    case RenderSurfaceReason::kOpacity:
      return "opacity";
    case RenderSurfaceReason::kOpacityAnimation:
      return "opacity animation";
    case RenderSurfaceReason::kFilter:
      return "filter";
    case RenderSurfaceReason::kFilterAnimation:
      return "filter animation";
    case RenderSurfaceReason::kBackdropFilter:
      return "backdrop filter";
    case RenderSurfaceReason::kBackdropFilterAnimation:
      return "backdrop filter animation";
    case RenderSurfaceReason::kRoundedCorner:
      return "rounded corner";
    case RenderSurfaceReason::kClipPath:
      return "clip path";
    case RenderSurfaceReason::kClipAxisAlignment:
      return "clip axis alignment";
    case RenderSurfaceReason::kMask:
      return "mask";
    case RenderSurfaceReason::kTrilinearFiltering:
      return "trilinear filtering";
    case RenderSurfaceReason::kCache:
      return "cache";
    case RenderSurfaceReason::kCopyRequest:
      return "copy request";
    case RenderSurfaceReason::kTest:
      return "test";
    default:
      NOTREACHED() << static_cast<int>(reason);
      return "";
  }
}

void EffectNode::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("backdrop_mask_element_id",
                    backdrop_mask_element_id.GetStableId());
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  value->SetInteger("stable_id", stable_id);
  value->SetDouble("opacity", opacity);
  if (!backdrop_filters.IsEmpty()) {
    value->SetString("backdrop_filters", backdrop_filters.ToString());
  }
  value->SetDouble("backdrop_filter_quality", backdrop_filter_quality);
  value->SetBoolean("is_fast_rounded_corner", is_fast_rounded_corner);
  if (!rounded_corner_bounds.IsEmpty()) {
    MathUtil::AddToTracedValue("rounded_corner_bounds", rounded_corner_bounds,
                               value);
  }
  value->SetString("blend_mode", SkBlendMode_Name(blend_mode));
  value->SetBoolean("cache_render_surface", cache_render_surface);
  value->SetBoolean("has_copy_request", has_copy_request);
  value->SetBoolean("double_sided", double_sided);
  value->SetBoolean("trilinear_filtering", trilinear_filtering);
  value->SetBoolean("is_drawn", is_drawn);
  value->SetBoolean("has_potential_filter_animation",
                    has_potential_filter_animation);
  value->SetBoolean("has_potential_backdrop_filter_animation",
                    has_potential_backdrop_filter_animation);
  value->SetBoolean("has_potential_opacity_animation",
                    has_potential_opacity_animation);
  value->SetBoolean("has_masking_child", has_masking_child);
  value->SetBoolean("effect_changed", effect_changed);
  value->SetBoolean("subtree_has_copy_request", subtree_has_copy_request);
  value->SetString("render_surface_reason",
                   RenderSurfaceReasonToString(render_surface_reason));
  value->SetInteger("transform_id", transform_id);
  value->SetInteger("clip_id", clip_id);
  value->SetInteger("target_id", target_id);
  value->SetInteger("closest_ancestor_with_cached_render_surface_id",
                    closest_ancestor_with_cached_render_surface_id);
  value->SetInteger("closest_ancestor_with_copy_request_id",
                    closest_ancestor_with_copy_request_id);
}

}  // namespace cc
