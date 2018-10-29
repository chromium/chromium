// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <set>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/checked_math.h"
#include "base/trace_event/traced_value.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host_common.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/mutator_host.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "ui/gfx/geometry/vector2d_conversions.h"

namespace cc {

template <typename T>
PropertyTree<T>::PropertyTree()
    : needs_update_(false) {
  nodes_.push_back(T());
  back()->id = kRootNodeId;
  back()->parent_id = kInvalidNodeId;
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

TransformTree::TransformTree()
    : source_to_parent_updates_allowed_(true),
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
void PropertyTree<T>::clear() {
  needs_update_ = false;
  nodes_.clear();
  nodes_.push_back(T());
  back()->id = kRootNodeId;
  back()->parent_id = kInvalidNodeId;

#if DCHECK_IS_ON()
  PropertyTree<T> tree;
  DCHECK(tree == *this);
#endif
}

template <typename T>
bool PropertyTree<T>::operator==(const PropertyTree<T>& other) const {
  return nodes_ == other.nodes() && needs_update_ == other.needs_update();
}

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

void TransformTree::clear() {
  PropertyTree<TransformNode>::clear();

  page_scale_factor_ = 1.f;
  device_scale_factor_ = 1.f;
  device_transform_scale_factor_ = 1.f;
  nodes_affected_by_outer_viewport_bounds_delta_.clear();
  cached_data_.clear();
  cached_data_.push_back(TransformCachedNodeData());
  sticky_position_data_.clear();

#if DCHECK_IS_ON()
  TransformTree tree;
  // TODO(jaydasika) : Move tests that expect source_to_parent_updates_allowed
  // to be true on impl thread to main thread and set it to is_main_thread here.
  tree.source_to_parent_updates_allowed_ = source_to_parent_updates_allowed_;
  DCHECK(tree == *this);
#endif
}

void TransformTree::set_needs_update(bool needs_update) {
  if (needs_update && !PropertyTree<TransformNode>::needs_update())
    property_trees()->UpdateTransformTreeUpdateNumber();
  PropertyTree<TransformNode>::set_needs_update(needs_update);
}

bool TransformTree::ComputeTranslation(int source_id,
                                       int dest_id,
                                       gfx::Transform* transform) const {
  transform->MakeIdentity();
  if (source_id == dest_id)
    return true;

  const TransformNode* dest = Node(dest_id);
  if (!dest->ancestors_are_invertible)
    return false;
  if (source_id != kInvalidNodeId)
    transform->ConcatTransform(ToScreen(source_id));
  if (dest_id != kInvalidNodeId) {
    if (dest->local.IsFlat() && (dest->node_and_ancestors_are_flat ||
                                 dest->flattens_inherited_transform)) {
      // In this case, flattenning will not affect the result, so we can use the
      // FromScreen transform of the dest node.
      transform->ConcatTransform(FromScreen(dest_id));
    } else {
      // In this case, some node between source and destination flattens
      // inherited transform. Consider the tree R->A->B->C->D, where D is the
      // source, A is the destination and C flattens inherited transform. The
      // expected result is D * C * flattened(B). D's ToScreen will be D * C *
      // flattened(B * A * R), but as the source to destination transform is
      // at most translation, C and B cannot be non-flat and so flattened(B * A
      // * R) = B * flattened(A * R). So, to get the expected result we have to
      // multiply D's ToScreen transform with flattened(A * R)^{-1}, which is
      // the inverse of flattened ToScreen of destination.
      gfx::Transform to_screen = ToScreen(dest_id);
      to_screen.FlattenTo2d();
      gfx::Transform from_screen;
      bool success = to_screen.GetInverse(&from_screen);
      if (!success)
        return false;
      transform->ConcatTransform(from_screen);
    }
  }

  return true;
}

TransformNode* TransformTree::FindNodeFromElementId(ElementId id) {
  auto iterator = property_trees()->element_id_to_transform_node_index.find(id);
  if (iterator == property_trees()->element_id_to_transform_node_index.end())
    return nullptr;

  return Node(iterator->second);
}

bool TransformTree::OnTransformAnimated(ElementId element_id,
                                        const gfx::Transform& transform) {
  TransformNode* node = FindNodeFromElementId(element_id);
  DCHECK(node);
  if (node->local == transform)
    return false;
  node->local = transform;
  node->needs_local_transform_update = true;
  node->transform_changed = true;
  property_trees()->changed = true;
  set_needs_update(true);
  return true;
}

bool TransformTree::NeedsSourceToParentUpdate(TransformNode* node) {
  return (source_to_parent_updates_allowed() &&
          node->parent_id != node->source_node_id);
}

void TransformTree::ResetChangeTracking() {
  for (int id = TransformTree::kContentsRootNodeId;
       id < static_cast<int>(size()); ++id) {
    TransformNode* node = Node(id);
    node->transform_changed = false;
  }
}

void TransformTree::UpdateTransforms(int id) {
  TransformNode* node = Node(id);
  TransformNode* parent_node = parent(node);
  DCHECK(parent_node);
  TransformNode* source_node = Node(node->source_node_id);
  // TODO(flackr): Only dirty when scroll offset changes.
  if (node->sticky_position_constraint_id >= 0 ||
      node->needs_local_transform_update || NeedsSourceToParentUpdate(node)) {
    UpdateLocalTransform(node);
  } else {
    UndoSnapping(node);
  }
  UpdateScreenSpaceTransform(node, parent_node);
  UpdateAnimationProperties(node, parent_node);
  UpdateSnapping(node);
  UpdateNodeAndAncestorsHaveIntegerTranslations(node, parent_node);
  UpdateTransformChanged(node, parent_node, source_node);
  UpdateNodeAndAncestorsAreAnimatedOrInvertible(node, parent_node);
}

bool TransformTree::IsDescendant(int desc_id, int source_id) const {
  while (desc_id != source_id) {
    if (desc_id == kInvalidNodeId)
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
    transform->ConcatTransform(ToScreen(current->id));
    if (dest)
      transform->ConcatTransform(FromScreen(dest->id));
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
    DCHECK(combined_transform.IsApproximatelyIdentityOrTranslation(
        SkDoubleToMScalar(1e-4)));
  }

  size_t source_to_destination_size = source_to_destination.size();
  for (size_t i = 0; i < source_to_destination_size; ++i) {
    size_t index = source_to_destination_size - 1 - i;
    const TransformNode* node = Node(source_to_destination[index]);
    if (node->flattens_inherited_transform)
      combined_transform.FlattenTo2d();
    combined_transform.PreconcatTransform(node->to_parent);
  }

  transform->ConcatTransform(combined_transform);
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
    transform->PreconcatTransform(FromScreen(current->id));
    if (dest)
      transform->PreconcatTransform(ToScreen(dest->id));
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
  transform->PreconcatTransform(source_to_dest);
  return all_are_invertible;
}

// This function should match the offset we set for sticky position layer in
// CompositedLayerMapping::UpdateMainGraphicsLayerGeometry.
gfx::Vector2dF StickyPositionOffset(TransformTree* tree, TransformNode* node) {
  if (node->sticky_position_constraint_id == -1)
    return gfx::Vector2dF();
  StickyPositionNodeData* sticky_data = tree->StickyPositionData(node->id);
  const LayerStickyPositionConstraint& constraint = sticky_data->constraints;
  auto& property_trees = *tree->property_trees();
  ScrollNode* scroll_node =
      property_trees.scroll_tree.Node(sticky_data->scroll_ancestor);
  TransformNode* transform_node =
      property_trees.transform_tree.Node(scroll_node->transform_id);
  const auto& scroll_offset = transform_node->scroll_offset;
  DCHECK(property_trees.scroll_tree.current_scroll_offset(
             scroll_node->element_id) == scroll_offset);
  gfx::PointF scroll_position(scroll_offset.x(), scroll_offset.y());
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
    clip.set_width(clip.width() +
                   property_trees.outer_viewport_container_bounds_delta().x());
    clip.set_height(clip.height() +
                    property_trees.outer_viewport_container_bounds_delta().y());
  }

  gfx::Vector2dF ancestor_sticky_box_offset;
  if (sticky_data->nearest_node_shifting_sticky_box !=
      TransformTree::kInvalidNodeId) {
    ancestor_sticky_box_offset =
        tree->StickyPositionData(sticky_data->nearest_node_shifting_sticky_box)
            ->total_sticky_box_sticky_offset;
  }

  gfx::Vector2dF ancestor_containing_block_offset;
  if (sticky_data->nearest_node_shifting_containing_block !=
      TransformTree::kInvalidNodeId) {
    ancestor_containing_block_offset =
        tree->StickyPositionData(
                sticky_data->nearest_node_shifting_containing_block)
            ->total_containing_block_sticky_offset;
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

  return sticky_offset;
}

void TransformTree::UpdateLocalTransform(TransformNode* node) {
  gfx::Transform transform = node->post_local;
  if (NeedsSourceToParentUpdate(node)) {
    gfx::Transform to_parent;
    ComputeTranslation(node->source_node_id, node->parent_id, &to_parent);
    gfx::Vector2dF unsnapping;
    TransformNode* current;
    TransformNode* parent_node;
    // Since we are calculating the adjustment for fixed position node or a
    // scroll child, we need to unsnap only if the snap was caused by a scroll.
    for (current = Node(node->source_node_id); current->id > node->parent_id;
         current = parent(current)) {
      DCHECK(current->scrolls || current->snap_amount.IsZero());
      if (current->scrolls)
        unsnapping.Subtract(current->snap_amount);
    }
    for (parent_node = Node(node->parent_id);
         parent_node->id > node->source_node_id;
         parent_node = parent(parent_node)) {
      DCHECK(parent_node->scrolls || parent_node->snap_amount.IsZero());
      if (parent_node->scrolls)
        unsnapping.Add(parent_node->snap_amount);
    }
    // If a node NeedsSourceToParentUpdate, the node is either a fixed position
    // node or a scroll child.
    // If the node has a fixed position, the parent of the node is an ancestor
    // of source node, current->id should be equal to node->parent_id.
    // Otherwise, the node's source node is always an ancestor of the node owned
    // by the scroll parent, so parent_node->id should be equal to
    // node->source_node_id.
    DCHECK(current->id == node->parent_id ||
           parent_node->id == node->source_node_id);
    to_parent.Translate(unsnapping.x(), unsnapping.y());
    node->source_to_parent = to_parent.To2dTranslation();
  }

  gfx::Vector2dF fixed_position_adjustment;
  gfx::Vector2dF outer_viewport_bounds_delta =
      property_trees()->outer_viewport_container_bounds_delta();
  if (node->moved_by_outer_viewport_bounds_delta_x)
    fixed_position_adjustment.set_x(outer_viewport_bounds_delta.x());

  if (node->moved_by_outer_viewport_bounds_delta_y)
    fixed_position_adjustment.set_y(outer_viewport_bounds_delta.y());

  transform.Translate(node->source_to_parent.x() - node->scroll_offset.x() +
                          fixed_position_adjustment.x(),
                      node->source_to_parent.y() - node->scroll_offset.y() +
                          fixed_position_adjustment.y());
  transform.Translate(StickyPositionOffset(this, node));
  transform.PreconcatTransform(node->local);
  transform.PreconcatTransform(node->pre_local);

  node->set_to_parent(transform);
  node->needs_local_transform_update = false;
}

void TransformTree::UpdateScreenSpaceTransform(TransformNode* node,
                                               TransformNode* parent_node) {
  DCHECK(parent_node);
  gfx::Transform to_screen_space_transform = ToScreen(parent_node->id);
  if (node->flattens_inherited_transform)
    to_screen_space_transform.FlattenTo2d();
  to_screen_space_transform.PreconcatTransform(node->to_parent);
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
  gfx::Transform rounded = ToScreen(node->id);
  rounded.RoundTranslationComponents();
  gfx::Transform delta = FromScreen(node->id);
  delta *= rounded;

  DCHECK(delta.IsApproximatelyIdentityOrTranslation(SkDoubleToMScalar(1e-4)))
      << delta.ToString();

  gfx::Vector2dF translation = delta.To2dTranslation();

  // Now that we have our delta, we must apply it to each of our combined,
  // to/from matrices.
  SetToScreen(node->id, rounded);
  node->to_parent.Translate(translation.x(), translation.y());
  gfx::Transform from_screen = FromScreen(node->id);
  from_screen.matrix().postTranslate(-translation.x(), -translation.y(), 0);
  SetFromScreen(node->id, from_screen);
  node->snap_amount = translation;
}

void TransformTree::UpdateTransformChanged(TransformNode* node,
                                           TransformNode* parent_node,
                                           TransformNode* source_node) {
  DCHECK(parent_node);
  if (parent_node->transform_changed) {
    node->transform_changed = true;
    return;
  }

  if (source_node && source_node->id != parent_node->id &&
      source_to_parent_updates_allowed_ && source_node->transform_changed)
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

void TransformTree::SetRootTransformsAndScales(
    float device_scale_factor,
    float page_scale_factor_for_root,
    const gfx::Transform& device_transform,
    gfx::PointF root_position) {
  gfx::Vector2dF device_transform_scale_components =
      MathUtil::ComputeTransform2dScaleComponents(device_transform, 1.f);

  // Not handling the rare case of different x and y device scale.
  device_transform_scale_factor_ =
      std::max(device_transform_scale_components.x(),
               device_transform_scale_components.y());

  // If DT is the device transform, DSF is the matrix scaled by (device scale
  // factor * page scale factor for root), RP is the matrix translated by root's
  // position,
  // Let Screen Space Scale(SSS) = scale component of DT*DSF*RP,
  // then the screen space transform of the root transform node is set to SSS
  // and the post local transform of the contents root node is set to
  // SSS^-1*DT*DSF*RP.
  gfx::Transform transform = device_transform;
  transform.Scale(device_scale_factor * page_scale_factor_for_root,
                  device_scale_factor * page_scale_factor_for_root);
  transform.Translate(root_position.x(), root_position.y());
  float fallback_value = device_scale_factor * page_scale_factor_for_root;
  gfx::Vector2dF screen_space_scale =
      MathUtil::ComputeTransform2dScaleComponents(transform, fallback_value);
  DCHECK_NE(screen_space_scale.x(), 0.f);
  DCHECK_NE(screen_space_scale.y(), 0.f);

  gfx::Transform root_to_screen;
  root_to_screen.Scale(screen_space_scale.x(), screen_space_scale.y());
  gfx::Transform root_from_screen;
  bool invertible = root_to_screen.GetInverse(&root_from_screen);
  DCHECK(invertible);
  if (root_to_screen != ToScreen(kRootNodeId)) {
    SetToScreen(kRootNodeId, root_to_screen);
    SetFromScreen(kRootNodeId, root_from_screen);
    set_needs_update(true);
  }

  transform.ConcatTransform(root_from_screen);
  TransformNode* contents_root_node = Node(kContentsRootNodeId);
  if (contents_root_node->post_local != transform) {
    contents_root_node->post_local = transform;
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

bool TransformTree::operator==(const TransformTree& other) const {
  return PropertyTree::operator==(other) &&
         source_to_parent_updates_allowed_ ==
             other.source_to_parent_updates_allowed() &&
         page_scale_factor_ == other.page_scale_factor() &&
         device_scale_factor_ == other.device_scale_factor() &&
         device_transform_scale_factor_ ==
             other.device_transform_scale_factor() &&
         nodes_affected_by_outer_viewport_bounds_delta_ ==
             other.nodes_affected_by_outer_viewport_bounds_delta() &&
         cached_data_ == other.cached_data();
}

StickyPositionNodeData* TransformTree::StickyPositionData(int node_id) {
  TransformNode* node = Node(node_id);
  if (node->sticky_position_constraint_id == -1) {
    node->sticky_position_constraint_id = sticky_position_data_.size();
    sticky_position_data_.push_back(StickyPositionNodeData());
  }
  return &sticky_position_data_[node->sticky_position_constraint_id];
}

EffectTree::EffectTree() {
  render_surfaces_.push_back(nullptr);
}

EffectTree::~EffectTree() = default;

int EffectTree::Insert(const EffectNode& tree_node, int parent_id) {
  int node_id = PropertyTree<EffectNode>::Insert(tree_node, parent_id);
  DCHECK_EQ(node_id, static_cast<int>(render_surfaces_.size()));

  render_surfaces_.push_back(nullptr);
  return node_id;
}

void EffectTree::clear() {
  PropertyTree<EffectNode>::clear();
  mask_layer_ids_.clear();
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

void EffectTree::UpdateIsDrawn(EffectNode* node, EffectNode* parent_node) {
  // Nodes that have screen space opacity 0 are hidden. So they are not drawn.
  // Exceptions:
  // 1) Nodes that contribute to copy requests, whether hidden or not, must be
  //    drawn.
  // 2) Nodes that have a backdrop filter.
  // 3) Nodes with animating screen space opacity on main thread or pending tree
  //    are drawn if their parent is drawn irrespective of their opacity.
  if (node->has_copy_request || node->cache_render_surface)
    node->is_drawn = true;
  else if (EffectiveOpacity(node) == 0.f &&
           (!node->has_potential_opacity_animation ||
            property_trees()->is_active) &&
           node->backdrop_filters.IsEmpty())
    node->is_drawn = false;
  else if (parent_node)
    node->is_drawn = parent_node->is_drawn;
  else
    node->is_drawn = true;
}

void EffectTree::UpdateEffectChanged(EffectNode* node,
                                     EffectNode* parent_node) {
  if (parent_node && parent_node->effect_changed) {
    node->effect_changed = true;
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
  node->hidden_by_backface_visibility =
      property_trees()
          ->transform_tree.cached_data()[node->transform_id]
          .is_showing_backface;
}

void EffectTree::UpdateHasMaskingChild(EffectNode* node,
                                       EffectNode* parent_node) {
  // Reset to false when a node is first met. We'll set the bit later
  // when we actually encounter a masking child.
  node->has_masking_child = false;
  if (node->blend_mode == SkBlendMode::kDstIn)
    parent_node->has_masking_child = true;
}

void EffectTree::UpdateSurfaceContentsScale(EffectNode* effect_node) {
  if (!effect_node->has_render_surface) {
    effect_node->surface_contents_scale = gfx::Vector2dF(1.0f, 1.0f);
    return;
  }

  TransformTree& transform_tree = property_trees()->transform_tree;
  float layer_scale_factor = transform_tree.device_scale_factor() *
                             transform_tree.device_transform_scale_factor();
  TransformNode* transform_node =
      transform_tree.Node(effect_node->transform_id);
  if (transform_node->in_subtree_of_page_scale_layer)
    layer_scale_factor *= transform_tree.page_scale_factor();

  // Note: Copy requests currently expect transform to effect output size.
  bool use_transform_for_contents_scale =
      property_trees()->can_adjust_raster_scales ||
      effect_node->has_copy_request;
  const gfx::Vector2dF old_scale = effect_node->surface_contents_scale;
  effect_node->surface_contents_scale =
      use_transform_for_contents_scale
          ? MathUtil::ComputeTransform2dScaleComponents(
                transform_tree.ToScreen(transform_node->id), layer_scale_factor)
          : gfx::Vector2dF(layer_scale_factor, layer_scale_factor);

  // If surface contents scale changes, draw transforms are no longer valid.
  // Invalidates the draw transform cache and updates the clip for the surface.
  if (old_scale != effect_node->surface_contents_scale) {
    property_trees()->clip_tree.set_needs_update(true);
    property_trees()->UpdateTransformTreeUpdateNumber();
  }
}

EffectNode* EffectTree::FindNodeFromElementId(ElementId id) {
  auto iterator = property_trees()->element_id_to_effect_node_index.find(id);
  if (iterator == property_trees()->element_id_to_effect_node_index.end())
    return nullptr;

  return Node(iterator->second);
}

bool EffectTree::OnOpacityAnimated(ElementId id, float opacity) {
  EffectNode* node = FindNodeFromElementId(id);
  DCHECK(node);
  if (node->opacity == opacity)
    return false;
  node->opacity = opacity;
  node->effect_changed = true;
  property_trees()->changed = true;
  property_trees()->effect_tree.set_needs_update(true);
  return true;
}

bool EffectTree::OnFilterAnimated(ElementId id,
                                  const FilterOperations& filters) {
  EffectNode* node = FindNodeFromElementId(id);
  DCHECK(node);
  if (node->filters == filters)
    return false;
  node->filters = filters;
  node->effect_changed = true;
  property_trees()->changed = true;
  property_trees()->effect_tree.set_needs_update(true);
  return true;
}

void EffectTree::UpdateEffects(int id) {
  EffectNode* node = Node(id);
  EffectNode* parent_node = parent(node);

  UpdateOpacities(node, parent_node);
  UpdateIsDrawn(node, parent_node);
  UpdateEffectChanged(node, parent_node);
  UpdateBackfaceVisibility(node, parent_node);
  UpdateHasMaskingChild(node, parent_node);
  UpdateSurfaceContentsScale(node);
}

void EffectTree::AddCopyRequest(
    int node_id,
    std::unique_ptr<viz::CopyOutputRequest> request) {
  copy_requests_.insert(std::make_pair(node_id, std::move(request)));
}

void EffectTree::PushCopyRequestsTo(EffectTree* other_tree) {
  // If other_tree still has copy requests, this means there was a commit
  // without a draw. This only happens in some edge cases during lost context or
  // visibility changes, so don't try to handle preserving these output
  // requests.
  if (!other_tree->copy_requests_.empty()) {
    // Destroying these copy requests will abort them.
    other_tree->copy_requests_.clear();
  }

  if (copy_requests_.empty())
    return;

  for (auto& request : copy_requests_) {
    other_tree->copy_requests_.insert(
        std::make_pair(request.first, std::move(request.second)));
  }
  copy_requests_.clear();

  // Property trees need to get rebuilt since effect nodes (and render surfaces)
  // that were created only for the copy requests we just pushed are no longer
  // needed.
  if (property_trees()->is_main_thread)
    property_trees()->needs_rebuild = true;
}

void EffectTree::TakeCopyRequestsAndTransformToSurface(
    int node_id,
    std::vector<std::unique_ptr<viz::CopyOutputRequest>>* requests) {
  EffectNode* effect_node = Node(node_id);
  DCHECK(effect_node->has_render_surface);
  DCHECK(effect_node->has_copy_request);

  // The area needs to be transformed from the space of content that draws to
  // the surface to the space of the surface itself.
  int destination_id = effect_node->transform_id;
  int source_id;
  if (effect_node->parent_id != EffectTree::kInvalidNodeId) {
    // For non-root surfaces, transform only by sub-layer scale.
    source_id = destination_id;
  } else {
    // The root surface doesn't have the notion of sub-layer scale, but instead
    // has a similar notion of transforming from the space of the root layer to
    // the space of the screen.
    DCHECK_EQ(kRootNodeId, destination_id);
    source_id = TransformTree::kContentsRootNodeId;
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
    gfx::RectF surface_rect(0, 0, kContentVector.x(), kContentVector.y());
    transform.TransformRect(&surface_rect);

    for (auto it = range.first; it != range.second; ++it) {
      viz::CopyOutputRequest* const request = it->second.get();
      if (request->has_area()) {
        request->set_area(
            MathUtil::MapEnclosingClippedRect(transform, request->area()));
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
        int scale_from_x = gfx::ToRoundedInt(scale_from_x_f);
        int scale_from_y = gfx::ToRoundedInt(scale_from_y_f);
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
    node.closest_ancestor_with_copy_request_id = EffectTree::kInvalidNodeId;
  }

  // Any copy requests that are still left will be aborted (sending an empty
  // result) on destruction.
  copy_requests_.clear();
  set_needs_update(true);
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

void EffectTree::AddMaskLayerId(int id) {
  mask_layer_ids_.push_back(id);
}

void EffectTree::UpdateRenderSurfaces(LayerTreeImpl* layer_tree_impl) {
  for (int id = kContentsRootNodeId; id < static_cast<int>(size()); ++id) {
    EffectNode* effect_node = Node(id);
    bool needs_render_surface =
        id == kContentsRootNodeId || effect_node->has_render_surface;
    if (needs_render_surface == !!render_surfaces_[id])
      continue;

    if (needs_render_surface) {
      render_surfaces_[id] = std::make_unique<RenderSurfaceImpl>(
          layer_tree_impl, effect_node->stable_id);
      render_surfaces_[id]->set_effect_tree_index(id);
    } else {
      render_surfaces_[id].reset();
    }
  }
}

bool EffectTree::ContributesToDrawnSurface(int id) {
  // All drawn nodes contribute to drawn surface.
  // Exception : Nodes that are hidden and are drawn only for the sake of
  // copy requests.
  EffectNode* node = Node(id);
  EffectNode* parent_node = parent(node);
  return node->is_drawn && (!parent_node || parent_node->is_drawn);
}

void EffectTree::ResetChangeTracking() {
  for (int id = EffectTree::kContentsRootNodeId; id < static_cast<int>(size());
       ++id) {
    Node(id)->effect_changed = false;
    if (render_surfaces_[id])
      render_surfaces_[id]->ResetPropertyChangedFlags();
  }
}

void EffectTree::TakeRenderSurfaces(
    std::vector<std::unique_ptr<RenderSurfaceImpl>>* render_surfaces) {
  for (int id = kContentsRootNodeId; id < static_cast<int>(size()); ++id) {
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
  std::vector<std::pair<uint64_t, int>> stable_id_node_id_list;
  for (int id = kContentsRootNodeId; id < static_cast<int>(size()); ++id) {
    EffectNode* node = Node(id);
    if (node->has_render_surface) {
      stable_id_node_id_list.push_back(
          std::make_pair(node->stable_id, node->id));
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

    if ((*surfaces_list_it)->id() > id_list_it->first) {
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
  for (; effect_node->id != kContentsRootNodeId;
       effect_node = Node(effect_node->target_id)) {
    gfx::Transform to_target;
    if (!property_trees()->GetToTarget(effect_node->transform_id,
                                       effect_node->target_id, &to_target) ||
        !to_target.Preserves2dAxisAlignment())
      return false;
    if (effect_node->mask_layer_id != Layer::INVALID_ID)
      return false;
  }
  return true;
}

void TransformTree::UpdateNodeAndAncestorsHaveIntegerTranslations(
    TransformNode* node,
    TransformNode* parent_node) {
  DCHECK(parent_node);
  node->node_and_ancestors_have_only_integer_translation =
      node->to_parent.IsIdentityOrIntegerTranslation() &&
      parent_node->node_and_ancestors_have_only_integer_translation;
}

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
  const unsigned long min_size = 1;
  DCHECK_GT(size(), min_size);
  return Node(kViewportNodeId)->clip;
}

bool ClipTree::operator==(const ClipTree& other) const {
  return PropertyTree::operator==(other);
}

EffectTree& EffectTree::operator=(const EffectTree& from) {
  PropertyTree::operator=(from);
  render_surfaces_.resize(size());
  mask_layer_ids_ = from.mask_layer_ids_;
  // copy_requests_ are omitted here, since these need to be moved rather
  // than copied or assigned.

  return *this;
}

bool EffectTree::operator==(const EffectTree& other) const {
  return PropertyTree::operator==(other) &&
         mask_layer_ids_ == other.mask_layer_ids_;
}

ScrollTree::ScrollTree()
    : currently_scrolling_node_id_(kInvalidNodeId),
      scroll_offset_map_(ScrollTree::ScrollOffsetMap()) {}

ScrollTree::~ScrollTree() = default;

ScrollTree& ScrollTree::operator=(const ScrollTree& from) {
  PropertyTree::operator=(from);
  currently_scrolling_node_id_ = kInvalidNodeId;
  // Maps for ScrollOffsets/SyncedScrollOffsets are intentionally ommitted here
  // since we can not directly copy them. Pushing of these updates from main
  // currently depends on Layer properties for scroll offset animation changes
  // (setting clobber_active_value for scroll offset animations interrupted on
  // the main thread) being pushed to impl first.
  return *this;
}

bool ScrollTree::operator==(const ScrollTree& other) const {
  if (scroll_offset_map_ != other.scroll_offset_map_)
    return false;
  if (synced_scroll_offset_map_ != other.synced_scroll_offset_map_)
    return false;

  bool is_currently_scrolling_node_equal =
      currently_scrolling_node_id_ == other.currently_scrolling_node_id_;

  return PropertyTree::operator==(other) && is_currently_scrolling_node_equal;
}

#if DCHECK_IS_ON()
void ScrollTree::CopyCompleteTreeState(const ScrollTree& other) {
  currently_scrolling_node_id_ = other.currently_scrolling_node_id_;
  scroll_offset_map_ = other.scroll_offset_map_;
  synced_scroll_offset_map_ = other.synced_scroll_offset_map_;
}
#endif

ScrollNode* ScrollTree::FindNodeFromElementId(ElementId id) {
  auto iterator = property_trees()->element_id_to_scroll_node_index.find(id);
  if (iterator == property_trees()->element_id_to_scroll_node_index.end())
    return nullptr;

  return Node(iterator->second);
}

const ScrollNode* ScrollTree::FindNodeFromElementId(ElementId id) const {
  auto iterator = property_trees()->element_id_to_scroll_node_index.find(id);
  if (iterator == property_trees()->element_id_to_scroll_node_index.end())
    return nullptr;

  return Node(iterator->second);
}

void ScrollTree::clear() {
  PropertyTree<ScrollNode>::clear();

  if (property_trees()->is_main_thread) {
    currently_scrolling_node_id_ = kInvalidNodeId;
    scroll_offset_map_.clear();
  }

#if DCHECK_IS_ON()
  ScrollTree tree;
  if (!property_trees()->is_main_thread) {
    DCHECK(scroll_offset_map_.empty());
    tree.currently_scrolling_node_id_ = currently_scrolling_node_id_;
    tree.synced_scroll_offset_map_ = synced_scroll_offset_map_;
  }
  DCHECK(tree == *this);
#endif
}

gfx::ScrollOffset ScrollTree::MaxScrollOffset(int scroll_node_id) const {
  const ScrollNode* scroll_node = Node(scroll_node_id);
  gfx::SizeF scroll_bounds =
      gfx::SizeF(scroll_node->bounds.width(), scroll_node->bounds.height());

  if (scroll_node->scrolls_inner_viewport) {
    scroll_bounds.Enlarge(
        property_trees()->inner_viewport_scroll_bounds_delta().x(),
        property_trees()->inner_viewport_scroll_bounds_delta().y());
  }

  if (!scroll_node->scrollable || scroll_bounds.IsEmpty())
    return gfx::ScrollOffset();

  TransformTree& transform_tree = property_trees()->transform_tree;
  float scale_factor = 1.f;
  if (scroll_node->max_scroll_offset_affected_by_page_scale)
    scale_factor = transform_tree.page_scale_factor();

  gfx::SizeF scaled_scroll_bounds = gfx::ScaleSize(scroll_bounds, scale_factor);
  scaled_scroll_bounds.SetSize(std::floor(scaled_scroll_bounds.width()),
                               std::floor(scaled_scroll_bounds.height()));

  gfx::Size clip_layer_bounds = container_bounds(scroll_node->id);

  gfx::ScrollOffset max_offset(
      scaled_scroll_bounds.width() - clip_layer_bounds.width(),
      scaled_scroll_bounds.height() - clip_layer_bounds.height());

  max_offset.Scale(1 / scale_factor);
  max_offset.SetToMax(gfx::ScrollOffset());
  return max_offset;
}

void ScrollTree::OnScrollOffsetAnimated(ElementId id,
                                        int scroll_tree_index,
                                        const gfx::ScrollOffset& scroll_offset,
                                        LayerTreeImpl* layer_tree_impl) {
  // Only active tree needs to be updated, pending tree will find out about
  // these changes as a result of the shared SyncedProperty.
  if (!property_trees()->is_active)
    return;

  ScrollNode* scroll_node = Node(scroll_tree_index);
  if (SetScrollOffset(id,
                      ClampScrollOffsetToLimits(scroll_offset, *scroll_node)))
    layer_tree_impl->DidUpdateScrollOffset(id);
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
  const ScrollNode* scroll_node = Node(scroll_node_id);
  const TransformTree& transform_tree = property_trees()->transform_tree;
  const TransformNode* transform_node =
      transform_tree.Node(scroll_node->transform_id);
  gfx::Transform screen_space_transform(
      1, 0, 0, 1, scroll_node->offset_to_transform_parent.x(),
      scroll_node->offset_to_transform_parent.y());
  screen_space_transform.ConcatTransform(
      transform_tree.ToScreen(transform_node->id));
  if (scroll_node->should_flatten)
    screen_space_transform.FlattenTo2d();
  return screen_space_transform;
}

SyncedScrollOffset* ScrollTree::GetOrCreateSyncedScrollOffset(ElementId id) {
  DCHECK(!property_trees()->is_main_thread);
  if (synced_scroll_offset_map_.find(id) == synced_scroll_offset_map_.end()) {
    synced_scroll_offset_map_[id] = new SyncedScrollOffset;
  }
  return synced_scroll_offset_map_[id].get();
}

const SyncedScrollOffset* ScrollTree::GetSyncedScrollOffset(
    ElementId id) const {
  DCHECK(!property_trees()->is_main_thread);
  auto it = synced_scroll_offset_map_.find(id);
  return it != synced_scroll_offset_map_.end() ? it->second.get() : nullptr;
}

gfx::Vector2dF ScrollTree::ClampScrollToMaxScrollOffset(
    ScrollNode* node,
    LayerTreeImpl* layer_tree_impl) {
  gfx::ScrollOffset old_offset = current_scroll_offset(node->element_id);
  gfx::ScrollOffset clamped_offset =
      ClampScrollOffsetToLimits(old_offset, *node);
  gfx::Vector2dF delta = clamped_offset.DeltaFrom(old_offset);
  if (!delta.IsZero())
    ScrollBy(node, delta, layer_tree_impl);
  return delta;
}

const gfx::ScrollOffset ScrollTree::current_scroll_offset(ElementId id) const {
  if (property_trees()->is_main_thread) {
    auto it = scroll_offset_map_.find(id);
    return it != scroll_offset_map_.end() ? it->second : gfx::ScrollOffset();
  }
  return GetSyncedScrollOffset(id)
             ? GetSyncedScrollOffset(id)->Current(property_trees()->is_active)
             : gfx::ScrollOffset();
}

const gfx::ScrollOffset ScrollTree::GetPixelSnappedScrollOffset(
    int scroll_node_id) const {
  const ScrollNode* scroll_node = Node(scroll_node_id);
  DCHECK(scroll_node);
  gfx::ScrollOffset offset = current_scroll_offset(scroll_node->element_id);

  const TransformNode* transform_node =
      property_trees()->transform_tree.Node(scroll_node->transform_id);
  DCHECK(offset == transform_node->scroll_offset)
      << "Transform node scroll offset does not match the actual offset, this "
         "means the snapped_amount calculation will be incorrect";

  if (transform_node->scrolls) {
    // If necessary perform a update for this node to ensure snap amount is
    // accurate. This method is used by scroll timeline, so it is possible for
    // it to get called before transform tree has gone through a full update
    // cycle so this node snap amount may be stale.
    if (transform_node->needs_local_transform_update)
      property_trees()->transform_tree.UpdateTransforms(transform_node->id);

    offset.set_x(offset.x() - transform_node->snap_amount.x());
    offset.set_y(offset.y() - transform_node->snap_amount.y());
  }

  return offset;
}

gfx::ScrollOffset ScrollTree::PullDeltaForMainThread(
    SyncedScrollOffset* scroll_offset,
    bool use_fractional_deltas) {
  DCHECK(property_trees()->is_active);

  // Once this setting is enabled, all the complicated rounding logic below can
  // go away.
  if (use_fractional_deltas)
    return scroll_offset->PullDeltaForMainThread();

  // TODO(flackr): We should pass the fractional scroll deltas when Blink fully
  // supports fractional scrolls. crbug.com/414283.
  // TODO(flackr): We should ideally round the fractional scrolls in the same
  // direction as the scroll will be snapped but for common cases this is
  // equivalent to rounding to the nearest integer offset.
  gfx::ScrollOffset current_offset =
      scroll_offset->Current(/* is_active_tree */ true);
  gfx::ScrollOffset rounded_offset =
      gfx::ScrollOffset(roundf(current_offset.x()), roundf(current_offset.y()));
  // The calculation of the difference from the rounded active base is to
  // represent the integer delta that the main thread should know about.
  gfx::ScrollOffset active_base = scroll_offset->ActiveBase();
  gfx::ScrollOffset diff_active_base =
      gfx::ScrollOffset(active_base.x() - roundf(active_base.x()),
                        active_base.y() - roundf(active_base.y()));
  scroll_offset->SetCurrent(rounded_offset + diff_active_base);
  gfx::ScrollOffset delta = scroll_offset->PullDeltaForMainThread();
  scroll_offset->SetCurrent(current_offset);
  return delta;
}

void ScrollTree::CollectScrollDeltas(ScrollAndScaleSet* scroll_info,
                                     ElementId inner_viewport_scroll_element_id,
                                     bool use_fractional_deltas) {
  DCHECK(!property_trees()->is_main_thread);
  for (auto map_entry : synced_scroll_offset_map_) {
    gfx::ScrollOffset scroll_delta =
        PullDeltaForMainThread(map_entry.second.get(), use_fractional_deltas);

    ElementId id = map_entry.first;

    if (!scroll_delta.IsZero()) {
      if (id == inner_viewport_scroll_element_id) {
        // Inner (visual) viewport is stored separately.
        scroll_info->inner_viewport_scroll.element_id = id;
        scroll_info->inner_viewport_scroll.scroll_delta = scroll_delta;
      } else {
        LayerTreeHostCommon::ScrollUpdateInfo scroll;
        scroll.element_id = id;
        scroll.scroll_delta = scroll_delta;
        scroll_info->scrolls.push_back(scroll);
      }
    }
  }
}

void ScrollTree::CollectScrollDeltasForTesting() {
  LayerTreeSettings settings;
  bool use_fractional_deltas = settings.commit_fractional_scroll_deltas;

  for (auto map_entry : synced_scroll_offset_map_) {
    PullDeltaForMainThread(map_entry.second.get(), use_fractional_deltas);
  }
}

void ScrollTree::PushScrollUpdatesFromMainThread(
    PropertyTrees* main_property_trees,
    LayerTreeImpl* sync_tree) {
  DCHECK(!property_trees()->is_main_thread);
  const ScrollOffsetMap& main_scroll_offset_map =
      main_property_trees->scroll_tree.scroll_offset_map_;

  // We first want to clear SyncedProperty instances for layers which were
  // destroyed or became non-scrollable on the main thread.
  for (auto map_entry = synced_scroll_offset_map_.begin();
       map_entry != synced_scroll_offset_map_.end();) {
    ElementId id = map_entry->first;
    if (main_scroll_offset_map.find(id) == main_scroll_offset_map.end())
      map_entry = synced_scroll_offset_map_.erase(map_entry);
    else
      map_entry++;
  }

  for (auto map_entry : main_scroll_offset_map) {
    ElementId id = map_entry.first;
    SyncedScrollOffset* synced_scroll_offset =
        GetOrCreateSyncedScrollOffset(id);

    // If the value on the main thread differs from the value on the pending
    // tree after state sync, we need to update the scroll state on the newly
    // committed PropertyTrees.
    bool needs_scroll_update =
        synced_scroll_offset->PushMainToPending(map_entry.second);

    // If we are committing directly to the active tree, push pending to active
    // here. If the value differs between the pending and active trees, we need
    // to update the scroll state on the newly activated PropertyTrees.
    // In the case of pushing to the active tree, even if the pending and active
    // tree state match but the value on the active tree changed, we need to
    // update the scrollbar geometries.
    if (property_trees()->is_active)
      needs_scroll_update |= synced_scroll_offset->PushPendingToActive();

    if (needs_scroll_update)
      sync_tree->DidUpdateScrollOffset(id);
  }
}

void ScrollTree::PushScrollUpdatesFromPendingTree(
    PropertyTrees* pending_property_trees,
    LayerTreeImpl* active_tree) {
  DCHECK(property_trees()->is_active);
  DCHECK(!pending_property_trees->is_main_thread);
  DCHECK(!pending_property_trees->is_active);

  // When pushing to the active tree, we can simply copy over the map from the
  // pending tree. The pending and active tree hold a reference to the same
  // SyncedProperty instances.
  synced_scroll_offset_map_.clear();
  for (auto map_entry :
       pending_property_trees->scroll_tree.synced_scroll_offset_map_) {
    synced_scroll_offset_map_[map_entry.first] = map_entry.second;
    if (map_entry.second->PushPendingToActive())
      active_tree->DidUpdateScrollOffset(map_entry.first);
  }
}

void ScrollTree::ApplySentScrollDeltasFromAbortedCommit() {
  DCHECK(property_trees()->is_active);
  for (auto& map_entry : synced_scroll_offset_map_)
    map_entry.second->AbortCommit();
}

void ScrollTree::SetBaseScrollOffset(ElementId id,
                                     const gfx::ScrollOffset& scroll_offset) {
  if (property_trees()->is_main_thread) {
    scroll_offset_map_[id] = scroll_offset;
    return;
  }

  // Scroll offset updates on the impl thread should only be for layers which
  // were created on the main thread. But this method is called when we build
  // PropertyTrees on the impl thread from LayerTreeImpl.
  GetOrCreateSyncedScrollOffset(id)->PushMainToPending(scroll_offset);
}

bool ScrollTree::SetScrollOffset(ElementId id,
                                 const gfx::ScrollOffset& scroll_offset) {
  if (property_trees()->is_main_thread) {
    if (scroll_offset_map_[id] == scroll_offset)
      return false;
    scroll_offset_map_[id] = scroll_offset;
    return true;
  }

  if (property_trees()->is_active) {
    return GetOrCreateSyncedScrollOffset(id)->SetCurrent(scroll_offset);
  }

  return false;
}

bool ScrollTree::UpdateScrollOffsetBaseForTesting(
    ElementId id,
    const gfx::ScrollOffset& offset) {
  DCHECK(!property_trees()->is_main_thread);
  SyncedScrollOffset* synced_scroll_offset = GetOrCreateSyncedScrollOffset(id);
  bool changed = synced_scroll_offset->PushMainToPending(offset);
  if (property_trees()->is_active)
    changed |= synced_scroll_offset->PushPendingToActive();
  return changed;
}

bool ScrollTree::SetScrollOffsetDeltaForTesting(ElementId id,
                                                const gfx::Vector2dF& delta) {
  return GetOrCreateSyncedScrollOffset(id)->SetCurrent(
      GetOrCreateSyncedScrollOffset(id)->ActiveBase() +
      gfx::ScrollOffset(delta));
}

const gfx::ScrollOffset ScrollTree::GetScrollOffsetBaseForTesting(
    ElementId id) const {
  DCHECK(!property_trees()->is_main_thread);
  if (GetSyncedScrollOffset(id))
    return property_trees()->is_active
               ? GetSyncedScrollOffset(id)->ActiveBase()
               : GetSyncedScrollOffset(id)->PendingBase();
  else
    return gfx::ScrollOffset();
}

const gfx::ScrollOffset ScrollTree::GetScrollOffsetDeltaForTesting(
    ElementId id) const {
  DCHECK(!property_trees()->is_main_thread);
  if (GetSyncedScrollOffset(id))
    return property_trees()->is_active
               ? GetSyncedScrollOffset(id)->Delta()
               : GetSyncedScrollOffset(id)->PendingDelta().get();
  else
    return gfx::ScrollOffset();
}

void ScrollTree::DistributeScroll(ScrollNode* scroll_node,
                                  ScrollState* scroll_state) {
  DCHECK(scroll_node && scroll_state);
  if (scroll_state->FullyConsumed())
    return;
  scroll_state->DistributeToScrollChainDescendant();

  // If we're currently scrolling a node other than this one, prevent the scroll
  // from propagating to this node.
  if (scroll_state->delta_consumed_for_scroll_sequence() &&
      scroll_state->current_native_scrolling_node()->id != scroll_node->id) {
    return;
  }

  scroll_state->layer_tree_impl()->ApplyScroll(scroll_node, scroll_state);
}

gfx::Vector2dF ScrollTree::ScrollBy(ScrollNode* scroll_node,
                                    const gfx::Vector2dF& scroll,
                                    LayerTreeImpl* layer_tree_impl) {
  gfx::ScrollOffset adjusted_scroll(scroll);
  if (!scroll_node->user_scrollable_horizontal)
    adjusted_scroll.set_x(0);
  if (!scroll_node->user_scrollable_vertical)
    adjusted_scroll.set_y(0);
  DCHECK(scroll_node->scrollable);
  gfx::ScrollOffset old_offset = current_scroll_offset(scroll_node->element_id);
  gfx::ScrollOffset new_offset =
      ClampScrollOffsetToLimits(old_offset + adjusted_scroll, *scroll_node);
  if (SetScrollOffset(scroll_node->element_id, new_offset))
    layer_tree_impl->DidUpdateScrollOffset(scroll_node->element_id);

  gfx::ScrollOffset unscrolled =
      old_offset + gfx::ScrollOffset(scroll) - new_offset;
  return gfx::Vector2dF(unscrolled.x(), unscrolled.y());
}

gfx::ScrollOffset ScrollTree::ClampScrollOffsetToLimits(
    gfx::ScrollOffset offset,
    const ScrollNode& scroll_node) const {
  offset.SetToMin(MaxScrollOffset(scroll_node.id));
  offset.SetToMax(gfx::ScrollOffset());
  return offset;
}

PropertyTreesCachedData::PropertyTreesCachedData()
    : transform_tree_update_number(0) {
  animation_scales.clear();
}

PropertyTreesCachedData::~PropertyTreesCachedData() = default;

PropertyTrees::PropertyTrees()
    : needs_rebuild(true),
      can_adjust_raster_scales(true),
      changed(false),
      full_tree_damaged(false),
      sequence_number(0),
      is_main_thread(true),
      is_active(false) {
  transform_tree.SetPropertyTrees(this);
  effect_tree.SetPropertyTrees(this);
  clip_tree.SetPropertyTrees(this);
  scroll_tree.SetPropertyTrees(this);
}

PropertyTrees::~PropertyTrees() = default;

bool PropertyTrees::operator==(const PropertyTrees& other) const {
  return transform_tree == other.transform_tree &&
         effect_tree == other.effect_tree && clip_tree == other.clip_tree &&
         scroll_tree == other.scroll_tree &&
         element_id_to_effect_node_index ==
             other.element_id_to_effect_node_index &&
         element_id_to_scroll_node_index ==
             other.element_id_to_scroll_node_index &&
         element_id_to_transform_node_index ==
             other.element_id_to_transform_node_index &&
         needs_rebuild == other.needs_rebuild && changed == other.changed &&
         full_tree_damaged == other.full_tree_damaged &&
         is_main_thread == other.is_main_thread &&
         is_active == other.is_active &&
         can_adjust_raster_scales == other.can_adjust_raster_scales &&
         sequence_number == other.sequence_number;
}

PropertyTrees& PropertyTrees::operator=(const PropertyTrees& from) {
  transform_tree = from.transform_tree;
  effect_tree = from.effect_tree;
  clip_tree = from.clip_tree;
  scroll_tree = from.scroll_tree;
  element_id_to_effect_node_index = from.element_id_to_effect_node_index;
  element_id_to_scroll_node_index = from.element_id_to_scroll_node_index;
  element_id_to_transform_node_index = from.element_id_to_transform_node_index;
  needs_rebuild = from.needs_rebuild;
  changed = from.changed;
  full_tree_damaged = from.full_tree_damaged;
  can_adjust_raster_scales = from.can_adjust_raster_scales;
  sequence_number = from.sequence_number;
  is_main_thread = from.is_main_thread;
  is_active = from.is_active;
  inner_viewport_container_bounds_delta_ =
      from.inner_viewport_container_bounds_delta();
  outer_viewport_container_bounds_delta_ =
      from.outer_viewport_container_bounds_delta();
  inner_viewport_scroll_bounds_delta_ =
      from.inner_viewport_scroll_bounds_delta();
  transform_tree.SetPropertyTrees(this);
  effect_tree.SetPropertyTrees(this);
  clip_tree.SetPropertyTrees(this);
  scroll_tree.SetPropertyTrees(this);
  ResetCachedData();
  return *this;
}

void PropertyTrees::clear() {
  transform_tree.clear();
  clip_tree.clear();
  effect_tree.clear();
  scroll_tree.clear();
  element_id_to_effect_node_index.clear();
  element_id_to_scroll_node_index.clear();
  element_id_to_transform_node_index.clear();

  needs_rebuild = true;
  full_tree_damaged = false;
  changed = false;
  can_adjust_raster_scales = true;
  sequence_number++;

#if DCHECK_IS_ON()
  PropertyTrees tree;
  tree.transform_tree = transform_tree;
  tree.effect_tree = effect_tree;
  tree.clip_tree = clip_tree;
  tree.scroll_tree = scroll_tree;
  tree.scroll_tree.CopyCompleteTreeState(scroll_tree);

  tree.sequence_number = sequence_number;
  tree.is_main_thread = is_main_thread;
  tree.is_active = is_active;
  DCHECK(tree == *this);
#endif
}

void PropertyTrees::SetInnerViewportContainerBoundsDelta(
    gfx::Vector2dF bounds_delta) {
  if (inner_viewport_container_bounds_delta_ == bounds_delta)
    return;

  inner_viewport_container_bounds_delta_ = bounds_delta;
}

void PropertyTrees::SetOuterViewportContainerBoundsDelta(
    gfx::Vector2dF bounds_delta) {
  if (outer_viewport_container_bounds_delta_ == bounds_delta)
    return;

  outer_viewport_container_bounds_delta_ = bounds_delta;
  transform_tree.UpdateOuterViewportContainerBoundsDelta();
}

bool PropertyTrees::ElementIsAnimatingChanged(
    const MutatorHost* mutator_host,
    ElementId element_id,
    ElementListType list_type,
    const PropertyAnimationState& mask,
    const PropertyAnimationState& state,
    bool check_node_existence) {
  bool updated_transform = false;
  for (int property = TargetProperty::FIRST_TARGET_PROPERTY;
       property <= TargetProperty::LAST_TARGET_PROPERTY; ++property) {
    if (!mask.currently_running[property] &&
        !mask.potentially_animating[property])
      continue;

    switch (property) {
      case TargetProperty::TRANSFORM:
        if (TransformNode* transform_node =
                transform_tree.FindNodeFromElementId(element_id)) {
          if (mask.currently_running[property])
            transform_node->is_currently_animating =
                state.currently_running[property];
          if (mask.potentially_animating[property]) {
            transform_node->has_potential_animation =
                state.potentially_animating[property];
            transform_node->has_only_translation_animations =
                mutator_host->HasOnlyTranslationTransforms(element_id,
                                                           list_type);
            transform_tree.set_needs_update(true);
            // We track transform updates specifically, whereas we
            // don't do so for opacity/filter, because whether a
            // transform is animating can change what layer(s) we
            // draw.
            updated_transform = true;
          }
        } else {
          DCHECK_NODE_EXISTENCE(check_node_existence, state, property,
                                needs_rebuild)
              << "Attempting to animate non existent transform node";
        }
        break;
      case TargetProperty::OPACITY:
        if (EffectNode* effect_node =
                effect_tree.FindNodeFromElementId(element_id)) {
          if (mask.currently_running[property])
            effect_node->is_currently_animating_opacity =
                state.currently_running[property];
          if (mask.potentially_animating[property]) {
            effect_node->has_potential_opacity_animation =
                state.potentially_animating[property];
            // We may need to propagate things like screen space opacity.
            effect_tree.set_needs_update(true);
          }
        } else {
          DCHECK_NODE_EXISTENCE(check_node_existence, state, property,
                                needs_rebuild)
              << "Attempting to animate opacity on non existent effect node";
        }
        break;
      case TargetProperty::FILTER:
        if (EffectNode* effect_node =
                effect_tree.FindNodeFromElementId(element_id)) {
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
                                needs_rebuild)
              << "Attempting to animate filter on non existent effect node";
        }
        break;
      default:
        break;
    }
  }
  return updated_transform;
}

void PropertyTrees::SetInnerViewportScrollBoundsDelta(
    gfx::Vector2dF bounds_delta) {
  inner_viewport_scroll_bounds_delta_ = bounds_delta;
}

void PropertyTrees::UpdateChangeTracking() {
  for (int id = EffectTree::kContentsRootNodeId;
       id < static_cast<int>(effect_tree.size()); ++id) {
    EffectNode* node = effect_tree.Node(id);
    EffectNode* parent_node = effect_tree.parent(node);
    effect_tree.UpdateEffectChanged(node, parent_node);
  }
  for (int i = TransformTree::kContentsRootNodeId;
       i < static_cast<int>(transform_tree.size()); ++i) {
    TransformNode* node = transform_tree.Node(i);
    TransformNode* parent_node = transform_tree.parent(node);
    TransformNode* source_node = transform_tree.Node(node->source_node_id);
    transform_tree.UpdateTransformChanged(node, parent_node, source_node);
  }
}

void PropertyTrees::PushChangeTrackingTo(PropertyTrees* tree) {
  for (int id = EffectTree::kContentsRootNodeId;
       id < static_cast<int>(effect_tree.size()); ++id) {
    EffectNode* node = effect_tree.Node(id);
    if (node->effect_changed) {
      EffectNode* target_node = tree->effect_tree.Node(node->id);
      target_node->effect_changed = true;
    }
  }
  for (int id = TransformTree::kContentsRootNodeId;
       id < static_cast<int>(transform_tree.size()); ++id) {
    TransformNode* node = transform_tree.Node(id);
    if (node->transform_changed) {
      TransformNode* target_node = tree->transform_tree.Node(node->id);
      target_node->transform_changed = true;
    }
  }
  // Ensure that change tracking is updated even if property trees don't have
  // other reasons to get updated.
  tree->UpdateChangeTracking();
  tree->full_tree_damaged = full_tree_damaged;
}

void PropertyTrees::ResetAllChangeTracking() {
  transform_tree.ResetChangeTracking();
  effect_tree.ResetChangeTracking();
  changed = false;
  full_tree_damaged = false;
}

std::unique_ptr<base::trace_event::TracedValue> PropertyTrees::AsTracedValue()
    const {
  auto value = base::WrapUnique(new base::trace_event::TracedValue);

  value->SetInteger("sequence_number", sequence_number);

  value->BeginDictionary("transform_tree");
  transform_tree.AsValueInto(value.get());
  value->EndDictionary();

  value->BeginDictionary("effect_tree");
  effect_tree.AsValueInto(value.get());
  value->EndDictionary();

  value->BeginDictionary("clip_tree");
  clip_tree.AsValueInto(value.get());
  value->EndDictionary();

  value->BeginDictionary("scroll_tree");
  scroll_tree.AsValueInto(value.get());
  value->EndDictionary();

  return value;
}

CombinedAnimationScale PropertyTrees::GetAnimationScales(
    int transform_node_id,
    LayerTreeImpl* layer_tree_impl) {
  if (cached_data_.animation_scales[transform_node_id].update_number !=
      cached_data_.transform_tree_update_number) {
    if (!layer_tree_impl->settings()
             .layer_transforms_should_scale_layer_contents) {
      cached_data_.animation_scales[transform_node_id].update_number =
          cached_data_.transform_tree_update_number;
      cached_data_.animation_scales[transform_node_id]
          .combined_maximum_animation_target_scale = 0.f;
      cached_data_.animation_scales[transform_node_id]
          .combined_starting_animation_scale = 0.f;
      return CombinedAnimationScale(
          cached_data_.animation_scales[transform_node_id]
              .combined_maximum_animation_target_scale,
          cached_data_.animation_scales[transform_node_id]
              .combined_starting_animation_scale);
    }

    TransformNode* node = transform_tree.Node(transform_node_id);
    TransformNode* parent_node = transform_tree.parent(node);
    bool ancestor_is_animating_scale = false;
    float ancestor_maximum_target_scale = 0.f;
    float ancestor_starting_animation_scale = 0.f;
    if (parent_node) {
      CombinedAnimationScale combined_animation_scale =
          GetAnimationScales(parent_node->id, layer_tree_impl);
      ancestor_maximum_target_scale =
          combined_animation_scale.maximum_animation_scale;
      ancestor_starting_animation_scale =
          combined_animation_scale.starting_animation_scale;
      ancestor_is_animating_scale =
          cached_data_.animation_scales[parent_node->id]
              .to_screen_has_scale_animation;
    }

    cached_data_.animation_scales[transform_node_id]
        .to_screen_has_scale_animation =
        !node->has_only_translation_animations || ancestor_is_animating_scale;

    // Once we've failed to compute a maximum animated scale at an ancestor, we
    // continue to fail.
    bool failed_at_ancestor =
        ancestor_is_animating_scale && ancestor_maximum_target_scale == 0.f;

    // Computing maximum animated scale in the presence of non-scale/translation
    // transforms isn't supported.
    bool failed_for_non_scale_or_translation =
        !node->to_parent.IsScaleOrTranslation();

    // We don't attempt to accumulate animation scale from multiple nodes with
    // scale animations, because of the risk of significant overestimation. For
    // example, one node might be increasing scale from 1 to 10 at the same time
    // as another node is decreasing scale from 10 to 1. Naively combining these
    // scales would produce a scale of 100.
    bool failed_for_multiple_scale_animations =
        ancestor_is_animating_scale && !node->has_only_translation_animations;

    if (failed_at_ancestor || failed_for_non_scale_or_translation ||
        failed_for_multiple_scale_animations) {
      // This ensures that descendants know we've failed to compute a maximum
      // animated scale.
      cached_data_.animation_scales[transform_node_id]
          .to_screen_has_scale_animation = true;

      cached_data_.animation_scales[transform_node_id]
          .combined_maximum_animation_target_scale = 0.f;
      cached_data_.animation_scales[transform_node_id]
          .combined_starting_animation_scale = 0.f;
    } else if (!cached_data_.animation_scales[transform_node_id]
                    .to_screen_has_scale_animation) {
      cached_data_.animation_scales[transform_node_id]
          .combined_maximum_animation_target_scale = 0.f;
      cached_data_.animation_scales[transform_node_id]
          .combined_starting_animation_scale = 0.f;
    } else if (node->has_only_translation_animations) {
      // An ancestor is animating scale.
      gfx::Vector2dF local_scales =
          MathUtil::ComputeTransform2dScaleComponents(node->local, 0.f);
      float max_local_scale = std::max(local_scales.x(), local_scales.y());
      cached_data_.animation_scales[transform_node_id]
          .combined_maximum_animation_target_scale =
          max_local_scale * ancestor_maximum_target_scale;
      cached_data_.animation_scales[transform_node_id]
          .combined_starting_animation_scale =
          max_local_scale * ancestor_starting_animation_scale;
    } else {
      ElementListType list_type = layer_tree_impl->IsActiveTree()
                                      ? ElementListType::ACTIVE
                                      : ElementListType::PENDING;

      layer_tree_impl->mutator_host()->MaximumTargetScale(
          node->element_id, list_type,
          &cached_data_.animation_scales[transform_node_id]
               .local_maximum_animation_target_scale);
      layer_tree_impl->mutator_host()->AnimationStartScale(
          node->element_id, list_type,
          &cached_data_.animation_scales[transform_node_id]
               .local_starting_animation_scale);
      gfx::Vector2dF local_scales =
          MathUtil::ComputeTransform2dScaleComponents(node->local, 0.f);
      float max_local_scale = std::max(local_scales.x(), local_scales.y());

      if (cached_data_.animation_scales[transform_node_id]
                  .local_starting_animation_scale == 0.f ||
          cached_data_.animation_scales[transform_node_id]
                  .local_maximum_animation_target_scale == 0.f) {
        cached_data_.animation_scales[transform_node_id]
            .combined_maximum_animation_target_scale =
            max_local_scale * ancestor_maximum_target_scale;
        cached_data_.animation_scales[transform_node_id]
            .combined_starting_animation_scale =
            max_local_scale * ancestor_starting_animation_scale;
      } else {
        gfx::Vector2dF ancestor_scales =
            parent_node ? MathUtil::ComputeTransform2dScaleComponents(
                              transform_tree.ToScreen(parent_node->id), 0.f)
                        : gfx::Vector2dF(1.f, 1.f);

        float max_ancestor_scale =
            std::max(ancestor_scales.x(), ancestor_scales.y());
        cached_data_.animation_scales[transform_node_id]
            .combined_maximum_animation_target_scale =
            max_ancestor_scale *
            cached_data_.animation_scales[transform_node_id]
                .local_maximum_animation_target_scale;
        cached_data_.animation_scales[transform_node_id]
            .combined_starting_animation_scale =
            max_ancestor_scale *
            cached_data_.animation_scales[transform_node_id]
                .local_starting_animation_scale;
      }
    }
    cached_data_.animation_scales[transform_node_id].update_number =
        cached_data_.transform_tree_update_number;
  }
  return CombinedAnimationScale(cached_data_.animation_scales[transform_node_id]
                                    .combined_maximum_animation_target_scale,
                                cached_data_.animation_scales[transform_node_id]
                                    .combined_starting_animation_scale);
}

void PropertyTrees::SetAnimationScalesForTesting(
    int transform_id,
    float maximum_animation_scale,
    float starting_animation_scale) {
  cached_data_.animation_scales[transform_id]
      .combined_maximum_animation_target_scale = maximum_animation_scale;
  cached_data_.animation_scales[transform_id]
      .combined_starting_animation_scale = starting_animation_scale;
  cached_data_.animation_scales[transform_id].update_number =
      cached_data_.transform_tree_update_number;
}

bool PropertyTrees::GetToTarget(int transform_id,
                                int effect_id,
                                gfx::Transform* to_target) const {
  if (effect_id == EffectTree::kContentsRootNodeId) {
    *to_target = transform_tree.ToScreen(transform_id);
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
  const TransformNode* node = transform_tree.Node(transform_id);
  if (node->ancestors_are_invertible &&
      effect_id == EffectTree::kContentsRootNodeId) {
    *from_target = transform_tree.FromScreen(transform_id);
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
        transform_data.target_id == EffectTree::kInvalidNodeId) {
      return transform_data;
    }
  }
  // Add an entry to the cache.
  cached_data_.draw_transforms[transform_id].push_back(DrawTransformData());
  DrawTransformData& data = cached_data_.draw_transforms[transform_id].back();
  data.update_number = -1;
  data.target_id = dest_id;
  return data;
}

ClipRectData* PropertyTrees::FetchClipRectFromCache(int clip_id,
                                                    int target_id) {
  ClipNode* clip_node = clip_tree.Node(clip_id);
  for (size_t i = 0; i < clip_node->cached_clip_rects->size(); ++i) {
    auto& data = clip_node->cached_clip_rects[i];
    if (data.target_id == target_id || data.target_id == -1)
      return &data;
  }
  clip_node->cached_clip_rects->emplace_back();
  return &clip_node->cached_clip_rects->back();
}

DrawTransforms& PropertyTrees::GetDrawTransforms(int transform_id,
                                                 int effect_id) const {
  const EffectNode* effect_node = effect_tree.Node(effect_id);
  int dest_id = effect_node->transform_id;

  DrawTransformData& data =
      FetchDrawTransformsDataFromCache(transform_id, dest_id);

  DCHECK(data.update_number != cached_data_.transform_tree_update_number ||
         data.target_id != EffectTree::kInvalidNodeId);
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
    transform_tree.CombineTransformsBetween(transform_id, dest_id,
                                            &target_space_transform);
    target_space_transform.matrix().postScale(
        effect_node->surface_contents_scale.x(),
        effect_node->surface_contents_scale.y(), 1.f);
    data.transforms.to_valid = true;
    data.transforms.from_valid = false;
    data.transforms.might_be_invertible = true;
  } else {
    gfx::Transform combined_transform;
    transform_tree.CombineTransformsBetween(dest_id, transform_id,
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
  cached_data_.animation_scales = std::vector<AnimationScaleData>(
      transform_tree.nodes().size(), AnimationScaleData());
  cached_data_.draw_transforms = std::vector<std::vector<DrawTransformData>>(
      transform_tree.nodes().size(), std::vector<DrawTransformData>(1));
}

void PropertyTrees::UpdateTransformTreeUpdateNumber() {
  cached_data_.transform_tree_update_number++;
}

gfx::Transform PropertyTrees::ToScreenSpaceTransformWithoutSurfaceContentsScale(
    int transform_id,
    int effect_id) const {
  if (transform_id == TransformTree::kRootNodeId) {
    return gfx::Transform();
  }
  gfx::Transform screen_space_transform = transform_tree.ToScreen(transform_id);
  const EffectNode* effect_node = effect_tree.Node(effect_id);

  if (effect_node->surface_contents_scale.x() != 0.0 &&
      effect_node->surface_contents_scale.y() != 0.0)
    screen_space_transform.Scale(1.0 / effect_node->surface_contents_scale.x(),
                                 1.0 / effect_node->surface_contents_scale.y());
  return screen_space_transform;
}

}  // namespace cc
