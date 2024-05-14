// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_EFFECT_NODE_H_
#define CC_TREES_EFFECT_NODE_H_

#include <optional>

#include "cc/cc_export.h"
#include "cc/paint/element_id.h"
#include "cc/paint/filter_operations.h"
#include "cc/trees/property_ids.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "components/viz/common/view_transition_element_resource_id.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "ui/gfx/geometry/mask_filter_info.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_f.h"

namespace base {
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace cc {

enum class RenderSurfaceReason : uint8_t {
  kNone,
  kRoot,
  k3dTransformFlattening,
  // Defines the scope of the backdrop for child blend mode or backdrop filter.
  kBackdropScope,
  kBlendMode,
  kBlendModeDstIn,
  kOpacity,
  kOpacityAnimation,
  kFilter,
  kFilterAnimation,
  kBackdropFilter,
  kBackdropFilterAnimation,
  kRoundedCorner,
  kClipPath,
  kClipAxisAlignment,
  kMask,
  kTrilinearFiltering,
  kCache,
  kCopyRequest,
  kMirrored,
  kSubtreeIsBeingCaptured,
  kViewTransitionParticipant,
  kGradientMask,
  // This must be the last value because it's used in tracing code to know the
  // number of reasons.
  kTest,
};

CC_EXPORT const char* RenderSurfaceReasonToString(RenderSurfaceReason);

struct CC_EXPORT EffectNode {
  EffectNode();
  EffectNode(const EffectNode& other);
  ~EffectNode();

  // The node index of this node in the effect tree node vector.
  int id = kInvalidPropertyNodeId;
  // The node index of the parent node in the effect tree node vector.
  int parent_id = kInvalidPropertyNodeId;

  // An opaque, unique, stable identifier for this effect that persists across
  // frame commits. This id is used only for internal implementation
  // details such as RenderSurface and RenderPass ids, and should not
  // be assumed to have semantic meaning.
  ElementId element_id;

  float opacity = 1.f;
  float screen_space_opacity = 1.f;

  FilterOperations filters;
  FilterOperations backdrop_filters;
  std::optional<gfx::RRectF> backdrop_filter_bounds;
  float backdrop_filter_quality = 1.f;
  gfx::PointF filters_origin;

  // The element id corresponding to the mask to apply to the filtered backdrop
  // image.
  ElementId backdrop_mask_element_id;

  // The mask filter information applied to this effect node. The coordinates of
  // in the mask info is in the space of the transform node associated with this
  // effect node.
  gfx::MaskFilterInfo mask_filter_info;

  SkBlendMode blend_mode = SkBlendMode::kSrcOver;

  gfx::Vector2dF surface_contents_scale;

  viz::SubtreeCaptureId subtree_capture_id;
  gfx::Size subtree_size;

  bool cache_render_surface : 1 = false;
  bool has_copy_request : 1 = false;
  bool hidden_by_backface_visibility : 1 = false;
  // Whether the contents should continue to be visible when rotated such that
  // its back face is facing toward the camera. It's true by default.
  bool double_sided : 1 = true;
  bool trilinear_filtering : 1 = false;
  bool is_drawn : 1 = true;
  // In most cases we only need to draw the visible part of any content
  // contributing to the effect. For copy request case, we would need to copy
  // the entire content, and could not only draw the visible part. In the rare
  // case of a backdrop zoom filter we need to take into consideration the
  // content offscreen to make sure the backdrop zoom filter is applied with the
  // correct center.
  bool only_draws_visible_content : 1 = true;
  // TODO(jaydasika) : Delete this after implementation of
  // SetHideLayerAndSubtree is cleaned up. (crbug.com/595843)
  bool subtree_hidden : 1 = false;
  // Whether this node has a potentially running (i.e., irrespective
  // of exact timeline) filter animation.
  bool has_potential_filter_animation : 1 = false;
  // Whether this node has a potentially running (i.e., irrespective
  // of exact timeline) backdrop-filter animation.
  bool has_potential_backdrop_filter_animation : 1 = false;
  // Whether this node has a potentially running (i.e., irrespective
  // of exact timeline) opacity animation.
  bool has_potential_opacity_animation : 1 = false;
  // Whether this node has a currently running filter animation.
  bool is_currently_animating_filter : 1 = false;
  // Whether this node has a currently running backdrop-filter animation.
  bool is_currently_animating_backdrop_filter : 1 = false;
  // Whether this node has a currently running opacity animation.
  bool is_currently_animating_opacity : 1 = false;
  // Whether this node has a child node with kDstIn blend mode.
  bool has_masking_child : 1 = false;
  // Whether this node's effect has been changed since the last
  // frame. Needed in order to compute damage rect.
  bool effect_changed : 1 = false;
  bool subtree_has_copy_request : 1 = false;
  // If set, the effect node tries to not trigger a render surface due to it
  // having a rounded corner.
  bool is_fast_rounded_corner : 1 = false;
  bool node_or_ancestor_has_fast_rounded_corner : 1 = false;
  // If the node or it's parent has the filters, it sets to true.
  bool node_or_ancestor_has_filters : 1 = false;
  // All node in the subtree starting from the containing render surface, and
  // before the backdrop filter node in pre tree order.
  // This is set and used for the impl-side effect tree only.
  bool affected_by_backdrop_filter : 1 = false;
  // RenderSurfaceReason::kNone if this effect node should not create a render
  // surface, or the reason that this effect node should create one.
  RenderSurfaceReason render_surface_reason = RenderSurfaceReason::kNone;
  // The transform node index of the transform to apply to this effect
  // node's content when rendering to a surface.
  int transform_id = kRootPropertyNodeId;
  // The clip node index of the clip to apply to this effect node's
  // content when rendering to a surface.
  int clip_id = kRootPropertyNodeId;

  // This is the id of the ancestor effect node that induces a
  // RenderSurfaceImpl.
  // This is set and used for the impl-side effect tree only.
  int target_id = 1;
  // If this node is tagged with a ViewTransitionElementResourceId, it means it
  // produces a snapshot for an element participating in a transition. This
  // target id corresponds to the effect node where the
  // ViewTransitionContentLayer using this resource draws. Can be unset if no
  // layer using this resource is being drawn.
  int view_transition_target_id = kInvalidPropertyNodeId;
  int closest_ancestor_with_cached_render_surface_id = kInvalidPropertyNodeId;
  int closest_ancestor_with_copy_request_id = kInvalidPropertyNodeId;
  int closest_ancestor_being_captured_id = kInvalidPropertyNodeId;
  int closest_ancestor_with_shared_element_id = kInvalidPropertyNodeId;

  // Represents a resource id for a resource cached or generated in the Viz
  // process.
  viz::ViewTransitionElementResourceId view_transition_element_resource_id;

  bool HasRenderSurface() const {
    return render_surface_reason != RenderSurfaceReason::kNone;
  }

#if DCHECK_IS_ON()
  bool operator==(const EffectNode& other) const;
#endif

  void AsValueInto(base::trace_event::TracedValue* value) const;
};

}  // namespace cc

#endif  // CC_TREES_EFFECT_NODE_H_
