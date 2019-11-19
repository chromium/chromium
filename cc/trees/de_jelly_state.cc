// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/de_jelly_state.h"

#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "components/viz/common/display/de_jelly.h"

namespace cc {

DeJellyState::DeJellyState() = default;
DeJellyState::~DeJellyState() = default;

void DeJellyState::AdvanceFrame(LayerTreeImpl* layer_tree_impl) {
  if (!layer_tree_impl->settings().allow_de_jelly_effect)
    return;

  should_de_jelly_ = false;

  // Move the |new_transforms_| from the previous draw into
  // |previous_transforms_|.
  std::swap(previous_transforms_, new_transforms_);
  new_transforms_.clear();

  // Make sure we have an active scroll node. Otherwise we won't perform any
  // de-jelly.
  ScrollNode* current_scroll =
      layer_tree_impl->property_trees()->scroll_tree.Node(
          layer_tree_impl->property_trees()
              ->scroll_tree.currently_scrolling_node());
  if (!current_scroll) {
    new_scroll_node_transform_.reset();
    return;
  }

  scroll_transform_node_ = current_scroll->transform_id;

  // Check to make sure the ToScreen transform of our scroll node is not a
  // complex transform (doesn't work well with de-jelly). Also make sure the
  // scale is not changing.
  base::Optional<gfx::Transform> previous_scroll_transform =
      new_scroll_node_transform_;
  new_scroll_node_transform_ =
      layer_tree_impl->property_trees()->transform_tree.ToScreen(
          current_scroll->transform_id);
  if (!previous_scroll_transform ||
      !previous_scroll_transform->IsScaleOrTranslation() ||
      !new_scroll_node_transform_->IsScaleOrTranslation() ||
      new_scroll_node_transform_->Scale2d() !=
          previous_scroll_transform->Scale2d()) {
    return;
  }

  // Compute the fallback movement of a scrolling element based purely on the
  // scroll offset of the currently scrolling node.
  float previous_scroll_offset = scroll_offset_;
  scroll_offset_ = layer_tree_impl->property_trees()
                       ->transform_tree.Node(scroll_transform_node_)
                       ->scroll_offset.y();
  fallback_delta_y_ = scroll_offset_ - previous_scroll_offset;
  gfx::Vector3dF vector(0, fallback_delta_y_, 0);
  new_scroll_node_transform_->TransformVector(&vector);
  fallback_delta_y_ = vector.y();

  // Don't attempt de-jelly while the omnibox is transitioning in or out. There
  // is no correct way to handle this.
  float top_controls_shown_ratio =
      layer_tree_impl->top_controls_shown_ratio()->Current(
          true /* is_active_tree */);
  if (top_controls_shown_ratio != 0.0f && top_controls_shown_ratio != 1.0f)
    return;

  // We've passed our initial checks, allow de-jelly in UpdateSharedQuadState.
  should_de_jelly_ = true;
}

void DeJellyState::UpdateSharedQuadState(LayerTreeImpl* layer_tree_impl,
                                         int transform_id,
                                         viz::RenderPass* target_render_pass) {
  if (!should_de_jelly_)
    return;
  DCHECK(layer_tree_impl->settings().allow_de_jelly_effect);

  viz::SharedQuadState* state =
      target_render_pass->shared_quad_state_list.back();
  state->de_jelly_delta_y = 0.0f;

  // Check if |transform_id| is a child of our |scroll_transform_node_|
  // and if it scrolls (is not sticky or fixed).
  bool does_not_scroll = false;
  auto node_id = transform_id;
  while (node_id != scroll_transform_node_ && node_id != kInvalidNodeId) {
    auto* current_node =
        layer_tree_impl->property_trees()->transform_tree.Node(node_id);

    // Position fixed.
    if (current_node->moved_by_outer_viewport_bounds_delta_y) {
      does_not_scroll = true;
      break;
    }
    // Position sticky.
    if (current_node->sticky_position_constraint_id > -1) {
      const StickyPositionNodeData* sticky_data =
          layer_tree_impl->property_trees()
              ->transform_tree.GetStickyPositionData(node_id);
      if (sticky_data &&
          sticky_data->total_containing_block_sticky_offset.y() > 0.0f) {
        does_not_scroll = true;
        break;
      }
    }

    node_id = current_node->parent_id;
  }
  does_not_scroll |= node_id == kInvalidNodeId;
  if (does_not_scroll)
    return;

  // Get the current node's ToScreen transform.
  gfx::Transform transform =
      layer_tree_impl->property_trees()->transform_tree.ToScreen(transform_id);
  new_transforms_[transform_id] = transform;

  // Get the previous transform (if any).
  const auto& found = previous_transforms_.find(transform_id);

  float delta_y = 0.0f;
  if (found == previous_transforms_.end()) {
    delta_y = fallback_delta_y_;
  } else {
    // Calculate the delta of point (0, 0) from the previous frame.
    gfx::Transform previous_transform = found->second;
    gfx::PointF new_point(0, 0);
    transform.TransformPoint(&new_point);
    gfx::PointF old_point(0, 0);
    previous_transform.TransformPoint(&old_point);
    delta_y = old_point.y() - new_point.y();
  }

  if (delta_y == 0.0f) {
    return;
  }

  // To minimize jarring visible effects, we de-jelly differently at
  // different magnitudes of |delta_y|. This is controlled by three variables:
  // kLinearDeJellyStart, kFixedDeJellyStart, kZeroDeJellyStart.
  //                              _____________
  //                  |     |    _/|           |
  // de_jelly_delta_y |     |  _/  |           |
  //                  |_____|_/    |           |_______________
  //                  +----------------------------------------
  //                        kLinear   kFixed          kZero
  //
  const float kLinearDeJellyStart = 2.0f;
  const float kFixedDeJellyStart =
      viz::MaxDeJellyHeight() + kLinearDeJellyStart;
  const float kZeroDeJellyStart = 100.0f + kLinearDeJellyStart;
  float sign = std::abs(delta_y) / delta_y;
  float de_jelly_delta_y = std::abs(delta_y);
  if (de_jelly_delta_y > kZeroDeJellyStart) {
    de_jelly_delta_y = 0.0f;
  } else if (de_jelly_delta_y > kFixedDeJellyStart) {
    de_jelly_delta_y = kFixedDeJellyStart - kLinearDeJellyStart;
  } else if (de_jelly_delta_y > kLinearDeJellyStart) {
    de_jelly_delta_y = std::max(0.0f, de_jelly_delta_y - kLinearDeJellyStart);
  } else {
    de_jelly_delta_y = 0.0f;
  }
  // Re-apply the sign.
  de_jelly_delta_y *= sign;
  if (de_jelly_delta_y == 0.0f) {
    return;
  }

  state->de_jelly_delta_y = de_jelly_delta_y;
}

}  // namespace cc
