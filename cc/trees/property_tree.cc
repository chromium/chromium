// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/traced_value.h"
#include "cc/base/features.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/compositor_commit_data.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "cc/trees/viewport_property_ids.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "ui/gfx/geometry/outsets_f.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

void AnimationUpdateOnMissingPropertyNodeUMALog(bool missing_property_node) {
  UMA_HISTOGRAM_BOOLEAN(
      "Compositing.Renderer.AnimationUpdateOnMissingPropertyNode",
      missing_property_node);
}

AnchorPositionScrollData::AnchorPositionScrollData() = default;
AnchorPositionScrollData::~AnchorPositionScrollData() = default;
AnchorPositionScrollData::AnchorPositionScrollData(
    const AnchorPositionScrollData&) = default;

bool AnchorPositionScrollData::operator==(
    const AnchorPositionScrollData& other) const = default;

bool StickyPositionNodeData::operator==(
    const StickyPositionNodeData& other) const = default;

template <typename T>
PropertyTree<T>::PropertyTree(PropertyTrees* property_trees)
    : needs_update_(false), property_trees_(property_trees) {
  nodes_.push_back(T());
  back()->id = kRootPropertyNodeId;
  back()->parent_id = kInvalidPropertyNodeId;
}

// Equivalent to
// PropertyTree<T>::~PropertyTree() = default;
// but due to a gcc bug the generated destructor will have wrong symbol
// visibility in component build.
template <typename T>
PropertyTree<T>::~PropertyTree() = default;

template <typename T>
PropertyTree<T>& PropertyTree<T>::operator=(const PropertyTree<T>&) = default;

#define DCHECK_NODE_EXISTENCE(check_node_existence, state, property,           \
                              needs_rebuild)                                   \
  DCHECK(!check_node_existence || ((!state.currently_running[property] &&      \
                                    !state.potentially_animating[property]) || \
                                   needs_rebuild))

TransformTree::TransformTree(PropertyTrees* property_trees)
    : PropertyTree<TransformNode>(property_trees),
      page_scale_factor_(1.f),
      device_scale_factor_(1.f),
      device_transform_scale_factor_(1.f) {
  cached_data_.push_back(TransformCachedNodeData());
}

TransformTree::~TransformTree() = default;
TransformTree& TransformTree::operator=(const TransformTree&) = default;

template <typename T>
int PropertyTree<T>::Insert(const T& tree_node, int parent_id) {
  DCHECK_GT(nodes_.size(), 0u);
  nodes_.push_back(tree_node);
  T& node = nodes_.back();
  node.parent_id = parent_id;
  node.id = static_cast<int>(nodes_.size()) - 1;
  return node.id;
}

template <typename T>
void PropertyTree<T>::RemoveNodes(size_t n) {
  CHECK_LE(n, nodes_.size());
  nodes_.resize(nodes_.size() - n);

  const int upper_bound = base::checked_cast<int>(nodes_.size());
  base::EraseIf(element_id_to_node_index_, [upper_bound](const auto& entry) {
    return entry.second >= upper_bound;
  });
}

template <typename T>
void PropertyTree<T>::clear() {
  needs_update_ = false;
  nodes_.clear();
  nodes_.push_back(T());
  back()->id = kRootPropertyNodeId;
  back()->parent_id = kInvalidPropertyNodeId;
  element_id_to_node_index_.clear();

#if DCHECK_IS_ON()
  PropertyTree<T> tree(nullptr);
  DCHECK(tree == *this);
#endif
}

#if DCHECK_IS_ON()
template <typename T>
bool PropertyTree<T>::operator==(const PropertyTree<T>& other) const {
  return nodes() == other.nodes() && needs_update() == other.needs_update() &&
         element_id_to_node_index() == other.element_id_to_node_index();
}
#endif

template <typename T>
void PropertyTree<T>::AsValueInto(base::trace_event::TracedValue* value) const {
  value->BeginArray("nodes");
  for (const auto& node : nodes_) {
    value->BeginDictionary();
    node.AsValueInto(value);
    value->EndDictionary();
  }
  value->EndArray();
}

template class PropertyTree<TransformNode>;
template class PropertyTree<ClipNode>;
template class PropertyTree<EffectNode>;
template class PropertyTree<ScrollNode>;

int TransformTree::Insert(const TransformNode& tree_node, int parent_id) {
  int node_id = PropertyTree<TransformNode>::Insert(tree_node, parent_id);
  DCHECK_EQ(node_id, static_cast<int>(cached_data_.size()));

  cached_data_.push_back(TransformCachedNodeData());
  return node_id;
}

void TransformTree::RemoveNodes(size_t n) {
  PropertyTree<TransformNode>::RemoveNodes(n);
  cached_data_.resize(cached_data_.size() - n);
}

void TransformTree::clear() {
  PropertyTree<TransformNode>::clear();

  page_scale_factor_ = 1.f;
  device_scale_factor_ = 1.f;
  device_transform_scale_factor_ = 1.f;
  nodes_affected_by_outer_viewport_bounds_delta_.clear();
  cached_data_.clear();
  cached_data_.push_back(TransformCachedNodeData());
  sticky_position_data_.clear();
  anchor_position_scroll_data_.clear();

#if DCHECK_IS_ON()
  DCHECK(TransformTree() == *this);
#endif
}

void TransformTree::set_needs_update(bool needs_update) {
  if (needs_update && !PropertyTree<TransformNode>::needs_update())
    property_trees()->UpdateTransformTreeUpdateNumber();
  PropertyTree<TransformNode>::set_needs_update(needs_update);
}

bool TransformTree::OnTransformAnimated(ElementId element_id,
                                        const gfx::Transform& transform) {
  TransformNode* node = FindNodeFromElementId(element_id);
  // TODO(crbug.com/40828469): Remove this when we no longer animate
  // non-existent nodes.
  if (!node) {
    AnimationUpdateOnMissingPropertyNodeUMALog(true);
    return false;
  }
  AnimationUpdateOnMissingPropertyNodeUMALog(false);
  if (node->local == transform)
    return false;
  node->local = transform;
  node->needs_local_transform_update = true;
  node->transform_changed = true;
  property_trees()->set_changed(true);
  set_needs_update(true);
  return true;
}

void TransformTree::ResetChangeTracking() {
  for (int id = kContentsRootPropertyNodeId; id < static_cast<int>(size());
       ++id) {
    TransformNode* node = Node(id);
    node->transform_changed = false;
  }
}

void TransformTree::UpdateAllTransforms(
    const ViewportPropertyIds& viewport_property_ids) {
  if (!needs_update()) {
#if DCHECK_IS_ON()
    // If the transform tree does not need an update, no TransformNode should
    // need a local transform update.
    for (int i = kContentsRootPropertyNodeId; i < static_cast<int>(size());
         ++i) {
      DCHECK(!Node(i)->needs_local_transform_update);
    }
#endif
    return;
  }

  UpdateTransformsData update_data;
  do {
    size_t last_num_stale_forward_dependencies =
        update_data.stale_forward_dependencies.size();
    for (int i = kContentsRootPropertyNodeId; i < static_cast<int>(size());
         ++i) {
      UpdateTransforms(i, &viewport_property_ids, &update_data);
    }
    CHECK(last_num_stale_forward_dependencies == 0 ||
          update_data.stale_forward_dependencies.size() <
              last_num_stale_forward_dependencies);
  } while (!update_data.stale_forward_dependencies.empty());

  set_needs_update(false);
}

TransformTree::UpdateTransformsData::UpdateTransformsData() = default;
TransformTree::UpdateTransformsData::~UpdateTransformsData() = default;

void TransformTree::UpdateTransforms(
    int id,
    const ViewportPropertyIds* viewport_property_ids,
    UpdateTransformsData* update_data) {
  TransformNode* node = Node(id);
  TransformNode* parent_node = parent(node);
  DCHECK(parent_node);
  gfx::Transform old_to_parent = node->to_parent;
  gfx::Vector2dF old_snap_amount = node->snap_amount;
  // TODO(flackr): Only dirty when scroll offset changes.
  if (node->sticky_position_constraint_id >= 0 ||
      node->anchor_position_scroll_data_id >= 0 ||
      node->needs_local_transform_update || node->should_undo_overscroll) {
    UpdateLocalTransform(node, viewport_property_ids, update_data);
  } else {
    UndoSnapping(node);
  }
  UpdateScreenSpaceTransform(node, parent_node);
  UpdateAnimationProperties(node, parent_node);
  UpdateSnapping(node);
  UpdateTransformChanged(node, parent_node);
  UpdateNodeAndAncestorsAreAnimatedOrInvertible(node, parent_node);
  UpdateNodeOrAncestorsWillChangeTransform(node, parent_node);

  // If `node` has been depended by a previous node and neither its `to_parent`
  // nor its `snap_amount` is changed, the depending node actually got correct
  // data, so remove `id` from `stale_forward_dependencies`. Note that we
  // should check all changes that may affect the depending transform node.
  // For now forward dependency only happens in AnchorPositionOffset().
  if (update_data && node->to_parent == old_to_parent &&
      node->snap_amount == old_snap_amount) {
    update_data->stale_forward_dependencies.erase(id);
  }

  DCHECK(!node->needs_local_transform_update);
}

bool TransformTree::IsDescendant(int desc_id, int source_id) const {
  while (desc_id != source_id) {
    if (desc_id == kInvalidPropertyNodeId)
      return false;
    desc_id = Node(desc_id)->parent_id;
  }
  return true;
}

void TransformTree::CombineTransformsBetween(int source_id,
                                             int dest_id,
                                             gfx::Transform* transform) const {
  DCHECK(source_id > dest_id);
  const TransformNode* current = Node(source_id);
  const TransformNode* dest = Node(dest_id);
  // Combine transforms to and from the screen when possible. Since flattening
  // is a non-linear operation, we cannot use this approach when there is
  // non-trivial flattening between the source and destination nodes. For
  // example, consider the tree R->A->B->C, where B flattens its inherited
  // transform, and A has a non-flat transform. Suppose C is the source and A is
  // the destination. The expected result is C * B. But C's to_screen
  // transform is C * B * flattened(A * R), and A's from_screen transform is
  // R^{-1} * A^{-1}. If at least one of A and R isn't flat, the inverse of
  // flattened(A * R) won't be R^{-1} * A{-1}, so multiplying C's to_screen and
  // A's from_screen will not produce the correct result.
  if (!dest ||
      (dest->ancestors_are_invertible && dest->node_and_ancestors_are_flat)) {
    transform->PostConcat(ToScreen(current->id));
    if (dest)
      transform->PostConcat(FromScreen(dest->id));
    return;
  }

  // Flattening is defined in a way that requires it to be applied while
  // traversing downward in the tree. We first identify nodes that are on the
  // path from the source to the destination (this is traversing upward), and
  // then we visit these nodes in reverse order, flattening as needed. We
  // early-out if we get to a node whose target node is the destination, since
  // we can then re-use the target space transform stored at that node. However,
  // we cannot re-use a stored target space transform if the destination has a
  // zero surface contents scale, since stored target space transforms have
  // surface contents scale baked in, but we need to compute an unscaled
  // transform.
  std::vector<int> source_to_destination;
  source_to_destination.push_back(current->id);
  current = parent(current);
  for (; current && current->id > dest_id; current = parent(current))
    source_to_destination.push_back(current->id);

  gfx::Transform combined_transform;
  if (current->id < dest_id) {
    // We have reached the lowest common ancestor of the source and destination
    // nodes. This case can occur when we are transforming between a node
    // corresponding to a fixed-position layer (or its descendant) and the node
    // corresponding to the layer's render target. For example, consider the
    // layer tree R->T->S->F where F is fixed-position, S owns a render surface,
    // and T has a significant transform. This will yield the following
    // transform tree:
    //    R
    //    |
    //    T
    //   /|
    //  S F
    // In this example, T will have id 2, S will have id 3, and F will have id
    // 4. When walking up the ancestor chain from F, the first node with a
    // smaller id than S will be T, the lowest common ancestor of these nodes.
    // We compute the transform from T to S here, and then from F to T in the
    // loop below.
    DCHECK(IsDescendant(dest_id, current->id));
    CombineInversesBetween(current->id, dest_id, &combined_transform);
  }

  size_t source_to_destination_size = source_to_destination.size();
  for (size_t i = 0; i < source_to_destination_size; ++i) {
    size_t index = source_to_destination_size - 1 - i;
    const TransformNode* node = Node(source_to_destination[index]);
    if (node->flattens_inherited_transform)
      combined_transform.Flatten();
    combined_transform.PreConcat(node->to_parent);
  }

  transform->PostConcat(combined_transform);
}

bool TransformTree::CombineInversesBetween(int source_id,
                                           int dest_id,
                                           gfx::Transform* transform) const {
  DCHECK(source_id < dest_id);
  const TransformNode* current = Node(dest_id);
  const TransformNode* dest = Node(source_id);
  // Just as in CombineTransformsBetween, we can use screen space transforms in
  // this computation only when there isn't any non-trivial flattening
  // involved.
  if (current->ancestors_are_invertible &&
      current->node_and_ancestors_are_flat) {
    transform->PreConcat(FromScreen(current->id));
    if (dest)
      transform->PreConcat(ToScreen(dest->id));
    return true;
  }

  // Inverting a flattening is not equivalent to flattening an inverse. This
  // means we cannot, for example, use the inverse of each node's to_parent
  // transform, flattening where needed. Instead, we must compute the transform
  // from the destination to the source, with flattening, and then invert the
  // result.
  gfx::Transform dest_to_source;
  CombineTransformsBetween(dest_id, source_id, &dest_to_source);
  gfx::Transform source_to_dest;
  bool all_are_invertible = dest_to_source.GetInverse(&source_to_dest);
  transform->PreConcat(source_to_dest);
  return all_are_invertible;
}

// This function should match the offset we set for sticky position layer in
// blink::LayoutBoxModelObject::StickyPositionOffset.
gfx::Vector2dF TransformTree::StickyPositionOffset(TransformNode* node) {
  StickyPositionNodeData* sticky_data = MutableStickyPositionData(node->id);
  if (!sticky_data)
    return gfx::Vector2dF();
  const StickyPositionConstraint& constraint = sticky_data->constraints;
  const ScrollNode* scroll_node =
      property_trees()->scroll_tree().Node(sticky_data->scroll_ancestor);
  const TransformNode* transform_node = Node(scroll_node->transform_id);
  DCHECK(transform_node);
  // We need the scroll offset from the transform tree, not the scroll tree.
  // Tracking the scroll tree here would make sticky elements run "ahead" of a
  // main-repainted scroll.
  gfx::PointF scroll_position = transform_node->scroll_offset;
  if (transform_node->scrolls) {
    // The scroll position does not include snapping which shifts the scroll
    // offset to align to a pixel boundary, we need to manually include it here.
    // In this case, snapping is caused by a scroll.
    scroll_position -= transform_node->snap_amount;
  }

  gfx::RectF clip = constraint.constraint_box_rect;
  clip.Offset(scroll_position.x(), scroll_position.y());

  // The clip region may need to be offset by the outer viewport bounds, e.g. if
  // the top bar hides/shows. Position sticky should never attach to the inner
  // viewport since it shouldn't be affected by pinch-zoom.
  DCHECK(!scroll_node->scrolls_inner_viewport);
  if (scroll_node->scrolls_outer_viewport) {
    clip.set_width(
        clip.width() +
        property_trees()->outer_viewport_container_bounds_delta().x());
    clip.set_height(
        clip.height() +
        property_trees()->outer_viewport_container_bounds_delta().y());
  }

  gfx::Vector2dF ancestor_sticky_box_offset;
  if (sticky_data->nearest_node_shifting_sticky_box != kInvalidPropertyNodeId) {
    // TODO(crbug.com/40053373): Investigate why there would be an invalid index
    // passed in. Early return for now.
    if (sticky_data->nearest_node_shifting_sticky_box >=
        static_cast<int>(property_trees()->transform_tree().size()))
      return gfx::Vector2dF();
    const StickyPositionNodeData* ancestor_sticky_data =
        GetStickyPositionData(sticky_data->nearest_node_shifting_sticky_box);
    DCHECK(ancestor_sticky_data);
    ancestor_sticky_box_offset =
        ancestor_sticky_data->total_sticky_box_sticky_offset;
  }

  gfx::Vector2dF ancestor_containing_block_offset;
  if (sticky_data->nearest_node_shifting_containing_block !=
      kInvalidPropertyNodeId) {
    // TODO(crbug.com/40053373): Investigate why there would be an invalid index
    // passed in. Early return for now.
    if (sticky_data->nearest_node_shifting_containing_block >=
        static_cast<int>(property_trees()->transform_tree().size()))
      return gfx::Vector2dF();
    const StickyPositionNodeData* ancestor_sticky_data = GetStickyPositionData(
        sticky_data->nearest_node_shifting_containing_block);
    DCHECK(ancestor_sticky_data);
    ancestor_containing_block_offset =
        ancestor_sticky_data->total_containing_block_sticky_offset;
  }

  // Compute the current position of the constraint rects based on the original
  // positions and the offsets from ancestor sticky elements.
  gfx::RectF sticky_box_rect =
      gfx::RectF(constraint.scroll_container_relative_sticky_box_rect) +
      ancestor_sticky_box_offset + ancestor_containing_block_offset;
  gfx::RectF containing_block_rect =
      gfx::RectF(constraint.scroll_container_relative_containing_block_rect) +
      ancestor_containing_block_offset;

  gfx::Vector2dF sticky_offset;

  // In each of the following cases, we measure the limit which is the point
  // that the element should stick to, clamping on one side to 0 (because sticky
  // only pushes elements in one direction). Then we clamp to how far we can
  // push the element in that direction without being pushed outside of its
  // containing block.
  //
  // Note: The order of applying the sticky constraints is applied such that
  // left offset takes precedence over right offset, and top takes precedence
  // over bottom offset.
  if (constraint.is_anchored_right) {
    float right_limit = clip.right() - constraint.right_offset;
    float right_delta =
        std::min<float>(0, right_limit - sticky_box_rect.right());
    float available_space =
        std::min<float>(0, containing_block_rect.x() - sticky_box_rect.x());
    if (right_delta < available_space)
      right_delta = available_space;
    sticky_offset.set_x(sticky_offset.x() + right_delta);
  }
  if (constraint.is_anchored_left) {
    float left_limit = clip.x() + constraint.left_offset;
    float left_delta = std::max<float>(0, left_limit - sticky_box_rect.x());
    float available_space = std::max<float>(
        0, containing_block_rect.right() - sticky_box_rect.right());
    if (left_delta > available_space)
      left_delta = available_space;
    sticky_offset.set_x(sticky_offset.x() + left_delta);
  }
  if (constraint.is_anchored_bottom) {
    float bottom_limit = clip.bottom() - constraint.bottom_offset;
    float bottom_delta =
        std::min<float>(0, bottom_limit - sticky_box_rect.bottom());
    float available_space =
        std::min<float>(0, containing_block_rect.y() - sticky_box_rect.y());
    if (bottom_delta < available_space)
      bottom_delta = available_space;
    sticky_offset.set_y(sticky_offset.y() + bottom_delta);
  }
  if (constraint.is_anchored_top) {
    float top_limit = clip.y() + constraint.top_offset;
    float top_delta = std::max<float>(0, top_limit - sticky_box_rect.y());
    float available_space = std::max<float>(
        0, containing_block_rect.bottom() - sticky_box_rect.bottom());
    if (top_delta > available_space)
      top_delta = available_space;
    sticky_offset.set_y(sticky_offset.y() + top_delta);
  }

  sticky_data->total_sticky_box_sticky_offset =
      ancestor_sticky_box_offset + sticky_offset;
  sticky_data->total_containing_block_sticky_offset =
      ancestor_sticky_box_offset + ancestor_containing_block_offset +
      sticky_offset;

  // return
  return gfx::ToRoundedVector2d(sticky_offset);
}

AnchorPositionScrollData& TransformTree::EnsureAnchorPositionScrollData(
    int node_id) {
  TransformNode* node = Node(node_id);
  if (node->anchor_position_scroll_data_id == -1) {
    node->anchor_position_scroll_data_id = anchor_position_scroll_data_.size();
    anchor_position_scroll_data_.emplace_back();
  }
  return anchor_position_scroll_data_[node->anchor_position_scroll_data_id];
}

const AnchorPositionScrollData* TransformTree::GetAnchorPositionScrollData(
    int node_id) const {
  const TransformNode* node = Node(node_id);
  if (node->anchor_position_scroll_data_id == -1) {
    return nullptr;
  }
  return &anchor_position_scroll_data_[node->anchor_position_scroll_data_id];
}

gfx::Vector2dF TransformTree::AnchorPositionOffset(
    TransformNode* node,
    int max_updated_node_id,
    UpdateTransformsData* update_data) {
  const AnchorPositionScrollData* data = GetAnchorPositionScrollData(node->id);
  if (!data)
    return gfx::Vector2dF();

  // `update_data` can be null if UpdateTransforms() is called from
  // PropertyTreeBuilder (for layer tree mode for ui), but we should not have
  // anchor position in chrome ui.
  CHECK(update_data);

  gfx::Vector2dF accumulated_offset(0, 0);
  for (ElementId container_id : data->adjustment_container_ids) {
    int container_transform_id = kInvalidNodeId;
    if (const ScrollNode* scroll_node =
            property_trees()->scroll_tree().FindNodeFromElementId(
                container_id)) {
      container_transform_id = scroll_node->transform_id;
      const TransformNode* transform_node = Node(container_transform_id);
      // We don't ever expect that an anchor node or any of its scrolling
      // containers should have an invalid transform_id.
      DCHECK(container_transform_id != kInvalidPropertyNodeId);
      accumulated_offset += transform_node->scroll_offset.OffsetFromOrigin();
      // TODO(crbug.com/325613705): Should we consider snap_amount here?
    } else if (TransformNode* container_transform =
                   property_trees()
                       ->transform_tree_mutable()
                       .FindNodeFromElementId(container_id)) {
      container_transform_id = container_transform->id;
      accumulated_offset -= StickyPositionOffset(container_transform);
      // Adjust for chained anchor positioned offset. Note that "-=" here is
      // different from the blink version in anchor_position_scroll_data.cc
      // because AnchorPositionOffset() is the opposite of
      // blink::AnchorPositionScrollData::AccmulatedOffset().
      accumulated_offset -= AnchorPositionOffset(
          container_transform, max_updated_node_id, update_data);
    }
    if (container_transform_id > max_updated_node_id) {
      // The adjustment depends on a later transform node that may contain
      // stale data. See UpdateAllTransforms() and UpdateTransforms() for how
      // stale forward dependencies are handled.
      update_data->stale_forward_dependencies.insert(container_transform_id);
    }
  }
  gfx::Vector2dF result = data->accumulated_scroll_origin - accumulated_offset;
  if (!data->needs_scroll_adjustment_in_x) {
    result.set_x(0);
  }
  if (!data->needs_scroll_adjustment_in_y) {
    result.set_y(0);
  }
  return result;
}

void TransformTree::UndoOverscroll(
    const TransformNode* node,
    gfx::Vector2dF& position_adjustment,
    const ViewportPropertyIds* viewport_property_ids) {
  DCHECK(node->should_undo_overscroll);

  const int transform_id =
      viewport_property_ids
          ? viewport_property_ids->overscroll_elasticity_transform
          : kInvalidPropertyNodeId;
  if (transform_id == kInvalidPropertyNodeId)
    return;

  const int clip_id = viewport_property_ids ? viewport_property_ids->outer_clip
                                            : kInvalidPropertyNodeId;
  if (clip_id == kInvalidPropertyNodeId)
    return;

  const TransformNode* overscroll_node = Node(transform_id);
  const gfx::Vector2dF overscroll_offset =
      overscroll_node->scroll_offset.OffsetFromOrigin();
  if (overscroll_offset.IsZero())
    return;

  position_adjustment +=
      gfx::ScaleVector2d(overscroll_offset, 1.f / page_scale_factor());

  ClipTree& clip_tree = property_trees()->clip_tree_mutable();
  ClipNode* clip_node = clip_tree.Node(clip_id);
  DCHECK(clip_node);

  // Inflate the clip rect based on the overscroll direction.
  gfx::OutsetsF outsets;
  position_adjustment.x() < 0 ? outsets.set_left(-position_adjustment.x())
                              : outsets.set_right(position_adjustment.x());
  position_adjustment.y() < 0 ? outsets.set_top(-position_adjustment.y())
                              : outsets.set_bottom(position_adjustment.y());
  clip_node->clip.Outset(outsets);
  clip_tree.set_needs_update(true);
}

void TransformTree::UpdateLocalTransform(
    TransformNode* node,
    const ViewportPropertyIds* viewport_property_ids,
    UpdateTransformsData* update_data) {
  gfx::Transform transform;
  transform.Translate3d(node->post_translation.x() + node->origin.x(),
                        node->post_translation.y() + node->origin.y(),
                        node->origin.z());

  gfx::Vector2dF position_adjustment;
  if (node->moved_by_outer_viewport_bounds_delta_y) {
    position_adjustment.set_y(
        property_trees()->outer_viewport_container_bounds_delta().y());
  }
  if (node->should_undo_overscroll)
    UndoOverscroll(node, position_adjustment, viewport_property_ids);
  transform.Translate(position_adjustment -
                      node->scroll_offset.OffsetFromOrigin());
  transform.Translate(StickyPositionOffset(node));
  if (node->anchor_position_scroll_data_id >= 0) {
    transform.Translate(AnchorPositionOffset(node, node->id - 1, update_data));
    // Make sure the damage rect is tracked.
    node->transform_changed = true;
  }
  transform.PreConcat(node->local);
  transform.Translate3d(gfx::Point3F() - node->origin);

  node->set_to_parent(transform);
  node->needs_local_transform_update = false;
}

void TransformTree::UpdateScreenSpaceTransform(TransformNode* node,
                                               TransformNode* parent_node) {
  DCHECK(parent_node);
  gfx::Transform to_screen_space_transform = ToScreen(parent_node->id);
  if (node->flattens_inherited_transform)
    to_screen_space_transform.Flatten();
  to_screen_space_transform.PreConcat(node->to_parent);
  node->ancestors_are_invertible = parent_node->ancestors_are_invertible;
  node->node_and_ancestors_are_flat =
      parent_node->node_and_ancestors_are_flat && node->to_parent.IsFlat();
  SetToScreen(node->id, to_screen_space_transform);

  gfx::Transform from_screen;
  if (!ToScreen(node->id).GetInverse(&from_screen))
    node->ancestors_are_invertible = false;
  SetFromScreen(node->id, from_screen);
}

void TransformTree::UpdateAnimationProperties(TransformNode* node,
                                              TransformNode* parent_node) {
  DCHECK(parent_node);
  bool ancestor_is_animating = false;
  ancestor_is_animating = parent_node->to_screen_is_potentially_animated;
  node->to_screen_is_potentially_animated =
      node->has_potential_animation || ancestor_is_animating;
}

void TransformTree::UndoSnapping(TransformNode* node) {
  // to_parent transform has snapping from previous frame baked in.
  // We need to undo it and use the un-snapped transform to compute current
  // target and screen space transforms.
  node->to_parent.Translate(-node->snap_amount.x(), -node->snap_amount.y());
  node->snap_amount = gfx::Vector2dF();
}

void TransformTree::UpdateSnapping(TransformNode* node) {
  if (!node->should_be_snapped || node->to_screen_is_potentially_animated ||
      !ToScreen(node->id).IsScaleOrTranslation() ||
      !node->ancestors_are_invertible) {
    return;
  }

  // Snapping must be done in target space (the pixels we care about) and then
  // the render pass should also be snapped if necessary. But, we do it in
  // screen space because it is easier and works most of the time if there is
  // no intermediate render pass with a snap-destrying transform. If ST is the
  // screen space transform and ST' is ST with its translation components
  // rounded, then what we're after is the scroll delta X, where ST * X = ST'.
  // I.e., we want a transform that will realize our snap. It follows that
  // X = ST^-1 * ST'. We cache ST and ST^-1 to make this more efficient.
  DCHECK_LT(node->id, static_cast<int>(cached_data_.size()));
  gfx::Transform& to_screen = cached_data_[node->id].to_screen;
  to_screen.Round2dTranslationComponents();
  gfx::Transform& from_screen = cached_data_[node->id].from_screen;
  gfx::Transform delta = from_screen;
  delta *= to_screen;

  constexpr float kTolerance = 1e-4f;
  DCHECK(delta.IsApproximatelyIdentityOrTranslation(kTolerance))
      << delta.ToString();

  gfx::Vector2dF translation = delta.To2dTranslation();
  node->snap_amount = translation;
  if (translation.IsZero())
    return;

  from_screen.PostTranslate(-translation);
  node->to_parent.Translate(translation);
  // Avoid accumulation of errors in to_parent.
  if (node->to_parent.IsApproximatelyIdentityOrIntegerTranslation(kTolerance))
    node->to_parent.RoundToIdentityOrIntegerTranslation();
}

void TransformTree::UpdateTransformChanged(TransformNode* node,
                                           TransformNode* parent_node) {
  DCHECK(parent_node);
  if (parent_node->transform_changed)
    node->transform_changed = true;
}

void TransformTree::UpdateNodeAndAncestorsAreAnimatedOrInvertible(
    TransformNode* node,
    TransformNode* parent_node) {
  DCHECK(parent_node);
  if (!parent_node->node_and_ancestors_are_animated_or_invertible) {
    node->node_and_ancestors_are_animated_or_invertible = false;
    return;
  }
  bool is_invertible = node->is_invertible;
  // Even when the current node's transform and the parent's screen space
  // transform are invertible, the current node's screen space transform can
  // become uninvertible due to floating-point arithmetic.
  if (!node->ancestors_are_invertible && parent_node->ancestors_are_invertible)
    is_invertible = false;
  node->node_and_ancestors_are_animated_or_invertible =
      node->has_potential_animation || is_invertible;
}

void TransformTree::UpdateNodeOrAncestorsWillChangeTransform(
    TransformNode* node,
    TransformNode* parent_node) {
  node->node_or_ancestors_will_change_transform = node->will_change_transform;
  if (parent_node) {
    node->node_or_ancestors_will_change_transform |=
        parent_node->node_or_ancestors_will_change_transform;
  }
}

void TransformTree::SetRootScaleAndTransform(
    float device_scale_factor,
    const gfx::Transform& device_transform) {
  device_scale_factor_ = device_scale_factor;
  gfx::Vector2dF device_transform_scale_components =
      gfx::ComputeTransform2dScaleComponents(device_transform, 1.f);

  // Not handling the rare case of different x and y device scale.
  device_transform_scale_factor_ =
      std::max(device_transform_scale_components.x(),
               device_transform_scale_components.y());

  // Let DT be the device transform and DSF be the matrix scaled by (device
  // scale factor * page scale factor for root). Let Screen Space Scale(SSS) =
  // scale component of DT*DSF. The screen space transform of the root
  // transform node is set to SSS and the post local transform of the contents
  // root node is set to SSS^-1*DT*DSF.
  gfx::Transform transform = device_transform;
  transform.Scale(device_scale_factor, device_scale_factor);
  gfx::Vector2dF screen_space_scale =
      gfx::ComputeTransform2dScaleComponents(transform, device_scale_factor);
  DCHECK_NE(screen_space_scale.x(), 0.f);
  DCHECK_NE(screen_space_scale.y(), 0.f);

  gfx::Transform root_to_screen;
  root_to_screen.Scale(screen_space_scale.x(), screen_space_scale.y());
  gfx::Transform root_from_screen = root_to_screen.GetCheckedInverse();
  if (root_to_screen != ToScreen(kRootPropertyNodeId)) {
    SetToScreen(kRootPropertyNodeId, root_to_screen);
    SetFromScreen(kRootPropertyNodeId, root_from_screen);
    set_needs_update(true);
  }

  transform.PostConcat(root_from_screen);
  TransformNode* contents_root_node = Node(kContentsRootPropertyNodeId);
  if (contents_root_node->local != transform) {
    contents_root_node->local = transform;
    contents_root_node->needs_local_transform_update = true;
    set_needs_update(true);
  }
}

void TransformTree::UpdateOuterViewportContainerBoundsDelta() {
  if (nodes_affected_by_outer_viewport_bounds_delta_.empty())
    return;

  set_needs_update(true);
  for (int i : nodes_affected_by_outer_viewport_bounds_delta_)
    Node(i)->needs_local_transform_update = true;
}

void TransformTree::AddNodeAffectedByOuterViewportBoundsDelta(int node_id) {
  nodes_affected_by_outer_viewport_bounds_delta_.push_back(node_id);
}

bool TransformTree::HasNodesAffectedByOuterViewportBoundsDelta() const {
  return !nodes_affected_by_outer_viewport_bounds_delta_.empty();
}

const gfx::Transform& TransformTree::FromScreen(int node_id) const {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  return cached_data_[node_id].from_screen;
}

void TransformTree::SetFromScreen(int node_id,
                                  const gfx::Transform& transform) {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  cached_data_[node_id].from_screen = transform;
}

const gfx::Transform& TransformTree::ToScreen(int node_id) const {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  return cached_data_[node_id].to_screen;
}

void TransformTree::SetToScreen(int node_id, const gfx::Transform& transform) {
  DCHECK(static_cast<int>(cached_data_.size()) > node_id);
  cached_data_[node_id].to_screen = transform;
  cached_data_[node_id].is_showing_backface = transform.IsBackFaceVisible();
}

#if DCHECK_IS_ON()
bool TransformTree::operator==(const TransformTree& other) const {
  return PropertyTree::operator==(other) &&
         page_scale_factor_ == other.page_scale_factor() &&
         device_scale_factor_ == other.device_scale_factor() &&
         device_transform_scale_factor_ ==
             other.device_transform_scale_factor() &&
         nodes_affected_by_outer_viewport_bounds_delta_ ==
             other.nodes_affected_by_outer_viewport_bounds_delta() &&
         cached_data_ == other.cached_data();
}
#endif

StickyPositionNodeData* TransformTree::MutableStickyPositionData(int node_id) {
  const TransformNode* node = Node(node_id);
  if (node->sticky_position_constraint_id == -1)
    return nullptr;
  return &sticky_position_data_[node->sticky_position_constraint_id];
}

StickyPositionNodeData& TransformTree::EnsureStickyPositionData(int node_id) {
  TransformNode* node = Node(node_id);
  if (node->sticky_position_constraint_id == -1) {
    node->sticky_position_constraint_id = sticky_position_data_.size();
    sticky_position_data_.push_back(StickyPositionNodeData());
  }
  return sticky_position_data_[node->sticky_position_constraint_id];
}

EffectTree::EffectTree(PropertyTrees* property_trees)
    : PropertyTree<EffectNode>(property_trees) {
  render_surfaces_.push_back(nullptr);
}

EffectTree::~EffectTree() = default;

int EffectTree::Insert(const EffectNode& tree_node, int parent_id) {
  int node_id = PropertyTree<EffectNode>::Insert(tree_node, parent_id);
  DCHECK_EQ(node_id, static_cast<int>(render_surfaces_.size()));

  render_surfaces_.push_back(nullptr);
  return node_id;
}

void EffectTree::RemoveNodes(size_t n) {
  PropertyTree<EffectNode>::RemoveNodes(n);
  render_surfaces_.resize(render_surfaces_.size() - n);
}

void EffectTree::clear() {
  PropertyTree<EffectNode>::clear();
  render_surfaces_.clear();
  render_surfaces_.push_back(nullptr);

#if DCHECK_IS_ON()
  EffectTree tree;
  DCHECK(tree == *this);
#endif
}

float EffectTree::EffectiveOpacity(const EffectNode* node) const {
  return node->subtree_hidden ? 0.f : node->opacity;
}

void EffectTree::UpdateOpacities(EffectNode* node, EffectNode* parent_node) {
  node->screen_space_opacity = EffectiveOpacity(node);

  if (parent_node)
    node->screen_space_opacity *= parent_node->screen_space_opacity;
}

void EffectTree::UpdateSubtreeHidden(EffectNode* node,
                                     EffectNode* parent_node) {
  if (parent_node)
    node->subtree_hidden |= parent_node->subtree_hidden;
}

void EffectTree::UpdateIsDrawn(EffectNode* node, EffectNode* parent_node) {
  // Nodes that have screen space opacity 0 are hidden. So they are not drawn.
  // Exceptions:
  // 1) Nodes that contribute to copy requests, whether hidden or not, must be
  //    drawn.
  // 2) Nodes that have a valid SubtreeCaptureId, must be drawn so that they can
  //    be captured by the FrameSinkVideoCapturer.
  // 3) Nodes that have a backdrop filter.
  // 4) Nodes with animating screen space opacity on main thread or pending tree
  //    are drawn if their parent is drawn irrespective of their opacity.
  if (node->has_copy_request || node->cache_render_surface ||
      node->subtree_capture_id.is_valid()) {
    node->is_drawn = true;
  } else if (EffectiveOpacity(node) == 0.f &&
             (!node->has_potential_opacity_animation ||
              property_trees()->is_active()) &&
             node->backdrop_filters.IsEmpty()) {
    node->is_drawn = false;
  } else if (parent_node) {
    node->is_drawn = parent_node->is_drawn;
  } else {
    node->is_drawn = true;
  }
}

void EffectTree::UpdateEffectChanged(EffectNode* node,
                                     EffectNode* parent_node) {
  if (parent_node && parent_node->effect_changed) {
    node->effect_changed = true;
  }
}

void EffectTree::UpdateHasFilters(EffectNode* node, EffectNode* parent_node) {
  node->node_or_ancestor_has_filters =
      !node->filters.IsEmpty() || node->has_potential_filter_animation;
  if (parent_node) {
    node->node_or_ancestor_has_filters |=
        parent_node->node_or_ancestor_has_filters;
  }
}

void EffectTree::UpdateHasFastRoundedCorner(EffectNode* node,
                                            EffectNode* parent_node) {
  node->node_or_ancestor_has_fast_rounded_corner = node->is_fast_rounded_corner;
  if (parent_node) {
    node->node_or_ancestor_has_fast_rounded_corner |=
        parent_node->node_or_ancestor_has_fast_rounded_corner;
  }
}

void EffectTree::UpdateBackfaceVisibility(EffectNode* node,
                                          EffectNode* parent_node) {
  if (parent_node && parent_node->hidden_by_backface_visibility) {
    node->hidden_by_backface_visibility = true;
    return;
  }
  if (node->double_sided) {
    node->hidden_by_backface_visibility = false;
    return;
  }
  node->hidden_by_backface_visibility = property_trees()
                                            ->transform_tree()
                                            .cached_data()[node->transform_id]
                                            .is_showing_backface;
}

void EffectTree::UpdateHasMaskingChild(EffectNode* node,
                                       EffectNode* parent_node) {
  // Reset to false when a node is first met. We'll set the bit later
  // when we actually encounter a masking child.
  node->has_masking_child = false;
  if (node->blend_mode == SkBlendMode::kDstIn) {
    parent_node->has_masking_child = true;
  }
}

void EffectTree::UpdateOnlyDrawsVisibleContent(EffectNode* node,
                                               EffectNode* parent_node) {
  node->only_draws_visible_content =
      !node->has_copy_request && !node->subtree_capture_id.is_valid() &&
      !node->view_transition_element_resource_id.IsValid();
  if (parent_node)
    node->only_draws_visible_content &= parent_node->only_draws_visible_content;
  if (!node->backdrop_filters.IsEmpty()) {
    node->only_draws_visible_content &=
        !node->backdrop_filters.HasFilterOfType(FilterOperation::ZOOM);
  }
}

void EffectTree::UpdateSurfaceContentsScale(EffectNode* effect_node) {
  if (!effect_node->HasRenderSurface()) {
    effect_node->surface_contents_scale = gfx::Vector2dF(1.0f, 1.0f);
    return;
  }

  const TransformTree& transform_tree = property_trees()->transform_tree();
  float layer_scale_factor = transform_tree.device_scale_factor() *
                             transform_tree.device_transform_scale_factor();
  const TransformNode* transform_node =
      transform_tree.Node(effect_node->transform_id);
  if (transform_node->in_subtree_of_page_scale_layer)
    layer_scale_factor *= transform_tree.page_scale_factor();

  const gfx::Vector2dF old_scale = effect_node->surface_contents_scale;
  effect_node->surface_contents_scale = gfx::ComputeTransform2dScaleComponents(
      transform_tree.ToScreen(transform_node->id), layer_scale_factor);

  // If surface contents scale changes, draw transforms are no longer valid.
  // Invalidates the draw transform cache and updates the clip for the surface.
  if (old_scale != effect_node->surface_contents_scale) {
    property_trees()->clip_tree_mutable().set_needs_update(true);
    property_trees()->UpdateTransformTreeUpdateNumber();
  }
}

bool EffectTree::OnOpacityAnimated(ElementId id, float opacity) {
  EffectNode* node = FindNodeFromElementId(id);
  // TODO(crbug.com/40828469): Remove this when we no longer animate
  // non-existent nodes.
  if (!node) {
    AnimationUpdateOnMissingPropertyNodeUMALog(true);
    return false;
  }
  AnimationUpdateOnMissingPropertyNodeUMALog(false);
  if (node->opacity == opacity)
    return false;
  node->opacity = opacity;
  node->effect_changed = true;
  property_trees()->set_changed(true);
  property_trees()->effect_tree_mutable().set_needs_update(true);
  return true;
}

bool EffectTree::OnFilterAnimated(ElementId id,
                                  const FilterOperations& filters) {
  EffectNode* node = FindNodeFromElementId(id);
  // TODO(crbug.com/40828469): Remove this when we no longer animate
  // non-existent nodes.
  if (!node) {
    AnimationUpdateOnMissingPropertyNodeUMALog(true);
    return false;
  }
  AnimationUpdateOnMissingPropertyNodeUMALog(false);
  if (node->filters == filters)
    return false;
  node->filters = filters;
  node->effect_changed = true;
  property_trees()->set_changed(true);
  property_trees()->effect_tree_mutable().set_needs_update(true);
  return true;
}

bool EffectTree::OnBackdropFilterAnimated(
    ElementId id,
    const FilterOperations& backdrop_filters) {
  EffectNode* node = FindNodeFromElementId(id);
  // TODO(crbug.com/40828469): Remove this when we no longer animate
  // non-existent nodes.
  if (!node) {
    AnimationUpdateOnMissingPropertyNodeUMALog(true);
    return false;
  }
  AnimationUpdateOnMissingPropertyNodeUMALog(false);
  if (node->backdrop_filters == backdrop_filters)
    return false;
  node->backdrop_filters = backdrop_filters;
  node->effect_changed = true;
  property_trees()->set_changed(true);
  property_trees()->effect_tree_mutable().set_needs_update(true);
  return true;
}

void EffectTree::UpdateEffects(int id) {
  EffectNode* node = Node(id);
  EffectNode* parent_node = parent(node);

  UpdateOpacities(node, parent_node);
  UpdateSubtreeHidden(node, parent_node);
  UpdateIsDrawn(node, parent_node);
  UpdateEffectChanged(node, parent_node);
  UpdateHasFilters(node, parent_node);
  UpdateHasFastRoundedCorner(node, parent_node);
  UpdateBackfaceVisibility(node, parent_node);
  UpdateHasMaskingChild(node, parent_node);
  UpdateOnlyDrawsVisibleContent(node, parent_node);
  UpdateClosestAncestorSharedElement(node, parent_node);
  UpdateSurfaceContentsScale(node);
}

void EffectTree::UpdateClosestAncestorSharedElement(EffectNode* node,
                                                    EffectNode* parent_node) {
  if (node->view_transition_element_resource_id.IsValid()) {
    node->closest_ancestor_with_shared_element_id = node->id;
  } else if (parent_node) {
    node->closest_ancestor_with_shared_element_id =
        parent_node->closest_ancestor_with_shared_element_id;
  }
}

void EffectTree::AddCopyRequest(
    int node_id,
    std::unique_ptr<viz::CopyOutputRequest> request) {
  copy_requests_.insert(std::make_pair(node_id, std::move(request)));
}

void EffectTree::PullCopyRequestsFrom(CopyRequestMap& new_copy_requests) {
  copy_requests_ = std::move(new_copy_requests);
}

void EffectTree::TakeCopyRequestsAndTransformToSurface(
    int node_id,
    std::vector<std::unique_ptr<viz::CopyOutputRequest>>* requests) {
  EffectNode* effect_node = Node(node_id);
  DCHECK(effect_node->HasRenderSurface());
  DCHECK(effect_node->has_copy_request);

  // The area needs to be transformed from the space of content that draws to
  // the surface to the space of the surface itself.
  int destination_id = effect_node->transform_id;
  int source_id;
  if (effect_node->parent_id != kInvalidPropertyNodeId) {
    // For non-root surfaces, transform only by sub-layer scale.
    source_id = destination_id;
  } else {
    // The root surface doesn't have the notion of sub-layer scale, but instead
    // has a similar notion of transforming from the space of the root layer to
    // the space of the screen.
    DCHECK_EQ(kRootPropertyNodeId, destination_id);
    source_id = kContentsRootPropertyNodeId;
  }
  gfx::Transform transform;
  property_trees()->GetToTarget(source_id, node_id, &transform);

  // Move each CopyOutputRequest out of |copy_requests_| and into |requests|,
  // adjusting the source area and scale ratio of each. If the transform is
  // something other than a straightforward translate+scale, the copy requests
  // will be dropped.
  auto range = copy_requests_.equal_range(node_id);
  if (transform.IsPositiveScaleOrTranslation()) {
    // Transform a vector in content space to surface space to determine how the
    // scale ratio of each CopyOutputRequest should be adjusted. Since the scale
    // ratios are provided integer coordinates, the basis vector determines the
    // precision w.r.t. the fractional part of the Transform's scale factors.
    constexpr gfx::Vector2d kContentVector(1024, 1024);
    gfx::RectF surface_rect = transform.MapRect(
        gfx::RectF(0, 0, kContentVector.x(), kContentVector.y()));

    for (auto it = range.first; it != range.second; ++it) {
      viz::CopyOutputRequest* const request = it->second.get();
      if (request->has_area()) {
        // Avoid creating bigger copy area which may contain unnecessary
        // area if the error margin is tiny.
        constexpr float kEpsilon = 0.001f;
        request->set_area(MathUtil::MapEnclosingClippedRectIgnoringError(
            transform, request->area(), kEpsilon));
      }

      // Only adjust the scale ratio if the request specifies one, or if it
      // specifies a result selection. Otherwise, the requestor is expecting a
      // copy of the exact source pixels. If the adjustment to the scale ratio
      // would produce out-of-range values, drop the copy request.
      if (request->is_scaled() || request->has_result_selection()) {
        float scale_from_x_f = request->scale_from().x() * surface_rect.width();
        float scale_from_y_f =
            request->scale_from().y() * surface_rect.height();
        if (std::isnan(scale_from_x_f) ||
            !base::IsValueInRangeForNumericType<int>(scale_from_x_f) ||
            std::isnan(scale_from_y_f) ||
            !base::IsValueInRangeForNumericType<int>(scale_from_y_f)) {
          continue;
        }
        int scale_to_x = request->scale_to().x();
        int scale_to_y = request->scale_to().y();
        if (!base::CheckMul(scale_to_x, kContentVector.x())
                 .AssignIfValid(&scale_to_x) ||
            !base::CheckMul(scale_to_y, kContentVector.y())
                 .AssignIfValid(&scale_to_y)) {
          continue;
        }
        int scale_from_x = base::ClampRound(scale_from_x_f);
        int scale_from_y = base::ClampRound(scale_from_y_f);
        if (scale_from_x <= 0 || scale_from_y <= 0 || scale_to_x <= 0 ||
            scale_to_y <= 0) {
          // Transformed scaling ratio became illegal. Drop the request to
          // provide an empty response.
          continue;
        }
        request->SetScaleRatio(gfx::Vector2d(scale_from_x, scale_from_y),
                               gfx::Vector2d(scale_to_x, scale_to_y));
      }

      requests->push_back(std::move(it->second));
    }
  }
  copy_requests_.erase(range.first, range.second);
}

bool EffectTree::HasCopyRequests() const {
  return !copy_requests_.empty();
}

void EffectTree::ClearCopyRequests() {
  for (auto& node : nodes()) {
    node.subtree_has_copy_request = false;
    node.has_copy_request = false;
    node.closest_ancestor_with_copy_request_id = kInvalidPropertyNodeId;
  }

  // Any copy requests that are still left will be aborted (sending an empty
  // result) on destruction.
  copy_requests_.clear();
  set_needs_update(true);
}

void EffectTree::GetRenderSurfaceChangedFlags(
    std::vector<RenderSurfacePropertyChangedFlags>& flags) const {
  flags.resize(size());
  for (int id = kContentsRootPropertyNodeId; id < static_cast<int>(size());
       ++id) {
    if (render_surfaces_[id])
      flags[id] = render_surfaces_[id]->GetPropertyChangeFlags();
    else
      flags[id] = {false, false};
  }
}

void EffectTree::ApplyRenderSurfaceChangedFlags(
    const std::vector<RenderSurfacePropertyChangedFlags>& flags) {
  DCHECK_EQ(flags.size(), size());
  for (int id = kContentsRootPropertyNodeId; id < static_cast<int>(size());
       ++id) {
    if (render_surfaces_[id])
      render_surfaces_[id]->ApplyPropertyChangeFlags(flags[id]);
  }
}

int EffectTree::LowestCommonAncestorWithRenderSurface(int id_1,
                                                      int id_2) const {
  DCHECK(GetRenderSurface(id_1));
  DCHECK(GetRenderSurface(id_2));
  while (id_1 != id_2) {
    if (id_1 < id_2)
      id_2 = Node(id_2)->target_id;
    else
      id_1 = Node(id_1)->target_id;
  }

  return id_1;
}

bool EffectTree::ContributesToDrawnSurface(int id) const {
  // All drawn nodes contribute to drawn surface.
  // Exception : Nodes that are hidden and are drawn only for the sake of
  // copy requests.
  const EffectNode* node = Node(id);
  const EffectNode* parent_node = parent(node);
  return node->is_drawn && (!parent_node || parent_node->is_drawn);
}

void EffectTree::ResetChangeTracking() {
  for (int id = kContentsRootPropertyNodeId; id < static_cast<int>(size());
       ++id) {
    Node(id)->effect_changed = false;
    if (render_surfaces_[id])
      render_surfaces_[id]->ResetPropertyChangedFlags();
  }
}

void EffectTree::TakeRenderSurfaces(
    std::vector<std::unique_ptr<RenderSurfaceImpl>>* render_surfaces) {
  for (int id = kContentsRootPropertyNodeId; id < static_cast<int>(size());
       ++id) {
    if (render_surfaces_[id]) {
      render_surfaces->push_back(std::move(render_surfaces_[id]));
    }
  }
}

bool EffectTree::CreateOrReuseRenderSurfaces(
    std::vector<std::unique_ptr<RenderSurfaceImpl>>* old_render_surfaces,
    LayerTreeImpl* layer_tree_impl) {
  // Make a list of {stable id, node id} pairs for nodes that are supposed to
  // have surfaces.
  std::vector<std::pair<ElementId, int>> stable_id_node_id_list;
  for (int id = kContentsRootPropertyNodeId; id < static_cast<int>(size());
       ++id) {
    EffectNode* node = Node(id);
    if (node->HasRenderSurface()) {
      stable_id_node_id_list.emplace_back(node->element_id, node->id);
    }
  }

  // Sort by stable id so that we can process the two lists cosequentially.
  std::sort(stable_id_node_id_list.begin(), stable_id_node_id_list.end());
  std::sort(old_render_surfaces->begin(), old_render_surfaces->end(),
            [](const std::unique_ptr<RenderSurfaceImpl>& a,
               const std::unique_ptr<RenderSurfaceImpl>& b) {
              return a->id() < b->id();
            });

  bool render_surfaces_changed = false;
  auto surfaces_list_it = old_render_surfaces->begin();
  auto id_list_it = stable_id_node_id_list.begin();
  while (surfaces_list_it != old_render_surfaces->end() &&
         id_list_it != stable_id_node_id_list.end()) {
    if ((*surfaces_list_it)->id() == id_list_it->first) {
      int new_node_id = id_list_it->second;
      render_surfaces_[new_node_id] = std::move(*surfaces_list_it);
      render_surfaces_[new_node_id]->set_effect_tree_index(new_node_id);
      surfaces_list_it++;
      id_list_it++;
      continue;
    }

    render_surfaces_changed = true;

    if (id_list_it->first < (*surfaces_list_it)->id()) {
      int new_node_id = id_list_it->second;
      render_surfaces_[new_node_id] = std::make_unique<RenderSurfaceImpl>(
          layer_tree_impl, id_list_it->first);
      render_surfaces_[new_node_id]->set_effect_tree_index(new_node_id);
      id_list_it++;
    } else {
      surfaces_list_it++;
    }
  }

  if (surfaces_list_it != old_render_surfaces->end() ||
      id_list_it != stable_id_node_id_list.end()) {
    render_surfaces_changed = true;
  }

  while (id_list_it != stable_id_node_id_list.end()) {
    int new_node_id = id_list_it->second;
    render_surfaces_[new_node_id] =
        std::make_unique<RenderSurfaceImpl>(layer_tree_impl, id_list_it->first);
    render_surfaces_[new_node_id]->set_effect_tree_index(new_node_id);
    id_list_it++;
  }

  return render_surfaces_changed;
}

bool EffectTree::ClippedHitTestRegionIsRectangle(int effect_id) const {
  const EffectNode* effect_node = Node(effect_id);
  for (; effect_node->id != kContentsRootPropertyNodeId;
       effect_node = Node(effect_node->target_id)) {
    gfx::Transform to_target;
    if (!property_trees()->GetToTarget(effect_node->transform_id,
                                       effect_node->target_id, &to_target) ||
        !to_target.Preserves2dAxisAlignment())
      return false;
  }
  return true;
}

bool EffectTree::HitTestMayBeAffectedByMask(int effect_id) const {
  const EffectNode* effect_node = Node(effect_id);
  for (; effect_node->id != kContentsRootPropertyNodeId;
       effect_node = Node(effect_node->parent_id)) {
    if (!effect_node->mask_filter_info.IsEmpty() ||
        effect_node->has_masking_child)
      return true;
  }
  return false;
}

EffectTree::CopyRequestMap EffectTree::TakeCopyRequests() {
  // Property trees need to get rebuilt since effect nodes (and render surfaces)
  // that were created only for the copy requests we just pushed are no longer
  // needed.
  if (property_trees()->is_main_thread() && !copy_requests_.empty())
    property_trees()->set_needs_rebuild(true);
  return std::move(copy_requests_);
}

ClipTree::ClipTree(PropertyTrees* property_trees)
    : PropertyTree<ClipNode>(property_trees) {}

void ClipTree::SetViewportClip(gfx::RectF viewport_rect) {
  if (size() < 2)
    return;
  ClipNode* node = Node(1);
  if (viewport_rect == node->clip)
    return;
  node->clip = viewport_rect;
  set_needs_update(true);
}

gfx::RectF ClipTree::ViewportClip() const {
  const size_t min_size = 1;
  DCHECK_GT(size(), min_size);
  return Node(kViewportPropertyNodeId)->clip;
}

#if DCHECK_IS_ON()
bool ClipTree::operator==(const ClipTree& other) const {
  return PropertyTree::operator==(other);
}
#endif

EffectTree& EffectTree::operator=(const EffectTree& from) {
  PropertyTree::operator=(from);
  render_surfaces_.resize(size());
  // copy_requests_ are omitted here, since these need to be moved rather
  // than copied or assigned.

  return *this;
}

#if DCHECK_IS_ON()
bool EffectTree::operator==(const EffectTree& other) const {
  return PropertyTree::operator==(other);
}
#endif

ScrollTree::ScrollTree(PropertyTrees* property_trees)
    : PropertyTree<ScrollNode>(property_trees) {}

ScrollTree::~ScrollTree() = default;

ScrollTree& ScrollTree::operator=(const ScrollTree& from) {
  PropertyTree::operator=(from);
  scrolling_contents_cull_rects_ = from.scrolling_contents_cull_rects_;
  currently_scrolling_node_id_ = kInvalidPropertyNodeId;
  // Maps for ScrollOffsets/SyncedScrollOffsets are intentionally omitted here
  // since we can not directly copy them. Pushing of these updates from main
  // currently depends on Layer properties for scroll offset animation changes
  // (setting clobber_active_value for scroll offset animations interrupted on
  // the main thread) being pushed to impl first.
  // |callbacks_| is omitted because it's for the main thread only.
  return *this;
}

#if DCHECK_IS_ON()
bool ScrollTree::operator==(const ScrollTree& other) const {
  if (scroll_offset_map_ != other.scroll_offset_map_)
    return false;
  if (synced_scroll_offset_map_ != other.synced_scroll_offset_map_)
    return false;
  if (callbacks_.get() != other.callbacks_.get())
    return false;

  bool is_currently_scrolling_node_equal =
      currently_scrolling_node_id_ == other.currently_scrolling_node_id_;

  return PropertyTree::operator==(other) && is_currently_scrolling_node_equal;
}

void ScrollTree::CopyCompleteTreeState(const ScrollTree& other) {
  currently_scrolling_node_id_ = other.currently_scrolling_node_id_;
  scroll_offset_map_ = other.scroll_offset_map_;
  synced_scroll_offset_map_ = other.synced_scroll_offset_map_;
  callbacks_ = other.callbacks_;
}
#endif  // DCHECK_IS_ON()

bool ScrollTree::CanRealizeScrollsOnActiveTree(const ScrollNode& node) const {
  return node.transform_id != kInvalidPropertyNodeId && node.is_composited &&
         GetMainThreadRepaintReasons(node) ==
             MainThreadScrollingReason::kNotScrollingOnMain;
}

bool ScrollTree::CanRealizeScrollsOnPendingTree(const ScrollNode& node) const {
  return node.transform_id != kInvalidPropertyNodeId && !node.is_composited &&
         GetMainThreadRepaintReasons(node) ==
             MainThreadScrollingReason::kNotScrollingOnMain;
}

bool ScrollTree::ShouldRealizeScrollsOnMain(const ScrollNode& node) const {
  return node.transform_id != kInvalidPropertyNodeId &&
         GetMainThreadRepaintReasons(node) !=
             MainThreadScrollingReason::kNotScrollingOnMain;
}

uint32_t ScrollTree::GetMainThreadRepaintReasons(const ScrollNode& node) const {
  // kPopupNoThreadedInput is not a repaint reason so should be excluded.
  uint32_t reasons = node.main_thread_scrolling_reasons &
                     ~MainThreadScrollingReason::kPopupNoThreadedInput;
  if (!MainThreadScrollingReason::AreRepaintReasons(reasons)) {
    SCOPED_CRASH_KEY_NUMBER("Bug349709014", "reasons", reasons);
    NOTREACHED();
  }
  return reasons;
}

void ScrollTree::clear() {
  PropertyTree<ScrollNode>::clear();

  if (property_trees()->is_main_thread()) {
    currently_scrolling_node_id_ = kInvalidPropertyNodeId;
    scroll_offset_map_.clear();
  }

#if DCHECK_IS_ON()
  ScrollTree tree;
  if (property_trees()->is_main_thread()) {
    tree.callbacks_ = callbacks_;
  } else {
    DCHECK(scroll_offset_map_.empty());
    tree.currently_scrolling_node_id_ = currently_scrolling_node_id_;
    tree.synced_scroll_offset_map_ = synced_scroll_offset_map_;
  }
  DCHECK(tree == *this);
#endif
}

gfx::PointF ScrollTree::MaxScrollOffset(int scroll_node_id) const {
  const ScrollNode* scroll_node = Node(scroll_node_id);
  gfx::SizeF scroll_bounds = this->scroll_bounds(scroll_node_id);

  if (scroll_bounds.IsEmpty()) {
    return gfx::PointF();
  }

  const TransformTree& transform_tree = property_trees()->transform_tree();
  float scale_factor = 1.f;
  if (scroll_node->max_scroll_offset_affected_by_page_scale)
    scale_factor = transform_tree.page_scale_factor();

  gfx::SizeF scaled_scroll_bounds = gfx::ScaleSize(scroll_bounds, scale_factor);
  scaled_scroll_bounds.SetSize(std::floor(scaled_scroll_bounds.width()),
                               std::floor(scaled_scroll_bounds.height()));

  gfx::Size clip_layer_bounds = container_bounds(scroll_node->id);

  gfx::PointF max_offset(
      scaled_scroll_bounds.width() - clip_layer_bounds.width(),
      scaled_scroll_bounds.height() - clip_layer_bounds.height());

  max_offset.Scale(1 / scale_factor);
  max_offset.SetToMax(gfx::PointF());
  return max_offset;
}

gfx::SizeF ScrollTree::scroll_bounds(int scroll_node_id) const {
  const ScrollNode* scroll_node = Node(scroll_node_id);
  gfx::SizeF bounds(scroll_node->bounds);
  if (scroll_node->scrolls_inner_viewport) {
    const auto& delta = property_trees()->inner_viewport_scroll_bounds_delta();
    bounds.Enlarge(delta.x(), delta.y());
  }
  return bounds;
}

void ScrollTree::OnScrollOffsetAnimated(ElementId id,
                                        int scroll_tree_index,
                                        const gfx::PointF& scroll_offset,
                                        LayerTreeImpl* layer_tree_impl) {
  // Only active tree needs to be updated, pending tree will find out about
  // these changes as a result of the shared SyncedProperty.
  if (!property_trees()->is_active())
    return;

  TRACE_EVENT2("cc", "ScrollTree::OnScrollOffsetAnimated", "x",
               scroll_offset.x(), "y", scroll_offset.y());
  ScrollNode* scroll_node = Node(scroll_tree_index);
  if (SetScrollOffset(id,
                      ClampScrollOffsetToLimits(scroll_offset, *scroll_node))) {
    layer_tree_impl->DidUpdateScrollOffset(
        id, /*pushed_from_main_or_pending_tree=*/false);
  }
  layer_tree_impl->DidAnimateScrollOffset();
}

gfx::Size ScrollTree::container_bounds(int scroll_node_id) const {
  const ScrollNode* scroll_node = Node(scroll_node_id);
  gfx::Size container_bounds = scroll_node->container_bounds;

  gfx::Vector2dF container_bounds_delta;
  if (scroll_node->scrolls_inner_viewport) {
    container_bounds_delta.Add(
        property_trees()->inner_viewport_container_bounds_delta());
  } else if (scroll_node->scrolls_outer_viewport) {
    container_bounds_delta.Add(
        property_trees()->outer_viewport_container_bounds_delta());
  }

  gfx::Vector2d delta = gfx::ToCeiledVector2d(container_bounds_delta);
  container_bounds.Enlarge(delta.x(), delta.y());

  return container_bounds;
}

ScrollNode* ScrollTree::CurrentlyScrollingNode() {
  ScrollNode* scroll_node = Node(currently_scrolling_node_id_);
  return scroll_node;
}

const ScrollNode* ScrollTree::CurrentlyScrollingNode() const {
  const ScrollNode* scroll_node = Node(currently_scrolling_node_id_);
  return scroll_node;
}

#if DCHECK_IS_ON()
int ScrollTree::CurrentlyScrollingNodeId() const {
  return currently_scrolling_node_id_;
}
#endif

void ScrollTree::set_currently_scrolling_node(int scroll_node_id) {
  currently_scrolling_node_id_ = scroll_node_id;
}

gfx::Transform ScrollTree::ScreenSpaceTransform(int scroll_node_id) const {
  return property_trees()->transform_tree().ToScreen(
      Node(scroll_node_id)->transform_id);
}

SyncedScrollOffset* ScrollTree::GetSyncedScrollOffset(ElementId id) {
  DCHECK(!property_trees()->is_main_thread());
  auto it = synced_scroll_offset_map_.find(id);
  return it != synced_scroll_offset_map_.end() ? it->second.get() : nullptr;
}

const SyncedScrollOffset* ScrollTree::GetSyncedScrollOffset(
    ElementId id) const {
  return const_cast<ScrollTree*>(this)->GetSyncedScrollOffset(id);
}

gfx::Vector2dF ScrollTree::ClampScrollToMaxScrollOffset(
    const ScrollNode& node,
    LayerTreeImpl* layer_tree_impl) {
  gfx::PointF old_offset = current_scroll_offset(node.element_id);
  gfx::PointF clamped_offset = ClampScrollOffsetToLimits(old_offset, node);
  gfx::Vector2dF delta = clamped_offset - old_offset;
  if (!delta.IsZero())
    ScrollBy(node, delta, layer_tree_impl);
  return delta;
}

const gfx::PointF ScrollTree::current_scroll_offset(ElementId id) const {
  if (property_trees()->is_main_thread()) {
    auto it = scroll_offset_map_.find(id);
    return it != scroll_offset_map_.end() ? it->second : gfx::PointF();
  }
  if (const auto* synced_scroll_offset = GetSyncedScrollOffset(id))
    return synced_scroll_offset->Current(property_trees()->is_active());
  return gfx::PointF();
}

gfx::PointF ScrollTree::GetScrollOffsetForScrollTimeline(
    const ScrollNode& scroll_node) const {
  gfx::PointF offset = current_scroll_offset(scroll_node.element_id);
  if (!property_trees()->is_main_thread()) {
    if (const SyncedScrollOffset* synced_offset =
            GetSyncedScrollOffset(scroll_node.element_id)) {
      // Ignore compositor scroll delta if the scroll can't be realized on the
      // corresponding tree because the delta has not been realized yet.
      if (property_trees()->is_active()) {
        if (!CanRealizeScrollsOnActiveTree(scroll_node)) {
          offset = synced_offset->ActiveBase();
        }
      } else if (!CanRealizeScrollsOnActiveTree(scroll_node) &&
                 !CanRealizeScrollsOnPendingTree(scroll_node)) {
        offset = synced_offset->PendingBase();
      }
    }
  }

  const TransformNode* transform_node =
      property_trees()->transform_tree().Node(scroll_node.transform_id);

  // TODO(crbug.com/40894892): current_scroll_offset can disagree with
  // transform_node->scroll_offset if the delta on a main frame update is
  // simply rounding of the scroll position and not using fractional scroll
  // deltas (see needs_scroll_update in PushScrollUpdatesFromMainThread).

  if (transform_node && transform_node->scrolls) {
    // If necessary perform a update for this node to ensure snap amount is
    // accurate. This method is used by scroll timeline, so it is possible for
    // it to get called before transform tree has gone through a full update
    // cycle so this node snap amount may be stale.
    if (transform_node->needs_local_transform_update)
      property_trees()->transform_tree_mutable().UpdateTransforms(
          transform_node->id);

    // The calculated pixel snap amount can be slightly larger than the actual
    // snapping needed, due to floating point precision errors. In general this
    // is fine, but we never want to report a negative scroll offset so avoid
    // that case here.
    // TODO(crbug.com/40688441): Remove the clamping when scroll timeline
    // effects always match the snapping.
    offset = ClampScrollOffsetToLimits(offset - transform_node->snap_amount,
                                       scroll_node);
  }

  return offset;
}

gfx::Vector2dF ScrollTree::PullDeltaForMainThread(
    SyncedScrollOffset* scroll_offset,
    bool use_fractional_deltas,
    bool next_bmf) {
  DCHECK(property_trees()->is_active());

  // Once this setting is enabled, all the complicated rounding logic below can
  // go away.
  if (use_fractional_deltas)
    return scroll_offset->PullDeltaForMainThread(next_bmf);

  // TODO(flackr): We should pass the fractional scroll deltas when Blink fully
  // supports fractional scrolls. crbug.com/414283.
  // TODO(flackr): We should ideally round the fractional scrolls in the same
  // direction as the scroll will be snapped but for common cases this is
  // equivalent to rounding to the nearest integer offset.
  gfx::PointF current_offset =
      scroll_offset->Current(/* is_active_tree */ true);
  gfx::PointF rounded_offset(gfx::ToRoundedPoint(current_offset));
  // The calculation of the difference from the rounded active base is to
  // represent the integer delta that the main thread should know about.
  gfx::PointF active_base = scroll_offset->ActiveBase();
  gfx::Vector2dF diff_active_base =
      active_base - gfx::PointF(gfx::ToRoundedPoint(active_base));
  scroll_offset->SetCurrent(rounded_offset + diff_active_base);
  gfx::Vector2dF delta = scroll_offset->PullDeltaForMainThread(next_bmf);
  scroll_offset->SetCurrent(current_offset);
  return delta;
}

void ScrollTree::CollectScrollDeltas(
    CompositorCommitData* commit_data,
    ElementId inner_viewport_scroll_element_id,
    bool use_fractional_deltas,
    const base::flat_map<ElementId, TargetSnapAreaElementIds>& snapped_elements,
    const MutatorHost* main_thread_mutator_host) {
  DCHECK(!property_trees()->is_main_thread());
  TRACE_EVENT0("cc", "ScrollTree::CollectScrollDeltas");
  for (auto map_entry : synced_scroll_offset_map_) {
    // The presence of a non-null mutator_host indicates that there is a
    // ready-to-commit main frame, hence we are pipelining this main frame.
    bool pipeline = main_thread_mutator_host;
    gfx::Vector2dF scroll_delta;
    // If the ready-to-commit main frame is going to clobber the scroll offset
    // in the active tree, then we shouldn't send a delta down to the main
    // thread.
    bool clobber =
        main_thread_mutator_host &&
        main_thread_mutator_host->ScrollOffsetAnimationWasInterrupted(
            map_entry.first);
    if (!clobber) {
      scroll_delta = PullDeltaForMainThread(map_entry.second.get(),
                                            use_fractional_deltas, pipeline);
    }

    ElementId id = map_entry.first;

    std::optional<TargetSnapAreaElementIds> snap_target_ids;
    if (snapped_elements.contains(id))
      snap_target_ids = snapped_elements.at(id);

    // Snap targets are set at the end of scroll offset animations (i.e when the
    // animation state is updated to FINISHED). The state can be updated after
    // the compositor's draw stage, which means the next attempt to push the
    // snap targets is during the next frame. This makes it possible for the
    // scroll delta to be zero.
    if (!scroll_delta.IsZero() || snap_target_ids) {
      TRACE_EVENT_INSTANT2("cc", "CollectScrollDeltas",
                           TRACE_EVENT_SCOPE_THREAD, "x", scroll_delta.x(), "y",
                           scroll_delta.y());
      CompositorCommitData::ScrollUpdateInfo update(id, scroll_delta,
                                                    snap_target_ids);
      if (id == inner_viewport_scroll_element_id) {
        // Inner (visual) viewport is stored separately.
        commit_data->inner_viewport_scroll = std::move(update);
      } else {
        commit_data->scrolls.push_back(std::move(update));
      }
    }
  }
}

void ScrollTree::CollectScrollDeltasForTesting(bool use_fractional_deltas) {
  for (auto map_entry : synced_scroll_offset_map_) {
    PullDeltaForMainThread(map_entry.second.get(), use_fractional_deltas,
                           /* next_bmf */ false);
  }
}

void ScrollTree::PushScrollUpdatesFromMainThread(
    const PropertyTrees& main_property_trees,
    LayerTreeImpl* sync_tree,
    bool use_fractional_deltas) {
  DCHECK(!property_trees()->is_main_thread());
  const ScrollOffsetMap& main_scroll_offset_map =
      main_property_trees.scroll_tree().scroll_offset_map_;

  // We first want to clear SyncedProperty instances for layers which were
  // destroyed or became non-scrollable on the main thread.
  for (auto map_entry = synced_scroll_offset_map_.begin();
       map_entry != synced_scroll_offset_map_.end();) {
    ElementId id = map_entry->first;
    if (main_scroll_offset_map.find(id) == main_scroll_offset_map.end()) {
      // This SyncedScrollOffset might still be used to send a delta from the
      // active tree to the main thread, so we need to clear out the delta that
      // was sent to the main thread for this commit.
      map_entry->second->PushMainToPending(map_entry->second->Current(true));
      map_entry = synced_scroll_offset_map_.erase(map_entry);
    } else {
      map_entry++;
    }
  }

  for (auto map_entry : main_scroll_offset_map) {
    ElementId id = map_entry.first;
    // In non-test code, this should be the only code path that creates a new
    // SyncedScrollOffset.
    SyncedScrollOffset* synced_scroll_offset = GetSyncedScrollOffset(id);
    if (!synced_scroll_offset) {
      synced_scroll_offset = new SyncedScrollOffset();
      synced_scroll_offset_map_[id] = synced_scroll_offset;
    }

    // If the value on the main thread differs from the value on the pending
    // tree after state sync, we need to update the scroll state on the newly
    // committed PropertyTrees.
    bool needs_scroll_update =
        synced_scroll_offset->PushMainToPending(map_entry.second);
    // If `use_fractional_deltas` is false, then check against the rounded
    // pending offset instead of the offset directly. This matches
    // PullDeltaForMainThread where only an integer delta is extracted and
    // prevents unnecessary property change in this case.
    if (!use_fractional_deltas) {
      gfx::PointF pending_offset = synced_scroll_offset->Current(false);
      gfx::PointF rounded_offset(gfx::ToRoundedPoint(pending_offset));
      needs_scroll_update = map_entry.second != rounded_offset;
    }

    // If we are committing directly to the active tree, push pending to active
    // here. If the value differs between the pending and active trees, we need
    // to update the scroll state on the newly activated PropertyTrees.
    // In the case of pushing to the active tree, even if the pending and active
    // tree state match but the value on the active tree changed, we need to
    // update the scrollbar geometries.
    if (property_trees()->is_active())
      needs_scroll_update |= synced_scroll_offset->PushPendingToActive();

    if (needs_scroll_update) {
      sync_tree->DidUpdateScrollOffset(
          id, /*pushed_from_main_or_pending_tree=*/true);
    }
  }
}

void ScrollTree::PushScrollUpdatesFromPendingTree(
    PropertyTrees* pending_property_trees,
    LayerTreeImpl* active_tree) {
  DCHECK(property_trees()->is_active());
  DCHECK(!pending_property_trees->is_main_thread());
  DCHECK(!pending_property_trees->is_active());

  // When pushing to the active tree, we can simply copy over the map from the
  // pending tree. The pending and active tree hold a reference to the same
  // SyncedProperty instances.
  synced_scroll_offset_map_.clear();
  for (auto map_entry :
       pending_property_trees->scroll_tree().synced_scroll_offset_map_) {
    synced_scroll_offset_map_[map_entry.first] = map_entry.second;
    if (map_entry.second->PushPendingToActive()) {
      active_tree->DidUpdateScrollOffset(
          map_entry.first, /*pushed_from_main_or_pending_tree=*/true);
    }
  }
}

void ScrollTree::ApplySentScrollDeltasFromAbortedCommit(
    bool next_bmf,
    bool main_frame_applied_deltas) {
  DCHECK(property_trees()->is_active());
  for (auto& map_entry : synced_scroll_offset_map_)
    map_entry.second->AbortCommit(next_bmf, main_frame_applied_deltas);
}

void ScrollTree::SetBaseScrollOffset(ElementId id,
                                     const gfx::PointF& scroll_offset) {
  if (property_trees()->is_main_thread()) {
    scroll_offset_map_[id] = scroll_offset;
    return;
  }

  DCHECK(GetSyncedScrollOffset(id));
  GetSyncedScrollOffset(id)->PushMainToPending(scroll_offset);
}

bool ScrollTree::SetScrollOffset(ElementId id,
                                 const gfx::PointF& scroll_offset) {
  // TODO(crbug.com/40132829): Remove TRACE_EVENT call when the bug is fixed
  TRACE_EVENT2("cc", "ScrollTree::SetScrollOffset", "x", scroll_offset.x(), "y",
               scroll_offset.y());
  if (property_trees()->is_main_thread()) {
    if (scroll_offset_map_[id] == scroll_offset)
      return false;
    scroll_offset_map_[id] = scroll_offset;
    return true;
  }

  if (property_trees()->is_active()) {
    DCHECK(GetSyncedScrollOffset(id));
    return GetSyncedScrollOffset(id)->SetCurrent(scroll_offset);
  }

  return false;
}

void ScrollTree::SetScrollingContentsCullRect(ElementId id,
                                              const gfx::Rect& cull_rect) {
  scrolling_contents_cull_rects_[id] = cull_rect;
}

const gfx::Rect* ScrollTree::ScrollingContentsCullRect(ElementId id) const {
  auto it = scrolling_contents_cull_rects_.find(id);
  if (it == scrolling_contents_cull_rects_.end()) {
    return nullptr;
  }
  return &it->second;
}

SyncedScrollOffset* ScrollTree::GetOrCreateSyncedScrollOffsetForTesting(
    ElementId id) {
  auto it = synced_scroll_offset_map_.find(id);
  if (it == synced_scroll_offset_map_.end()) {
    it = synced_scroll_offset_map_.try_emplace(id, new SyncedScrollOffset())
             .first;
  }
  return it->second.get();
}

bool ScrollTree::UpdateScrollOffsetBaseForTesting(ElementId id,
                                                  const gfx::PointF& offset) {
  DCHECK(!property_trees()->is_main_thread());
  SyncedScrollOffset* synced_scroll_offset =
      GetOrCreateSyncedScrollOffsetForTesting(id);  // IN-TEST
  bool changed = synced_scroll_offset->PushMainToPending(offset);
  if (property_trees()->is_active())
    changed |= synced_scroll_offset->PushPendingToActive();
  return changed;
}

bool ScrollTree::SetScrollOffsetDeltaForTesting(ElementId id,
                                                const gfx::Vector2dF& delta) {
  auto* synced_scroll_offset =
      GetOrCreateSyncedScrollOffsetForTesting(id);  // IN-TEST
  return synced_scroll_offset->SetCurrent(synced_scroll_offset->ActiveBase() +
                                          delta);
}

const gfx::PointF ScrollTree::GetScrollOffsetBaseForTesting(
    ElementId id) const {
  DCHECK(!property_trees()->is_main_thread());
  if (GetSyncedScrollOffset(id)) {
    return property_trees()->is_active()
               ? GetSyncedScrollOffset(id)->ActiveBase()
               : GetSyncedScrollOffset(id)->PendingBase();
  }
  return gfx::PointF();
}

const gfx::Vector2dF ScrollTree::GetScrollOffsetDeltaForTesting(
    ElementId id) const {
  DCHECK(!property_trees()->is_main_thread());
  if (GetSyncedScrollOffset(id)) {
    return property_trees()->is_active()
               ? GetSyncedScrollOffset(id)->Delta()
               : GetSyncedScrollOffset(id)->PendingDelta();
  }
  return gfx::Vector2dF();
}

gfx::Vector2dF ScrollTree::ScrollBy(const ScrollNode& scroll_node,
                                    const gfx::Vector2dF& scroll,
                                    LayerTreeImpl* layer_tree_impl) {
  TRACE_EVENT_BEGIN("input", "ScrollTree::ScrollBy", "scroll", scroll,
                    "scroll_node_id", scroll_node.id);
  gfx::Vector2dF adjusted_scroll(scroll);
  if (!scroll_node.user_scrollable_horizontal)
    adjusted_scroll.set_x(0);
  if (!scroll_node.user_scrollable_vertical)
    adjusted_scroll.set_y(0);
  gfx::PointF old_offset = current_scroll_offset(scroll_node.element_id);
  gfx::PointF new_offset =
      ClampScrollOffsetToLimits(old_offset + adjusted_scroll, scroll_node);
  if (SetScrollOffset(scroll_node.element_id, new_offset)) {
    layer_tree_impl->DidUpdateScrollOffset(
        scroll_node.element_id,
        /*pushed_from_main_or_pending_tree=*/false);
  }

  TRACE_EVENT_END("input", /* ScrollTree::ScrollBy */
                  "old_offset", old_offset, "new_offset", new_offset);

  // Return the amount of scroll delta we could not consume for this node.
  return old_offset + scroll - new_offset;
}

gfx::PointF ScrollTree::ClampScrollOffsetToLimits(
    gfx::PointF offset,
    const ScrollNode& scroll_node) const {
  offset.SetToMin(MaxScrollOffset(scroll_node.id));
  offset.SetToMax(gfx::PointF());
  return offset;
}

void ScrollTree::SetScrollCallbacks(base::WeakPtr<ScrollCallbacks> callbacks) {
  DCHECK(property_trees()->is_main_thread());
  callbacks_ = std::move(callbacks);
}

void ScrollTree::NotifyDidCompositorScroll(
    ElementId scroll_element_id,
    const gfx::PointF& scroll_offset,
    const std::optional<TargetSnapAreaElementIds>& snap_target_ids) {
  DCHECK(property_trees()->is_main_thread());
  if (callbacks_) {
    callbacks_->DidCompositorScroll(scroll_element_id, scroll_offset,
                                    snap_target_ids);
  }
}

void ScrollTree::NotifyDidChangeScrollbarsHidden(ElementId scroll_element_id,
                                                 bool hidden) const {
  DCHECK(property_trees()->is_main_thread());
  if (callbacks_)
    callbacks_->DidChangeScrollbarsHidden(scroll_element_id, hidden);
}

PropertyTreesCachedData::PropertyTreesCachedData()
    : transform_tree_update_number(0) {
  animation_scales.clear();
}

PropertyTreesCachedData::~PropertyTreesCachedData() = default;

PropertyTreesChangeState::PropertyTreesChangeState() = default;
PropertyTreesChangeState::~PropertyTreesChangeState() = default;
PropertyTreesChangeState::PropertyTreesChangeState(PropertyTreesChangeState&&) =
    default;
PropertyTreesChangeState& PropertyTreesChangeState::operator=(
    PropertyTreesChangeState&&) = default;

PropertyTrees::PropertyTrees(const ProtectedSequenceSynchronizer& synchronizer)
    : synchronizer_(synchronizer),
      transform_tree_(this),
      effect_tree_(this),
      clip_tree_(this),
      scroll_tree_(this),
      needs_rebuild_(true),
      changed_(false),
      full_tree_damaged_(false),
      is_main_thread_(true),
      is_active_(false),
      sequence_number_(0) {}

PropertyTrees::~PropertyTrees() = default;

#if DCHECK_IS_ON()
bool PropertyTrees::operator==(const PropertyTrees& other) const {
  return transform_tree() == other.transform_tree() &&
         effect_tree() == other.effect_tree() &&
         clip_tree() == other.clip_tree() &&
         scroll_tree() == other.scroll_tree() &&
         needs_rebuild() == other.needs_rebuild() &&
         changed() == other.changed() &&
         full_tree_damaged() == other.full_tree_damaged() &&
         is_main_thread() == other.is_main_thread() &&
         is_active() == other.is_active() &&
         sequence_number() == other.sequence_number();
}
#endif

PropertyTrees& PropertyTrees::operator=(const PropertyTrees& from) {
  transform_tree_mutable() = from.transform_tree();
  effect_tree_mutable() = from.effect_tree();
  clip_tree_mutable() = from.clip_tree();
  scroll_tree_mutable() = from.scroll_tree();
  set_needs_rebuild(from.needs_rebuild());
  set_changed(from.changed());
  set_full_tree_damaged(from.full_tree_damaged());
  set_sequence_number(from.sequence_number());
  set_is_main_thread(from.is_main_thread());
  set_is_active(from.is_active());
  SetInnerViewportContainerBoundsDelta(
      from.inner_viewport_container_bounds_delta());
  SetOuterViewportContainerBoundsDelta(
      from.outer_viewport_container_bounds_delta());
  transform_tree_mutable().SetPropertyTrees(this);
  effect_tree_mutable().SetPropertyTrees(this);
  clip_tree_mutable().SetPropertyTrees(this);
  scroll_tree_mutable().SetPropertyTrees(this);
  ResetCachedData();
  return *this;
}

void PropertyTrees::clear() {
  transform_tree_mutable().clear();
  clip_tree_mutable().clear();
  effect_tree_mutable().clear();
  scroll_tree_mutable().clear();

  set_needs_rebuild(true);
  set_full_tree_damaged(false);
  set_changed(false);
  increment_sequence_number();

#if DCHECK_IS_ON()
  PropertyTrees tree(synchronizer());
  tree.transform_tree_mutable() = transform_tree();
  tree.effect_tree_mutable() = effect_tree();
  tree.clip_tree_mutable() = clip_tree();
  tree.scroll_tree_mutable() = scroll_tree();
  tree.scroll_tree_mutable().CopyCompleteTreeState(scroll_tree());

  tree.set_sequence_number(sequence_number());
  tree.set_is_main_thread(is_main_thread());
  tree.set_is_active(is_active());
  DCHECK(tree == *this);
#endif
}

void PropertyTrees::SetInnerViewportContainerBoundsDelta(
    gfx::Vector2dF bounds_delta) {
  if (inner_viewport_container_bounds_delta() == bounds_delta)
    return;

  inner_viewport_container_bounds_delta_.Write(synchronizer()) = bounds_delta;
}

void PropertyTrees::SetOuterViewportContainerBoundsDelta(
    gfx::Vector2dF bounds_delta) {
  if (outer_viewport_container_bounds_delta() == bounds_delta)
    return;

  outer_viewport_container_bounds_delta_.Write(synchronizer()) = bounds_delta;
  transform_tree_mutable().UpdateOuterViewportContainerBoundsDelta();
}

bool PropertyTrees::ElementIsAnimatingChanged(
    const PropertyToElementIdMap& element_id_map,
    const PropertyAnimationState& mask,
    const PropertyAnimationState& state,
    bool check_node_existence) {
  bool updated_transform = false;
  for (int property = TargetProperty::FIRST_TARGET_PROPERTY;
       property <= TargetProperty::LAST_TARGET_PROPERTY; ++property) {
    if (!mask.currently_running[property] &&
        !mask.potentially_animating[property])
      continue;

    // The mask represents which properties have had their state changed. This
    // can include properties for which there are no longer any animations, in
    // which case there will not be an entry in the map.
    //
    // It is unclear whether this is desirable; it may be that we are missing
    // updates to property nodes here because we no longer have the required
    // ElementId to look them up. See http://crbug.com/912574 for context around
    // why this code was rewritten.
    auto it = element_id_map.find(static_cast<TargetProperty::Type>(property));
    if (it == element_id_map.end())
      continue;

    const ElementId element_id = it->second;
    switch (property) {
      case TargetProperty::TRANSFORM:
      case TargetProperty::SCALE:
      case TargetProperty::ROTATE:
      case TargetProperty::TRANSLATE:
        if (TransformNode* transform_node =
                transform_tree_mutable().FindNodeFromElementId(element_id)) {
          if (mask.currently_running[property])
            transform_node->is_currently_animating =
                state.currently_running[property];
          if (mask.potentially_animating[property]) {
            transform_node->has_potential_animation =
                state.potentially_animating[property];
            transform_tree_mutable().set_needs_update(true);
            // We track transform updates specifically, whereas we
            // don't do so for opacity/filter, because whether a
            // transform is animating can change what layer(s) we
            // draw.
            updated_transform = true;
          }
        } else {
          DCHECK_NODE_EXISTENCE(check_node_existence, state, property,
                                needs_rebuild())
              << "Attempting to animate non existent transform node";
        }
        break;
      case TargetProperty::OPACITY:
        if (EffectNode* effect_node =
                effect_tree_mutable().FindNodeFromElementId(element_id)) {
          if (mask.currently_running[property])
            effect_node->is_currently_animating_opacity =
                state.currently_running[property];
          if (mask.potentially_animating[property]) {
            effect_node->has_potential_opacity_animation =
                state.potentially_animating[property];
            // We may need to propagate things like screen space opacity.
            effect_tree_mutable().set_needs_update(true);
          }
        } else {
          DCHECK_NODE_EXISTENCE(check_node_existence, state, property,
                                needs_rebuild())
              << "Attempting to animate opacity on non existent effect node";
        }
        break;
      case TargetProperty::FILTER:
        if (EffectNode* effect_node =
                effect_tree_mutable().FindNodeFromElementId(element_id)) {
          if (mask.currently_running[property])
            effect_node->is_currently_animating_filter =
                state.currently_running[property];
          if (mask.potentially_animating[property])
            effect_node->has_potential_filter_animation =
                state.potentially_animating[property];
          // Filter animation changes only the node, and the subtree does not
          // care, thus there is no need to request property tree update.
        } else {
          DCHECK_NODE_EXISTENCE(check_node_existence, state, property,
                                needs_rebuild())
              << "Attempting to animate filter on non existent effect node";
        }
        break;
      case TargetProperty::BACKDROP_FILTER:
        if (EffectNode* effect_node =
                effect_tree_mutable().FindNodeFromElementId(element_id)) {
          if (mask.currently_running[property])
            effect_node->is_currently_animating_backdrop_filter =
                state.currently_running[property];
          if (mask.potentially_animating[property])
            effect_node->has_potential_backdrop_filter_animation =
                state.potentially_animating[property];
          // Backdrop-filter animation changes only the node, and the subtree
          // does not care, thus there is no need to request property tree
          // update.
        } else {
          DCHECK_NODE_EXISTENCE(check_node_existence, state, property,
                                needs_rebuild())
              << "Attempting to animate filter on non existent effect node";
        }
        break;
      default:
        break;
    }
  }
  return updated_transform;
}

void PropertyTrees::MaximumAnimationScaleChanged(ElementId element_id,
                                                 float maximum_scale) {
  if (TransformNode* transform_node =
          transform_tree_mutable().FindNodeFromElementId(element_id)) {
    transform_node->maximum_animation_scale = maximum_scale;
    UpdateTransformTreeUpdateNumber();
  }
}

void PropertyTrees::UpdateChangeTracking() {
  auto& mutable_effect_tree = effect_tree_mutable();
  for (int id = kContentsRootPropertyNodeId;
       id < static_cast<int>(mutable_effect_tree.size()); ++id) {
    EffectNode* node = mutable_effect_tree.Node(id);
    EffectNode* parent_node = mutable_effect_tree.parent(node);
    mutable_effect_tree.UpdateEffectChanged(node, parent_node);
  }
  auto& mutable_transform_tree = transform_tree_mutable();
  for (int i = kContentsRootPropertyNodeId;
       i < static_cast<int>(mutable_transform_tree.size()); ++i) {
    TransformNode* node = mutable_transform_tree.Node(i);
    TransformNode* parent_node = mutable_transform_tree.parent(node);
    mutable_transform_tree.UpdateTransformChanged(node, parent_node);
  }
}

void PropertyTrees::GetChangedNodes(std::vector<int>& effect_nodes,
                                    std::vector<int>& transform_nodes) const {
  for (int id = kContentsRootPropertyNodeId;
       id < static_cast<int>(effect_tree().size()); ++id) {
    if (effect_tree().Node(id)->effect_changed)
      effect_nodes.push_back(id);
  }
  for (int id = kContentsRootPropertyNodeId;
       id < static_cast<int>(transform_tree().size()); ++id) {
    if (transform_tree().Node(id)->transform_changed)
      transform_nodes.push_back(id);
  }
}

void PropertyTrees::ApplyChangedNodes(
    const std::vector<int>& changed_effect_nodes,
    const std::vector<int>& changed_transform_nodes) {
  if (changed_effect_nodes.size() || changed_transform_nodes.size()) {
    for (int i : changed_effect_nodes)
      effect_tree_mutable().Node(i)->effect_changed = true;
    for (int i : changed_transform_nodes)
      transform_tree_mutable().Node(i)->transform_changed = true;
    UpdateChangeTracking();
  }
}

void PropertyTrees::GetChangeState(PropertyTreesChangeState& change_state) {
  change_state.changed = changed();
  change_state.needs_rebuild = needs_rebuild();
  change_state.full_tree_damaged = full_tree_damaged();
  // Note that EffectTree::TakeCopyRequest() can flip the value of
  // needs_rebuild(), but the prior value is the one we need to propagate, so we
  // snapshot that first.
  change_state.effect_tree_copy_requests =
      effect_tree_mutable().TakeCopyRequests();
  GetChangedNodes(change_state.changed_effect_nodes,
                  change_state.changed_transform_nodes);
  effect_tree().GetRenderSurfaceChangedFlags(
      change_state.surface_property_changed_flags);
}

void PropertyTrees::ResetAllChangeTracking() {
  transform_tree_mutable().ResetChangeTracking();
  effect_tree_mutable().ResetChangeTracking();
  set_changed(false);
  set_full_tree_damaged(false);
}

std::unique_ptr<base::trace_event::TracedValue> PropertyTrees::AsTracedValue()
    const {
  auto value = base::WrapUnique(new base::trace_event::TracedValue);
  AsValueInto(value.get());
  return value;
}

void PropertyTrees::AsValueInto(base::trace_event::TracedValue* value) const {
  value->SetInteger("sequence_number", sequence_number());

  value->BeginDictionary("transform_tree");
  transform_tree().AsValueInto(value);
  value->EndDictionary();

  value->BeginDictionary("effect_tree");
  effect_tree().AsValueInto(value);
  value->EndDictionary();

  value->BeginDictionary("clip_tree");
  clip_tree().AsValueInto(value);
  value->EndDictionary();

  value->BeginDictionary("scroll_tree");
  scroll_tree().AsValueInto(value);
  value->EndDictionary();
}

std::string PropertyTrees::ToString() const {
  base::trace_event::TracedValueJSON value;
  AsValueInto(&value);
  return value.ToFormattedJSON();
}

bool PropertyTrees::AnimationScaleCacheIsInvalid(int transform_id) const {
  DCHECK(!is_main_thread());
  // This doesn't check if |update_number| equals to
  // |transform_tree_update_number| because the the latter is changed by the
  // animation itself while we want to treat the scale as valid during the
  // animation. |update_number| is reset to kInvalidUpdateNumber when a new
  // property tree is pushed.
  return cached_data_.animation_scales[transform_id].update_number ==
         kInvalidUpdateNumber;
}

float PropertyTrees::MaximumAnimationToScreenScale(int transform_id) {
  return GetAnimationScaleData(transform_id).maximum_to_screen_scale;
}

bool PropertyTrees::AnimationAffectedByInvalidScale(int transform_id) {
  return GetAnimationScaleData(transform_id).affected_by_invalid_scale;
}

const AnimationScaleData& PropertyTrees::GetAnimationScaleData(
    int transform_id) {
  DCHECK(!is_main_thread());

  auto& animation_scale = cached_data_.animation_scales[transform_id];
  if (animation_scale.update_number ==
      cached_data_.transform_tree_update_number) {
    return animation_scale;
  }

  animation_scale.update_number = cached_data_.transform_tree_update_number;

  TransformNode* node = transform_tree_mutable().Node(transform_id);
  TransformNode* parent_node = transform_tree_mutable().parent(node);
  const auto* parent_animation_scale =
      parent_node ? &GetAnimationScaleData(parent_node->id) : nullptr;

  bool ancestor_affected_by_animation_scale =
      parent_node && parent_animation_scale->affected_by_animation_scale;
  bool node_affected_by_animation_scale =
      node->has_potential_animation && node->maximum_animation_scale != 1.0f;

  animation_scale.affected_by_animation_scale =
      node_affected_by_animation_scale || ancestor_affected_by_animation_scale;
  animation_scale.affected_by_invalid_scale =
      (parent_node && parent_animation_scale->affected_by_invalid_scale) ||
      // Computing maximum animated scale in the presence of perspective isn't
      // supported.
      node->to_parent.HasPerspective() ||
      (node->has_potential_animation &&
       node->maximum_animation_scale == kInvalidScale);

  // We don't attempt to accumulate animation scale from multiple nodes with
  // scale animations, because of the risk of significant overestimation. For
  // example, one node might be increasing scale from 1 to 10 at the same time
  // as another node is decreasing scale from 10 to 1. Naively combining these
  // scales would produce a scale of 100.
  bool failed_for_multiple_scale_animations =
      ancestor_affected_by_animation_scale && node_affected_by_animation_scale;

  float local_maximum_scale = 1.0f;
  if (animation_scale.affected_by_invalid_scale ||
      failed_for_multiple_scale_animations) {
    // Will use the parent's maximum_to_screen_scale.
  } else if (!node->to_screen_is_potentially_animated) {
    // No transform animations. Calculate the current to_screen scale.
    gfx::Vector2dF to_screen_scales = gfx::ComputeTransform2dScaleComponents(
        transform_tree().ToScreen(transform_id), kInvalidScale);
    animation_scale.maximum_to_screen_scale =
        std::max(to_screen_scales.x(), to_screen_scales.y());
    return animation_scale;
  } else if (!node->has_potential_animation) {
    gfx::Vector2dF local_scales =
        gfx::ComputeTransform2dScaleComponents(node->local, 1.0f);
    local_maximum_scale = std::max(local_scales.x(), local_scales.y());
  } else {
    DCHECK_NE(node->maximum_animation_scale, kInvalidScale);
    local_maximum_scale = node->maximum_animation_scale;
  }

  animation_scale.maximum_to_screen_scale = local_maximum_scale;
  if (parent_node) {
    animation_scale.maximum_to_screen_scale *=
        parent_animation_scale->maximum_to_screen_scale;
  }

  return animation_scale;
}

void PropertyTrees::SetMaximumAnimationToScreenScaleForTesting(
    int transform_id,
    float maximum_scale,
    bool affected_by_invalid_scale) {
  auto& animation_scale = cached_data_.animation_scales[transform_id];
  animation_scale.maximum_to_screen_scale = maximum_scale;
  animation_scale.affected_by_invalid_scale = affected_by_invalid_scale;
  animation_scale.update_number = cached_data_.transform_tree_update_number;
}

bool PropertyTrees::GetToTarget(int transform_id,
                                int effect_id,
                                gfx::Transform* to_target) const {
  if (effect_id == kContentsRootPropertyNodeId) {
    *to_target = transform_tree().ToScreen(transform_id);
    return true;
  }
  DrawTransforms& transforms = GetDrawTransforms(transform_id, effect_id);
  if (transforms.to_valid) {
    *to_target = transforms.to_target;
    return true;
  } else if (!transforms.might_be_invertible) {
    return false;
  } else {
    transforms.might_be_invertible =
        transforms.from_target.GetInverse(to_target);
    transforms.to_valid = transforms.might_be_invertible;
    transforms.to_target = *to_target;
    return transforms.to_valid;
  }
}

bool PropertyTrees::GetFromTarget(int transform_id,
                                  int effect_id,
                                  gfx::Transform* from_target) const {
  const TransformNode* node = transform_tree().Node(transform_id);
  if (node->ancestors_are_invertible &&
      effect_id == kContentsRootPropertyNodeId) {
    *from_target = transform_tree().FromScreen(transform_id);
    return true;
  }
  DrawTransforms& transforms = GetDrawTransforms(transform_id, effect_id);
  if (transforms.from_valid) {
    *from_target = transforms.from_target;
    return true;
  } else if (!transforms.might_be_invertible) {
    return false;
  } else {
    transforms.might_be_invertible =
        transforms.to_target.GetInverse(from_target);
    transforms.from_valid = transforms.might_be_invertible;
    transforms.from_target = *from_target;
    return transforms.from_valid;
  }
}

DrawTransformData& PropertyTrees::FetchDrawTransformsDataFromCache(
    int transform_id,
    int dest_id) const {
  for (auto& transform_data : cached_data_.draw_transforms[transform_id]) {
    // We initialize draw_transforms with 1 element vectors when
    // ResetCachedData, so if we hit an invalid target id, it means it's the
    // first time we compute draw transforms after reset.
    if (transform_data.target_id == dest_id ||
        transform_data.target_id == kInvalidPropertyNodeId) {
      return transform_data;
    }
  }
  // Add an entry to the cache.
  cached_data_.draw_transforms[transform_id].push_back(DrawTransformData());
  DrawTransformData& data = cached_data_.draw_transforms[transform_id].back();
  data.update_number = kInvalidUpdateNumber;
  data.target_id = dest_id;
  return data;
}

ClipRectData* PropertyTrees::FetchClipRectFromCache(int clip_id,
                                                    int target_id) {
  ClipNode* clip_node = clip_tree_mutable().Node(clip_id);
  for (size_t i = 0; i < clip_node->cached_clip_rects.size(); ++i) {
    auto& data = clip_node->cached_clip_rects[i];
    if (data.target_id == target_id || data.target_id == kInvalidPropertyNodeId)
      return &data;
  }
  clip_node->cached_clip_rects.emplace_back();
  clip_node->cached_clip_rects.back().target_id = kInvalidPropertyNodeId;
  return &clip_node->cached_clip_rects.back();
}

bool PropertyTrees::HasElement(ElementId element_id) const {
  if (!element_id)
    return false;
  return clip_tree().FindNodeFromElementId(element_id) ||
         effect_tree().FindNodeFromElementId(element_id) ||
         scroll_tree().FindNodeFromElementId(element_id) ||
         transform_tree().FindNodeFromElementId(element_id);
}

DrawTransforms& PropertyTrees::GetDrawTransforms(int transform_id,
                                                 int effect_id) const {
  const EffectNode* effect_node = effect_tree().Node(effect_id);
  int dest_id = effect_node->transform_id;

  DrawTransformData& data =
      FetchDrawTransformsDataFromCache(transform_id, dest_id);

  DCHECK(data.update_number != cached_data_.transform_tree_update_number ||
         data.target_id != kInvalidPropertyNodeId);
  if (data.update_number == cached_data_.transform_tree_update_number)
    return data.transforms;

  // Cache miss.
  gfx::Transform target_space_transform;
  gfx::Transform from_target;
  bool already_computed_inverse = false;
  if (transform_id == dest_id) {
    target_space_transform.Scale(effect_node->surface_contents_scale.x(),
                                 effect_node->surface_contents_scale.y());
    data.transforms.to_valid = true;
    data.transforms.from_valid = false;
  } else if (transform_id > dest_id) {
    transform_tree().CombineTransformsBetween(transform_id, dest_id,
                                              &target_space_transform);
    target_space_transform.PostScale(effect_node->surface_contents_scale.x(),
                                     effect_node->surface_contents_scale.y());
    data.transforms.to_valid = true;
    data.transforms.from_valid = false;
    data.transforms.might_be_invertible = true;
  } else {
    gfx::Transform combined_transform;
    transform_tree().CombineTransformsBetween(dest_id, transform_id,
                                              &combined_transform);
    if (effect_node->surface_contents_scale.x() != 0.f &&
        effect_node->surface_contents_scale.y() != 0.f)
      combined_transform.Scale(1.0f / effect_node->surface_contents_scale.x(),
                               1.0f / effect_node->surface_contents_scale.y());
    bool invertible = combined_transform.GetInverse(&target_space_transform);
    data.transforms.might_be_invertible = invertible;
    data.transforms.to_valid = invertible;
    data.transforms.from_valid = true;
    from_target = combined_transform;
    already_computed_inverse = true;
  }

  if (!already_computed_inverse)
    data.transforms.to_valid = true;
  data.update_number = cached_data_.transform_tree_update_number;
  data.target_id = dest_id;
  data.transforms.from_target = from_target;
  data.transforms.to_target = target_space_transform;
  return data.transforms;
}

void PropertyTrees::ResetCachedData() {
  cached_data_.transform_tree_update_number = 0;
  const auto transform_count = transform_tree().size();
  cached_data_.animation_scales.resize(transform_count);
  for (auto& animation_scale : cached_data_.animation_scales)
    animation_scale.update_number = kInvalidUpdateNumber;

  cached_data_.draw_transforms.resize(transform_count,
                                      std::vector<DrawTransformData>(1));
  for (auto& draw_transforms_for_id : cached_data_.draw_transforms) {
    draw_transforms_for_id.resize(1);
    draw_transforms_for_id[0].update_number = kInvalidUpdateNumber;
    draw_transforms_for_id[0].target_id = kInvalidPropertyNodeId;
  }
}

void PropertyTrees::UpdateTransformTreeUpdateNumber() {
  cached_data_.transform_tree_update_number++;
}

gfx::Transform PropertyTrees::ToScreenSpaceTransformWithoutSurfaceContentsScale(
    int transform_id,
    int effect_id) const {
  if (transform_id == kRootPropertyNodeId) {
    return gfx::Transform();
  }
  gfx::Transform screen_space_transform =
      transform_tree().ToScreen(transform_id);
  const EffectNode* effect_node = effect_tree().Node(effect_id);

  if (effect_node->surface_contents_scale.x() != 0.0 &&
      effect_node->surface_contents_scale.y() != 0.0)
    screen_space_transform.Scale(1.0 / effect_node->surface_contents_scale.x(),
                                 1.0 / effect_node->surface_contents_scale.y());
  return screen_space_transform;
}

}  // namespace cc
