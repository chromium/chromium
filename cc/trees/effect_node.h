// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_EFFECT_NODE_H_
#define CC_TREES_EFFECT_NODE_H_

#include "cc/cc_export.h"
#include "cc/paint/element_id.h"
#include "cc/paint/filter_operations.h"
#include "third_party/skia/include/core/SkBlendMode.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/rrect_f.h"

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
  base::Optional<gfx::RRectF> backdrop_filter_bounds;
  float backdrop_filter_quality;
  gfx::PointF filters_origin;

  // The element id corresponding to the mask to apply to the filtered backdrop
  // image. Note that this is separate from mask_layer_id, which is a layer id,
  // and is used for masking the "normal" (non-backdrop-filter) content.
  ElementId backdrop_mask_element_id;

  // Bounds of rounded corner rrect in the space of the transform node
  // associated with this effect node.
  gfx::RRectF rounded_corner_bounds;

  SkBlendMode blend_mode;

  gfx::Vector2dF surface_contents_scale;

  bool cache_render_surface : 1;
  bool has_copy_request : 1;
  bool hidden_by_backface_visibility : 1;
  // Whether the contents should continue to be visible when rotated such that
  // its back face is facing toward the camera. It's true by default.
  bool double_sided : 1;
  bool trilinear_filtering : 1;
  bool is_drawn : 1;
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
  int target_id;
  int closest_ancestor_with_cached_render_surface_id;
  int closest_ancestor_with_copy_request_id;

  bool HasRenderSurface() const {
    return render_surface_reason != RenderSurfaceReason::kNone;
  }

  bool operator==(const EffectNode& other) const;

  void AsValueInto(base::trace_event::TracedValue* value) const;
};

}  // namespace cc

#endif  // CC_TREES_EFFECT_NODE_H_
