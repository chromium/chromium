// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_EFFECT_NODE_H_
#define CC_TREES_EFFECT_NODE_H_

#include "cc/cc_export.h"
#include "cc/paint/element_id.h"
#include "cc/paint/filter_operations.h"
#include "cc/view_transition/view_transition_shared_element_id.h"
#include "components/viz/common/surfaces/subtree_capture_id.h"
#include "components/viz/common/view_transition_element_resource_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

  enum StableIdLabels { INVALID_STABLE_ID = 0 };

  // The node index of this node in the effect tree node vector.
  int id;
  // The node index of the parent node in the effect tree node vector.
  int parent_id;
  // An opaque, unique, stable identifer for this effect that persists across
  // frame commits. This id is used only for internal implementation
  // details such as RenderSurface and RenderPass ids, and should not
  // be assumed to have semantic meaning.
  uint64_t stable_id;

  float opacity;
  float screen_space_opacity;

  FilterOperations filters;
  FilterOperations backdrop_filters;
  absl::optional<gfx::RRectF> backdrop_filter_bounds;
  float backdrop_filter_quality;
  gfx::PointF filters_origin;

  // The element id corresponding to the mask to apply to the filtered backdrop
  // image.
  ElementId backdrop_mask_element_id;

  // The mask filter information applied to this effect node. The coordinates of
  // in the mask info is in the space of the transform node associated with this
  // effect node.
  gfx::MaskFilterInfo mask_filter_info;

  SkBlendMode blend_mode;

  gfx::Vector2dF surface_contents_scale;

  viz::SubtreeCaptureId subtree_capture_id;
  gfx::Size subtree_size;

  bool cache_render_surface : 1;
  bool has_copy_request : 1;
  bool hidden_by_backface_visibility : 1;
  // Whether the contents should continue to be visible when rotated such that
  // its back face is facing toward the camera. It's true by default.
  bool double_sided : 1;
  bool trilinear_filtering : 1;
  bool is_drawn : 1;
  // In most cases we only need to draw the visible part of any content
  // contributing to the effect. For copy request case, we would need to copy
  // the entire content, and could not only draw the visible part. In the rare
  // case of a backdrop zoom filter we need to take into consideration the
  // content offscreen to make sure the backdrop zoom filter is applied with the
  // correct center.
  bool only_draws_visible_content : 1;
  // TODO(jaydasika) : Delete this after implementation of
  // SetHideLayerAndSubtree is cleaned up. (crbug.com/595843)
  bool subtree_hidden : 1;
  // Whether this node has a potentially running (i.e., irrespective
  // of exact timeline) filter animation.
  bool has_potential_filter_animation : 1;
  // Whether this node has a potentially running (i.e., irrespective
  // of exact timeline) backdrop-filter animation.
  bool has_potential_backdrop_filter_animation : 1;
  // Whether this node has a potentially running (i.e., irrespective
  // of exact timeline) opacity animation.
  bool has_potential_opacity_animation : 1;
  // Whether this node has a currently running filter animation.
  bool is_currently_animating_filter : 1;
  // Whether this node has a currently running backdrop-filter animation.
  bool is_currently_animating_backdrop_filter : 1;
  // Whether this node has a currently running opacity animation.
  bool is_currently_animating_opacity : 1;
  // Whether this node has a child node with kDstIn blend mode.
  bool has_masking_child : 1;
  // Whether this node's effect has been changed since the last
  // frame. Needed in order to compute damage rect.
  bool effect_changed : 1;
  bool subtree_has_copy_request : 1;
  // If set, the effect node tries to not trigger a render surface due to it
  // having a rounded corner.
  bool is_fast_rounded_corner : 1;
  // If the node or it's parent has the filters, it sets to true.
  bool node_or_ancestor_has_filters : 1;
  // All node in the subtree starting from the containing render surface, and
  // before the backdrop filter node in pre tree order.
  // This is set and used for the impl-side effect tree only.
  bool affected_by_backdrop_filter: 1;
  // RenderSurfaceReason::kNone if this effect node should not create a render
  // surface, or the reason that this effect node should create one.
  RenderSurfaceReason render_surface_reason;
  // The transform node index of the transform to apply to this effect
  // node's content when rendering to a surface.
  int transform_id;
  // The clip node index of the clip to apply to this effect node's
  // content when rendering to a surface.
  int clip_id;

  // This is the id of the ancestor effect node that induces a
  // RenderSurfaceImpl.
  // This is set and used for the impl-side effect tree only.
  int target_id;
  int closest_ancestor_with_cached_render_surface_id;
  int closest_ancestor_with_copy_request_id;
  int closest_ancestor_being_captured_id;
  int closest_ancestor_with_shared_element_id;

  // Represents a DOM element id for the view transition API.
  ViewTransitionElementId view_transition_shared_element_id;

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
