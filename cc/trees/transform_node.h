// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_TRANSFORM_NODE_H_
#define CC_TREES_TRANSFORM_NODE_H_

#include "cc/cc_export.h"
#include "cc/paint/element_id.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/property_ids.h"
#include "ui/gfx/geometry/point3_f.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace base {
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace cc {

struct CC_EXPORT TransformNode {
  TransformNode();
  TransformNode(const TransformNode&);
  TransformNode& operator=(const TransformNode&);

  // The node index of this node in the transform tree node vector.
  int id = kInvalidPropertyNodeId;
  // The node index of the parent node in the transform tree node vector.
  int parent_id = kInvalidPropertyNodeId;
  // The node index of the nearest parent frame node in the transform tree node
  // vector.
  int parent_frame_id = kInvalidPropertyNodeId;

  ElementId element_id;

  // The local transform information is combined to form to_parent (ignoring
  // snapping) as follows:
  //   to_parent =
  //       T_post_translation * T_origin * T_scroll * M_local * -T_origin.
  gfx::Transform local;
  gfx::Point3F origin;
  // For layer tree mode only. In layer list mode, when the translation is
  // needed, blink creates paint offset translation node above this node.
  gfx::Vector2dF post_translation;

  gfx::Transform to_parent;

  // This is the node which defines the sticky position constraints for this
  // transform node.
  int sticky_position_constraint_id = kInvalidPropertyNodeId;

  // This is the data of the scroll adjustment containers of the default anchor
  // of an anchor positioned element. -1 indicates there is no such node.
  int anchor_position_scroll_data_id = kInvalidPropertyNodeId;

  // This id determines which 3d rendering context the node is in. 0 is a
  // special value and indicates that the node is not in any 3d rendering
  // context.
  int sorting_context_id = 0;

  // True if |TransformTree::UpdateLocalTransform| needs to be called which
  // will update |to_parent|.
  bool needs_local_transform_update : 1 = true;

  // Whether this node or any ancestor has a potentially running
  // (i.e., irrespective of exact timeline) transform animation or an
  // invertible transform.
  bool node_and_ancestors_are_animated_or_invertible : 1 = true;

  bool is_invertible : 1 = true;
  // Whether the transform from this node to the screen is
  // invertible.
  bool ancestors_are_invertible : 1 = true;

  // Whether this node has a potentially running (i.e., irrespective
  // of exact timeline) transform animation.
  bool has_potential_animation : 1 = false;
  // Whether this node has a currently running transform animation.
  bool is_currently_animating : 1 = false;
  // Whether this node *or an ancestor* has a potentially running
  // (i.e., irrespective of exact timeline) transform
  // animation.
  bool to_screen_is_potentially_animated : 1 = false;

  // Flattening, when needed, is only applied to a node's inherited transform,
  // never to its local transform. It's true by default.
  bool flattens_inherited_transform : 1 = true;

  // This is true if the to_parent transform at every node on the path to the
  // root is flat.
  bool node_and_ancestors_are_flat : 1 = true;

  bool scrolls : 1 = false;

  bool should_undo_overscroll : 1 = false;

  bool should_be_snapped : 1 = false;

  // Used by the compositor to determine which layers need to be repositioned by
  // the compositor as a result of browser controls expanding/contracting the
  // outer viewport size before Blink repositions the fixed layers.
  bool moved_by_outer_viewport_bounds_delta_y : 1 = false;

  // Layer scale factor is used as a fallback when we either cannot adjust
  // raster scale or if the raster scale cannot be extracted from the screen
  // space transform. For layers in the subtree of the page scale layer, the
  // layer scale factor should include the page scale factor.
  bool in_subtree_of_page_scale_layer : 1 = false;

  // We need to track changes to to_screen transform to compute the damage rect.
  bool transform_changed : 1 = false;

  // Whether the parent transform node should be used for checking backface
  // visibility, not this transform one.
  bool delegates_to_parent_for_backface : 1 = false;

  // Set to true, if the compositing reason is will-change:transform, scale,
  // rotate, or translate (for the CSS property that created this node).
  bool will_change_transform : 1 = false;

  // Set to true, if the node or it's parent |will_change_transform| is true.
  bool node_or_ancestors_will_change_transform : 1 = false;

  gfx::PointF scroll_offset;

  // This value stores the snapped amount whenever we snap. If the snap is due
  // to a scroll, we need it to calculate fixed-pos elements adjustment, even
  // otherwise we may need it to undo the snapping next frame.
  gfx::Vector2dF snap_amount;

  // From MutatorHost::GetMaximuimAnimationScale(). Updated by
  // PropertyTrees::MaximumAnimationScaleChanged() and
  // LayerTreeImpl::UpdateTransformAnimation().
  float maximum_animation_scale = kInvalidScale;

  // Set to the element ID of containing document if this transform node is the
  // root of a visible frame subtree.
  ElementId visible_frame_element_id;

#if DCHECK_IS_ON()
  bool operator==(const TransformNode& other) const;
#endif

  void set_to_parent(const gfx::Transform& transform) {
    to_parent = transform;
    is_invertible = to_parent.IsInvertible();
  }

  void AsValueInto(base::trace_event::TracedValue* value) const;
};

// TODO(sunxd): move this into PropertyTrees::cached_data_.
struct CC_EXPORT TransformCachedNodeData {
  TransformCachedNodeData();
  TransformCachedNodeData(const TransformCachedNodeData& other);
  ~TransformCachedNodeData();

  gfx::Transform from_screen;
  gfx::Transform to_screen;

  bool is_showing_backface = false;

  bool operator==(const TransformCachedNodeData& other) const;
};

}  // namespace cc

#endif  // CC_TREES_TRANSFORM_NODE_H_
