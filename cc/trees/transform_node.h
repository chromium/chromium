// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_TRANSFORM_NODE_H_
#define CC_TREES_TRANSFORM_NODE_H_

#include "cc/cc_export.h"
#include "cc/trees/element_id.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/transform.h"

namespace base {
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace cc {

struct CC_EXPORT TransformNode {
  TransformNode();
  TransformNode(const TransformNode&);

  // The node index of this node in the transform tree node vector.
  int id;
  // The node index of the parent node in the transform tree node vector.
  int parent_id;

  ElementId element_id;

  // The local transform information is combined to form to_parent (ignoring
  // snapping) as follows:
  //
  //   to_parent = M_post_local * T_scroll * M_local * M_pre_local.
  //
  // The pre/post may seem odd when read LTR, but we multiply our points from
  // the right, so the pre_local matrix affects the result "first". This lines
  // up with the notions of pre/post used in skia and gfx::Transform.
  //
  // TODO(vollick): The values labeled with "will be moved..." take up a lot of
  // space, but are only necessary for animated or scrolled nodes (otherwise
  // we'll just use the baked to_parent). These values will be ultimately stored
  // directly on the transform/scroll display list items when that's possible,
  // or potentially in a scroll tree.
  //
  // TODO(vollick): will be moved when accelerated effects are implemented.
  gfx::Transform pre_local;
  gfx::Transform local;
  gfx::Transform post_local;

  gfx::Transform to_parent;

  // This is the node which defines the sticky position constraints for this
  // transform node. -1 indicates there are no sticky position constraints.
  int sticky_position_constraint_id;

  // This is the node with respect to which source_offset is defined. This will
  // not be needed once layerization moves to cc, but is needed in order to
  // efficiently update the transform tree for changes to position in the layer
  // tree.
  int source_node_id;

  // This id determines which 3d rendering context the node is in. 0 is a
  // special value and indicates that the node is not in any 3d rendering
  // context.
  int sorting_context_id;

  // TODO(vollick): will be moved when accelerated effects are implemented.
  bool needs_local_transform_update : 1;

  // Whether this node or any ancestor has a potentially running
  // (i.e., irrespective of exact timeline) transform animation or an
  // invertible transform.
  bool node_and_ancestors_are_animated_or_invertible : 1;

  bool is_invertible : 1;
  // Whether the transform from this node to the screen is
  // invertible.
  bool ancestors_are_invertible : 1;

  // Whether this node has a potentially running (i.e., irrespective
  // of exact timeline) transform animation.
  bool has_potential_animation : 1;
  // Whether this node has a currently running transform animation.
  bool is_currently_animating : 1;
  // Whether this node *or an ancestor* has a potentially running
  // (i.e., irrespective of exact timeline) transform
  // animation.
  bool to_screen_is_potentially_animated : 1;
  // Whether all animations on this transform node are simple
  // translations.
  bool has_only_translation_animations : 1;

  // Flattening, when needed, is only applied to a node's inherited transform,
  // never to its local transform.
  bool flattens_inherited_transform : 1;

  // This is true if the to_parent transform at every node on the path to the
  // root is flat.
  bool node_and_ancestors_are_flat : 1;

  // This is needed to know if a layer can use lcd text.
  bool node_and_ancestors_have_only_integer_translation : 1;

  bool scrolls : 1;

  bool should_be_snapped : 1;

  // These are used to position nodes wrt the right or bottom of the inner or
  // outer viewport.
  bool moved_by_inner_viewport_bounds_delta_x : 1;
  bool moved_by_inner_viewport_bounds_delta_y : 1;
  bool moved_by_outer_viewport_bounds_delta_x : 1;
  bool moved_by_outer_viewport_bounds_delta_y : 1;

  // Layer scale factor is used as a fallback when we either cannot adjust
  // raster scale or if the raster scale cannot be extracted from the screen
  // space transform. For layers in the subtree of the page scale layer, the
  // layer scale factor should include the page scale factor.
  bool in_subtree_of_page_scale_layer : 1;

  // We need to track changes to to_screen transform to compute the damage rect.
  bool transform_changed : 1;

  // TODO(vollick): will be moved when accelerated effects are implemented.
  float post_local_scale_factor;

  // TODO(vollick): will be moved when accelerated effects are implemented.
  gfx::ScrollOffset scroll_offset;

  // This value stores the snapped amount whenever we snap. If the snap is due
  // to a scroll, we need it to calculate fixed-pos elements adjustment, even
  // otherwise we may need it to undo the snapping next frame.
  gfx::Vector2dF snap_amount;

  // TODO(vollick): will be moved when accelerated effects are implemented.
  gfx::Vector2dF source_offset;
  gfx::Vector2dF source_to_parent;

  bool operator==(const TransformNode& other) const;

  void set_to_parent(const gfx::Transform& transform) {
    to_parent = transform;
    is_invertible = to_parent.IsInvertible();
  }

  void update_pre_local_transform(const gfx::Point3F& transform_origin);

  void update_post_local_transform(const gfx::PointF& position,
                                   const gfx::Point3F& transform_origin);

  void AsValueInto(base::trace_event::TracedValue* value) const;
};

// TODO(sunxd): move this into PropertyTrees::cached_data_.
struct CC_EXPORT TransformCachedNodeData {
  TransformCachedNodeData();
  TransformCachedNodeData(const TransformCachedNodeData& other);
  ~TransformCachedNodeData();

  gfx::Transform from_screen;
  gfx::Transform to_screen;

  bool is_showing_backface : 1;

  bool operator==(const TransformCachedNodeData& other) const;
};

}  // namespace cc

#endif  // CC_TREES_TRANSFORM_NODE_H_
