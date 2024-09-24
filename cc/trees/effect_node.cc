// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/effect_node.h"

#include "base/notreached.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/math_util.h"
#include "cc/layers/layer.h"
#include "cc/trees/property_tree.h"

namespace cc {

EffectNode::EffectNode() = default;
EffectNode::EffectNode(const EffectNode& other) = default;
EffectNode::~EffectNode() = default;

#if DCHECK_IS_ON()
bool EffectNode::operator==(const EffectNode& other) const = default;
#endif  // DCHECK_IS_ON()

const char* RenderSurfaceReasonToString(RenderSurfaceReason reason) {
  switch (reason) {
    case RenderSurfaceReason::kNone:
      return "none";
    case RenderSurfaceReason::kRoot:
      return "root";
    case RenderSurfaceReason::k3dTransformFlattening:
      return "3d transform flattening";
    case RenderSurfaceReason::kBackdropScope:
      return "backdrop scope";
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
    case RenderSurfaceReason::kGradientMask:
      return "gradient mask";
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
    case RenderSurfaceReason::kMirrored:
      return "mirrored";
    case RenderSurfaceReason::kSubtreeIsBeingCaptured:
      return "subtree being captured";
    case RenderSurfaceReason::kViewTransitionParticipant:
      return "view transition participant";
    case RenderSurfaceReason::kTest:
      return "test";
    default:
      NOTREACHED() << static_cast<int>(reason);
  }
}

void EffectNode::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetString("backdrop_mask_element_id",
                   backdrop_mask_element_id.ToString());
  value->SetInteger("id", id);
  value->SetInteger("parent_id", parent_id);
  value->SetString("element_id", element_id.ToString());
  value->SetDouble("opacity", opacity);
  if (!filters.IsEmpty())
    value->SetString("filters", filters.ToString());
  if (!backdrop_filters.IsEmpty())
    value->SetString("backdrop_filters", backdrop_filters.ToString());
  value->SetDouble("backdrop_filter_quality", backdrop_filter_quality);
  value->SetBoolean("node_or_ancestor_has_filters",
                    node_or_ancestor_has_filters);
  if (!mask_filter_info.IsEmpty()) {
    MathUtil::AddToTracedValue("mask_filter_bounds", mask_filter_info.bounds(),
                               value);
    if (mask_filter_info.HasRoundedCorners()) {
      MathUtil::AddCornerRadiiToTracedValue(
          "mask_filter_rounded_corners_radii",
          mask_filter_info.rounded_corner_bounds(), value);
      value->SetBoolean("mask_filter_is_fast_rounded_corner",
                        is_fast_rounded_corner);
    }
    if (mask_filter_info.HasGradientMask()) {
      MathUtil::AddToTracedValue("mask_filter_gradient_mask",
                                 mask_filter_info.gradient_mask().value(),
                                 value);
    }
  }
  value->SetString("blend_mode", SkBlendMode_Name(blend_mode));
  value->SetString("subtree_capture_id", subtree_capture_id.ToString());
  value->SetString("subtree_size", subtree_size.ToString());
  value->SetBoolean("cache_render_surface", cache_render_surface);
  value->SetBoolean("has_copy_request", has_copy_request);
  value->SetBoolean("hidden_by_backface_visibility",
                    hidden_by_backface_visibility);
  value->SetBoolean("double_sided", double_sided);
  value->SetBoolean("trilinear_filtering", trilinear_filtering);
  value->SetBoolean("is_drawn", is_drawn);
  value->SetBoolean("only_draws_visible_content", only_draws_visible_content);
  value->SetBoolean("subtree_hidden", subtree_hidden);
  value->SetBoolean("has_potential_filter_animation",
                    has_potential_filter_animation);
  value->SetBoolean("has_potential_backdrop_filter_animation",
                    has_potential_backdrop_filter_animation);
  value->SetBoolean("has_potential_opacity_animation",
                    has_potential_opacity_animation);
  value->SetBoolean("has_masking_child", has_masking_child);
  value->SetBoolean("effect_changed", effect_changed);
  value->SetBoolean("subtree_has_copy_request", subtree_has_copy_request);
  value->SetBoolean("affected_by_backdrop_filter", affected_by_backdrop_filter);
  value->SetString("render_surface_reason",
                   RenderSurfaceReasonToString(render_surface_reason));
  value->SetInteger("transform_id", transform_id);
  value->SetInteger("clip_id", clip_id);
  value->SetInteger("target_id", target_id);
  value->SetInteger("closest_ancestor_with_cached_render_surface_id",
                    closest_ancestor_with_cached_render_surface_id);
  value->SetInteger("closest_ancestor_with_copy_request_id",
                    closest_ancestor_with_copy_request_id);
  value->SetInteger("closest_ancestor_being_captured_id",
                    closest_ancestor_being_captured_id);
  if (view_transition_element_resource_id.IsValid()) {
    value->SetString("view_transition_element_resource_id",
                     view_transition_element_resource_id.ToString());
  }
}

}  // namespace cc
