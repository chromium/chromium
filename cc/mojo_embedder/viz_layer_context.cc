// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojo_embedder/viz_layer_context.h"

#include <cstdint>
#include <memory>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "cc/animation/animation.h"
#include "cc/animation/animation_host.h"
#include "cc/animation/animation_timeline.h"
#include "cc/animation/keyframe_effect.h"
#include "cc/animation/keyframe_model.h"
#include "cc/layers/layer_impl.h"
#include "cc/layers/mirror_layer_impl.h"
#include "cc/layers/nine_patch_thumb_scrollbar_layer_impl.h"
#include "cc/layers/painted_scrollbar_layer_impl.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/layers/solid_color_scrollbar_layer_impl.h"
#include "cc/layers/surface_layer_impl.h"
#include "cc/layers/texture_layer_impl.h"
#include "cc/layers/view_transition_content_layer_impl.h"
#include "cc/tiles/picture_layer_tiling.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree.h"
#include "components/viz/client/client_resource_provider.h"
#include "services/viz/public/mojom/compositing/layer.mojom.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"
#include "ui/gfx/animation/keyframe/animation_curve.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"
#include "ui/gfx/animation/keyframe/timing_function.h"
#include "ui/gfx/geometry/cubic_bezier.h"
#include "ui/gfx/geometry/transform_operation.h"

namespace cc::mojo_embedder {

namespace {

void ComputePropertyTreeNodeUpdate(
    const TransformNode* old_node,
    const TransformNode& new_node,
    std::vector<viz::mojom::TransformNodePtr>& container) {
  // TODO(https://crbug.com/40902503): This is a subset of the properties we
  // need to sync.
  if (old_node && old_node->id == new_node.id &&
      old_node->parent_id == new_node.parent_id &&
      old_node->parent_frame_id == new_node.parent_frame_id &&
      old_node->element_id == new_node.element_id &&
      old_node->local == new_node.local &&
      old_node->origin == new_node.origin &&
      old_node->post_translation == new_node.post_translation &&
      old_node->to_parent == new_node.to_parent &&
      old_node->sticky_position_constraint_id ==
          new_node.sticky_position_constraint_id &&
      old_node->anchor_position_scroll_data_id ==
          new_node.anchor_position_scroll_data_id &&
      old_node->sorting_context_id == new_node.sorting_context_id &&
      old_node->scroll_offset() == new_node.scroll_offset() &&
      old_node->snap_amount == new_node.snap_amount &&
      old_node->has_potential_animation == new_node.has_potential_animation &&
      old_node->is_currently_animating == new_node.is_currently_animating &&
      old_node->flattens_inherited_transform ==
          new_node.flattens_inherited_transform &&
      old_node->scrolls == new_node.scrolls &&
      old_node->should_undo_overscroll == new_node.should_undo_overscroll &&
      old_node->should_be_snapped == new_node.should_be_snapped &&
      old_node->moved_by_outer_viewport_bounds_delta_y ==
          new_node.moved_by_outer_viewport_bounds_delta_y &&
      old_node->in_subtree_of_page_scale_layer ==
          new_node.in_subtree_of_page_scale_layer &&
      old_node->delegates_to_parent_for_backface ==
          new_node.delegates_to_parent_for_backface &&
      old_node->will_change_transform == new_node.will_change_transform &&
      old_node->visible_frame_element_id == new_node.visible_frame_element_id) {
    return;
  }

  auto wire = viz::mojom::TransformNode::New();
  wire->id = new_node.id;
  wire->parent_id = new_node.parent_id;
  wire->parent_frame_id = new_node.parent_frame_id;
  wire->element_id = new_node.element_id;
  wire->local = new_node.local;
  wire->origin = new_node.origin;
  wire->post_translation = new_node.post_translation;
  wire->to_parent = new_node.to_parent;
  if (new_node.sticky_position_constraint_id >= 0) {
    wire->sticky_position_constraint_id =
        base::checked_cast<uint32_t>(new_node.sticky_position_constraint_id);
  }
  if (new_node.anchor_position_scroll_data_id >= 0) {
    wire->anchor_position_scroll_data_id =
        base::checked_cast<uint32_t>(new_node.anchor_position_scroll_data_id);
  }
  wire->sorting_context_id = new_node.sorting_context_id;
  wire->scroll_offset = new_node.scroll_offset();
  wire->snap_amount = new_node.snap_amount;
  wire->has_potential_animation = new_node.has_potential_animation;
  wire->is_currently_animating = new_node.is_currently_animating;
  wire->flattens_inherited_transform = new_node.flattens_inherited_transform;
  wire->scrolls = new_node.scrolls;
  wire->should_undo_overscroll = new_node.should_undo_overscroll;
  wire->should_be_snapped = new_node.should_be_snapped;
  wire->moved_by_outer_viewport_bounds_delta_y =
      new_node.moved_by_outer_viewport_bounds_delta_y;
  wire->in_subtree_of_page_scale_layer =
      new_node.in_subtree_of_page_scale_layer;
  wire->delegates_to_parent_for_backface =
      new_node.delegates_to_parent_for_backface;
  wire->will_change_transform = new_node.will_change_transform;
  wire->visible_frame_element_id = new_node.visible_frame_element_id;
  wire->damage_reasons_bit_mask = new_node.damage_reasons().ToEnumBitmask();
  wire->moved_by_safe_area_bottom = new_node.moved_by_safe_area_bottom;
  container.push_back(std::move(wire));
}

void ComputePropertyTreeNodeUpdate(
    const ClipNode* old_node,
    const ClipNode& new_node,
    std::vector<viz::mojom::ClipNodePtr>& container) {
  if (old_node && old_node->id == new_node.id &&
      old_node->parent_id == new_node.parent_id &&
      old_node->transform_id == new_node.transform_id &&
      old_node->clip == new_node.clip &&
      old_node->pixel_moving_filter_id == new_node.pixel_moving_filter_id) {
    return;
  }

  auto wire = viz::mojom::ClipNode::New();
  wire->id = new_node.id;
  wire->parent_id = new_node.parent_id;
  wire->transform_id = new_node.transform_id;
  wire->clip = new_node.clip;
  wire->pixel_moving_filter_id = new_node.pixel_moving_filter_id;
  container.push_back(std::move(wire));
}

void ComputePropertyTreeNodeUpdate(
    const EffectNode* old_node,
    const EffectNode& new_node,
    std::vector<viz::mojom::EffectNodePtr>& container,
    std::vector<std::unique_ptr<viz::CopyOutputRequest>> copy_requests) {
  if (old_node && old_node->id == new_node.id &&
      old_node->parent_id == new_node.parent_id &&
      old_node->transform_id == new_node.transform_id &&
      old_node->clip_id == new_node.clip_id &&
      old_node->element_id == new_node.element_id &&
      old_node->opacity == new_node.opacity &&
      old_node->render_surface_reason == new_node.render_surface_reason &&
      old_node->surface_contents_scale == new_node.surface_contents_scale &&
      old_node->subtree_capture_id == new_node.subtree_capture_id &&
      old_node->subtree_size == new_node.subtree_size &&
      old_node->blend_mode == new_node.blend_mode &&
      old_node->target_id == new_node.target_id &&
      old_node->view_transition_target_id ==
          new_node.view_transition_target_id &&
      old_node->closest_ancestor_with_cached_render_surface_id ==
          new_node.closest_ancestor_with_cached_render_surface_id &&
      old_node->closest_ancestor_with_copy_request_id ==
          new_node.closest_ancestor_with_copy_request_id &&
      old_node->closest_ancestor_being_captured_id ==
          new_node.closest_ancestor_being_captured_id &&
      old_node->closest_ancestor_with_shared_element_id ==
          new_node.closest_ancestor_with_shared_element_id &&
      old_node->view_transition_element_resource_id ==
          new_node.view_transition_element_resource_id &&
      old_node->has_copy_request == new_node.has_copy_request &&
      old_node->filters == new_node.filters &&
      old_node->backdrop_filters == new_node.backdrop_filters &&
      old_node->backdrop_filter_bounds == new_node.backdrop_filter_bounds &&
      old_node->backdrop_filter_quality == new_node.backdrop_filter_quality &&
      old_node->backdrop_mask_element_id == new_node.backdrop_mask_element_id &&
      old_node->mask_filter_info == new_node.mask_filter_info &&
      old_node->cache_render_surface == new_node.cache_render_surface &&
      old_node->hidden_by_backface_visibility ==
          new_node.hidden_by_backface_visibility &&
      old_node->double_sided == new_node.double_sided &&
      old_node->trilinear_filtering == new_node.trilinear_filtering &&
      old_node->is_drawn == new_node.is_drawn &&
      old_node->only_draws_visible_content ==
          new_node.only_draws_visible_content &&
      old_node->subtree_hidden == new_node.subtree_hidden &&
      old_node->has_potential_filter_animation ==
          new_node.has_potential_filter_animation &&
      old_node->has_potential_backdrop_filter_animation ==
          new_node.has_potential_backdrop_filter_animation &&
      old_node->has_potential_opacity_animation ==
          new_node.has_potential_opacity_animation &&
      old_node->has_masking_child == new_node.has_masking_child &&
      old_node->subtree_has_copy_request == new_node.subtree_has_copy_request &&
      old_node->is_fast_rounded_corner == new_node.is_fast_rounded_corner &&
      old_node->node_or_ancestor_has_fast_rounded_corner ==
          new_node.node_or_ancestor_has_fast_rounded_corner &&
      old_node->lcd_text_disallowed_by_filter ==
          new_node.lcd_text_disallowed_by_filter &&
      old_node->lcd_text_disallowed_by_backdrop_filter ==
          new_node.lcd_text_disallowed_by_backdrop_filter &&
      old_node->may_have_backdrop_effect == new_node.may_have_backdrop_effect &&
      old_node->needs_effect_for_2d_scale_transform ==
          new_node.needs_effect_for_2d_scale_transform &&
      copy_requests.empty()) {
    return;
  }

  auto wire = viz::mojom::EffectNode::New();
  wire->id = new_node.id;
  wire->parent_id = new_node.parent_id;
  wire->transform_id = new_node.transform_id;
  wire->clip_id = new_node.clip_id;
  wire->element_id = new_node.element_id;
  wire->opacity = new_node.opacity;
  wire->has_render_surface =
      new_node.render_surface_reason != RenderSurfaceReason::kNone;
  wire->surface_contents_scale = new_node.surface_contents_scale;
  wire->subtree_capture_id = new_node.subtree_capture_id;
  wire->subtree_size = new_node.subtree_size;
  wire->blend_mode = base::checked_cast<uint32_t>(new_node.blend_mode);
  wire->target_id = new_node.target_id;
  wire->view_transition_target_id = new_node.view_transition_target_id;
  wire->closest_ancestor_with_cached_render_surface_id =
      new_node.closest_ancestor_with_cached_render_surface_id;
  wire->closest_ancestor_with_copy_request_id =
      new_node.closest_ancestor_with_copy_request_id;
  wire->closest_ancestor_being_captured_id =
      new_node.closest_ancestor_being_captured_id;
  wire->closest_ancestor_with_shared_element_id =
      new_node.closest_ancestor_with_shared_element_id;
  wire->view_transition_element_resource_id =
      new_node.view_transition_element_resource_id;
  wire->copy_output_requests = std::move(copy_requests);
  wire->filters = new_node.filters;
  wire->backdrop_filters = new_node.backdrop_filters;
  wire->backdrop_filter_bounds = new_node.backdrop_filter_bounds;
  wire->backdrop_filter_quality = new_node.backdrop_filter_quality;
  wire->backdrop_mask_element_id = new_node.backdrop_mask_element_id;
  wire->mask_filter_info = new_node.mask_filter_info;

  wire->cache_render_surface = new_node.cache_render_surface;
  wire->double_sided = new_node.double_sided;
  wire->trilinear_filtering = new_node.trilinear_filtering;
  wire->subtree_hidden = new_node.subtree_hidden;
  wire->has_potential_filter_animation =
      new_node.has_potential_filter_animation;
  wire->has_potential_backdrop_filter_animation =
      new_node.has_potential_backdrop_filter_animation;
  wire->has_potential_opacity_animation =
      new_node.has_potential_opacity_animation;
  wire->subtree_has_copy_request = new_node.subtree_has_copy_request;
  wire->is_fast_rounded_corner = new_node.is_fast_rounded_corner;
  wire->may_have_backdrop_effect = new_node.may_have_backdrop_effect;
  wire->needs_effect_for_2d_scale_transform =
      new_node.needs_effect_for_2d_scale_transform;

  container.push_back(std::move(wire));
}

void ComputePropertyTreeNodeUpdate(
    const ScrollNode* old_node,
    const ScrollNode& new_node,
    std::vector<viz::mojom::ScrollNodePtr>& container) {
  if (old_node && old_node->id == new_node.id &&
      old_node->parent_id == new_node.parent_id &&
      old_node->transform_id == new_node.transform_id &&
      old_node->container_bounds == new_node.container_bounds &&
      old_node->bounds == new_node.bounds &&
      old_node->max_scroll_offset_affected_by_page_scale ==
          new_node.max_scroll_offset_affected_by_page_scale &&
      old_node->scrolls_inner_viewport == new_node.scrolls_inner_viewport &&
      old_node->scrolls_outer_viewport == new_node.scrolls_outer_viewport &&
      old_node->prevent_viewport_scrolling_from_inner ==
          new_node.prevent_viewport_scrolling_from_inner &&
      old_node->user_scrollable_horizontal ==
          new_node.user_scrollable_horizontal &&
      old_node->user_scrollable_vertical == new_node.user_scrollable_vertical &&
      old_node->is_composited == new_node.is_composited &&
      old_node->element_id == new_node.element_id) {
    return;
  }

  auto wire = viz::mojom::ScrollNode::New();
  wire->id = new_node.id;
  wire->parent_id = new_node.parent_id;
  wire->transform_id = new_node.transform_id;
  wire->container_bounds = new_node.container_bounds;
  wire->bounds = new_node.bounds;
  wire->max_scroll_offset_affected_by_page_scale =
      new_node.max_scroll_offset_affected_by_page_scale;
  wire->scrolls_inner_viewport = new_node.scrolls_inner_viewport;
  wire->scrolls_outer_viewport = new_node.scrolls_outer_viewport;
  wire->prevent_viewport_scrolling_from_inner =
      new_node.prevent_viewport_scrolling_from_inner;
  wire->user_scrollable_horizontal = new_node.user_scrollable_horizontal;
  wire->user_scrollable_vertical = new_node.user_scrollable_vertical;
  wire->is_composited = new_node.is_composited;
  wire->element_id = new_node.element_id;
  container.push_back(std::move(wire));
}

template <typename TreeType, typename ContainerType>
void ComputePropertyTreeUpdate(const TreeType& old_tree,
                               const TreeType& new_tree,
                               ContainerType& updates,
                               uint32_t& new_num_nodes) {
  using NodeType = typename TreeType::NodeType;
  new_num_nodes = base::checked_cast<uint32_t>(new_tree.size());
  for (size_t i = 0; i < new_tree.size(); ++i) {
    const NodeType* old_node = old_tree.size() > i ? old_tree.Node(i) : nullptr;
    ComputePropertyTreeNodeUpdate(old_node, *new_tree.Node(i), updates);
  }
}

void ComputeEffectTreeUpdate(const EffectTree& old_tree,
                             EffectTree& new_tree,
                             std::vector<::viz::mojom::EffectNodePtr>& updates,
                             uint32_t& new_num_nodes) {
  // Take any copy output requests from `new_tree` to push over the wire.
  auto copy_requests = new_tree.TakeCopyRequests();

  new_num_nodes = base::checked_cast<uint32_t>(new_tree.size());
  for (size_t i = 0; i < new_tree.size(); ++i) {
    const auto* old_node = old_tree.size() > i ? old_tree.Node(i) : nullptr;

    // Push any copy output requests for this node.
    auto range = copy_requests.equal_range(i);
    std::vector<std::unique_ptr<viz::CopyOutputRequest>> copy_requests_for_node;
    for (auto it = range.first; it != range.second; ++it) {
      copy_requests_for_node.push_back(std::move(it->second));
    }

    ComputePropertyTreeNodeUpdate(old_node, *new_tree.Node(i), updates,
                                  std::move(copy_requests_for_node));
  }
}

std::vector<viz::mojom::StickyPositionNodeDataPtr> SerializeStickyPositionData(
    const std::vector<StickyPositionNodeData>& entries) {
  std::vector<viz::mojom::StickyPositionNodeDataPtr> wire_data;
  for (const auto& data : entries) {
    auto wire = viz::mojom::StickyPositionNodeData::New();
    wire->scroll_ancestor = data.scroll_ancestor;
    wire->is_anchored_left = data.constraints.is_anchored_left;
    wire->is_anchored_right = data.constraints.is_anchored_right;
    wire->is_anchored_top = data.constraints.is_anchored_top;
    wire->is_anchored_bottom = data.constraints.is_anchored_bottom;
    wire->left_offset = data.constraints.left_offset;
    wire->right_offset = data.constraints.right_offset;
    wire->top_offset = data.constraints.top_offset;
    wire->bottom_offset = data.constraints.bottom_offset;
    wire->constraint_box_rect = data.constraints.constraint_box_rect;
    wire->scroll_container_relative_sticky_box_rect =
        data.constraints.scroll_container_relative_sticky_box_rect;
    wire->scroll_container_relative_containing_block_rect =
        data.constraints.scroll_container_relative_containing_block_rect;
    wire->pixel_snap_offset = data.constraints.pixel_snap_offset;
    wire->nearest_node_shifting_sticky_box =
        data.nearest_node_shifting_sticky_box;
    wire->nearest_node_shifting_containing_block =
        data.nearest_node_shifting_containing_block;
    wire->total_sticky_box_sticky_offset = data.total_sticky_box_sticky_offset;
    wire->total_containing_block_sticky_offset =
        data.total_containing_block_sticky_offset;
    wire_data.push_back(std::move(wire));
  }
  return wire_data;
}

std::vector<viz::mojom::AnchorPositionScrollDataPtr>
SerializeAnchorPositionScrollData(
    const std::vector<AnchorPositionScrollData>& entries) {
  std::vector<viz::mojom::AnchorPositionScrollDataPtr> wire_data;
  for (const auto& data : entries) {
    auto wire = viz::mojom::AnchorPositionScrollData::New();
    wire->adjustment_container_ids = data.adjustment_container_ids;
    wire->accumulated_scroll_origin = data.accumulated_scroll_origin;
    wire->needs_scroll_adjustment_in_x = data.needs_scroll_adjustment_in_x;
    wire->needs_scroll_adjustment_in_y = data.needs_scroll_adjustment_in_y;
    wire_data.push_back(std::move(wire));
  }
  return wire_data;
}

viz::mojom::TransformTreeUpdatePtr ComputeTransformTreePropertiesUpdate(
    const TransformTree& old_tree,
    const TransformTree& new_tree) {
  if (old_tree.page_scale_factor() == new_tree.page_scale_factor() &&
      old_tree.device_scale_factor() == new_tree.device_scale_factor() &&
      old_tree.device_transform_scale_factor() ==
          new_tree.device_transform_scale_factor() &&
      old_tree.nodes_affected_by_outer_viewport_bounds_delta() ==
          new_tree.nodes_affected_by_outer_viewport_bounds_delta() &&
      old_tree.sticky_position_data() == new_tree.sticky_position_data() &&
      old_tree.anchor_position_scroll_data() ==
          new_tree.anchor_position_scroll_data()) {
    return nullptr;
  }

  auto wire = viz::mojom::TransformTreeUpdate::New();
  wire->page_scale_factor = new_tree.page_scale_factor();
  wire->device_scale_factor = new_tree.device_scale_factor();
  wire->device_transform_scale_factor =
      new_tree.device_transform_scale_factor();
  wire->nodes_affected_by_outer_viewport_bounds_delta =
      new_tree.nodes_affected_by_outer_viewport_bounds_delta();
  wire->sticky_position_data =
      SerializeStickyPositionData(new_tree.sticky_position_data());
  wire->anchor_position_scroll_data =
      SerializeAnchorPositionScrollData(new_tree.anchor_position_scroll_data());
  return wire;
}

viz::mojom::ScrollTreeUpdatePtr ComputeScrollTreePropertiesUpdate(
    const ScrollTree& old_tree,
    const ScrollTree& new_tree) {
  if (old_tree.synced_scroll_offset_map() ==
      new_tree.synced_scroll_offset_map()) {
    return nullptr;
  }

  auto wire = viz::mojom::ScrollTreeUpdate::New();
  wire->synced_scroll_offsets = new_tree.synced_scroll_offset_map();

  return wire;
}

void SerializeUIResourceRequest(
    cc::LayerTreeHostImpl& host_impl,
    viz::RasterContextProvider& context_provider,
    viz::mojom::LayerTreeUpdate& update,
    cc::UIResourceId uid,
    viz::mojom::TransferableUIResourceRequest::Type type) {
  if (type == viz::mojom::TransferableUIResourceRequest::Type::kCreate) {
    std::vector<viz::ResourceId> ids;
    std::vector<viz::TransferableResource> resources;

    viz::ResourceId resource_id = host_impl.ResourceIdForUIResource(uid);
    bool opaque = host_impl.IsUIResourceOpaque(uid);
    ids.push_back(resource_id);
    host_impl.resource_provider()->PrepareSendToParent(ids, &resources,
                                                       &context_provider);
    CHECK_EQ(resources.size(), ids.size());

    auto& request = update.ui_resource_requests.emplace_back(
        viz::mojom::TransferableUIResourceRequest::New());
    request->type = type;
    request->uid = uid;
    request->transferable_resource = resources[0];
    request->opaque = opaque;
  } else {
    CHECK_EQ(type, viz::mojom::TransferableUIResourceRequest::Type::kDelete);
    auto& request = update.ui_resource_requests.emplace_back(
        viz::mojom::TransferableUIResourceRequest::New());
    request->type = type;
    request->uid = uid;
  }
}

viz::mojom::TileResourcePtr SerializeTileResource(
    const Tile& tile,
    viz::ClientResourceProvider& resource_provider,
    viz::RasterContextProvider& context_provider) {
  const auto& draw_info = tile.draw_info();
  std::vector<viz::ResourceId> ids(1, draw_info.resource_id_for_export());
  std::vector<viz::TransferableResource> resources;
  resource_provider.PrepareSendToParent(ids, &resources, &context_provider);
  CHECK_EQ(resources.size(), 1u);

  auto wire = viz::mojom::TileResource::New();
  wire->resource = resources[0];

  wire->is_checkered = draw_info.is_checker_imaged();
  return wire;
}

viz::mojom::TilePtr SerializeTile(
    const Tile& tile,
    viz::ClientResourceProvider& resource_provider,
    viz::RasterContextProvider& context_provider) {
  auto wire = viz::mojom::Tile::New();
  wire->column_index = tile.tiling_i_index();
  wire->row_index = tile.tiling_j_index();

  switch (tile.draw_info().mode()) {
    case TileDrawInfo::OOM_MODE:
      wire->contents = viz::mojom::TileContents::NewMissingReason(
          mojom::MissingTileReason::kOutOfMemory);
      break;

    case TileDrawInfo::SOLID_COLOR_MODE:
      wire->contents = viz::mojom::TileContents::NewSolidColor(
          tile.draw_info().solid_color());
      break;

    case TileDrawInfo::RESOURCE_MODE:
      if (tile.draw_info().has_resource() &&
          tile.draw_info().is_resource_ready_to_draw()) {
        wire->contents = viz::mojom::TileContents::NewResource(
            SerializeTileResource(tile, resource_provider, context_provider));
      } else {
        wire->contents = viz::mojom::TileContents::NewMissingReason(
            mojom::MissingTileReason::kResourceNotReady);
      }
      break;
  }
  return wire;
}

// Serializes a set of tile updates (live or deleted) into a mojo Tiling object.
// Handles nullptr tiling as a deleted tiling, and wraps valid and missing
// tiles.
viz::mojom::TilingPtr SerializeTiling(
    PictureLayerImpl& layer,
    const PictureLayerTiling* tiling,
    float scale_key,
    base::span<const std::pair<TileIndex, const Tile*>> tile_updates,
    viz::ClientResourceProvider& resource_provider,
    viz::RasterContextProvider& context_provider) {
  // Handle the case where the tiling no longer exists (deleted).
  if (!tiling) {
    auto deleted_tiling = viz::mojom::Tiling::New();
    deleted_tiling->layer_id = layer.id();
    deleted_tiling->scale_key = scale_key;
    deleted_tiling->is_deleted = true;
    return deleted_tiling;
  }

  std::vector<viz::mojom::TilePtr> wire_tiles;

  // Serialize both live and deleted tiles into mojo wire format.
  for (const auto& [index, tile] : tile_updates) {
    if (tile && !tile->deleted()) {
      // Serialize a live tile with content.
      if (auto wire_tile =
              SerializeTile(*tile, resource_provider, context_provider)) {
        wire_tiles.push_back(std::move(wire_tile));
      }
    } else {
      // Tile was deleted or missing, serialize a tile with deletion reason.
      // Mark the reason as kTileDeleted. This is essential to distinguish
      // deleted tiles from OOMed OR RESOURCE_MODE tiles with no resources.
      // OOMed tiles have no content but are still required in order to perform
      // checkerboard.
      auto deleted_tile = viz::mojom::Tile::New();
      deleted_tile->column_index = index.i;
      deleted_tile->row_index = index.j;
      deleted_tile->contents = viz::mojom::TileContents::NewMissingReason(
          mojom::MissingTileReason::kTileDeleted);
      wire_tiles.push_back(std::move(deleted_tile));
    }
  }

  if (wire_tiles.empty()) {
    return nullptr;
  }

  // Wrap into a mojo Tiling object.
  auto wire = viz::mojom::Tiling::New();
  wire->layer_id = layer.id();
  wire->scale_key = scale_key;
  wire->raster_translation = tiling->raster_transform().translation();
  wire->raster_scale = tiling->raster_transform().scale();
  wire->tile_size = tiling->tile_size();
  wire->tiling_rect = tiling->tiling_rect();
  wire->tiles = std::move(wire_tiles);
  wire->is_deleted = false;
  return wire;
}

// Collects updated tile indices and serializes them into tilings for the given
// layer.
void SerializePictureLayerTileUpdates(
    PictureLayerImpl& layer,
    viz::ClientResourceProvider& resource_provider,
    viz::RasterContextProvider& context_provider,
    std::vector<viz::mojom::TilingPtr>& tilings) {
  auto updates = layer.TakeUpdatedTiles();

  for (const auto& [scale_key, tile_indices] : updates) {
    const auto* tiling =
        layer.picture_layer_tiling_set()->FindTilingWithScaleKey(scale_key);

    // Create a unified vector of tile updates, marking missing tiles with
    // nullptr.
    std::vector<std::pair<TileIndex, const Tile*>> tile_updates;
    tile_updates.reserve(tile_indices.size());
    for (const auto& index : tile_indices) {
      const Tile* tile = tiling ? tiling->TileAt(index) : nullptr;
      tile_updates.emplace_back(index, tile);
    }

    // Serialize the tiling and push to output.
    if (auto wire_tiling =
            SerializeTiling(layer, tiling, scale_key, tile_updates,
                            resource_provider, context_provider)) {
      tilings.push_back(std::move(wire_tiling));
    }
  }
}

void SerializeMirrorLayerExtra(MirrorLayerImpl& layer,
                               viz::mojom::MirrorLayerExtraPtr& extra) {
  extra->mirrored_layer_id = layer.mirrored_layer_id();
}

void SerializeTextureLayerExtra(TextureLayerImpl& layer,
                                viz::mojom::TextureLayerExtraPtr& extra,
                                viz::ClientResourceProvider& resource_provider,
                                viz::RasterContextProvider& context_provider) {
  extra->blend_background_color = layer.blend_background_color();
  extra->force_texture_to_opaque = layer.force_texture_to_opaque();
  extra->uv_top_left = layer.uv_top_left();
  extra->uv_bottom_right = layer.uv_bottom_right();

  if (layer.needs_set_resource_push()) {
    if (layer.resource_id() != viz::kInvalidResourceId) {
      std::vector<viz::ResourceId> ids(1, layer.resource_id());
      std::vector<viz::TransferableResource> resources;
      resource_provider.PrepareSendToParent(ids, &resources, &context_provider);
      CHECK_EQ(resources.size(), 1u);
      extra->transferable_resource = resources[0];
    } else {
      extra->transferable_resource = viz::TransferableResource();
    }

    layer.ClearNeedsSetResourcePush();
  }
}

void SerializeScrollbarLayerBaseExtra(
    ScrollbarLayerImplBase& layer,
    viz::mojom::ScrollbarLayerBaseExtraPtr& extra) {
  extra = viz::mojom::ScrollbarLayerBaseExtra::New();
  extra->scroll_element_id = layer.scroll_element_id();
  extra->is_overlay_scrollbar = layer.is_overlay_scrollbar();
  extra->is_web_test = layer.is_web_test();
  extra->thumb_thickness_scale_factor = layer.thumb_thickness_scale_factor();
  extra->current_pos = layer.current_pos();
  extra->clip_layer_length = layer.clip_layer_length();
  extra->scroll_layer_length = layer.scroll_layer_length();
  extra->is_horizontal_orientation =
      layer.orientation() == ScrollbarOrientation::kHorizontal;
  extra->is_left_side_vertical_scrollbar =
      layer.is_left_side_vertical_scrollbar();
  extra->vertical_adjust = layer.vertical_adjust();
  extra->has_find_in_page_tickmarks = layer.has_find_in_page_tickmarks();
}

void SerializeNinePatchThumbScrollbarLayerExtra(
    NinePatchThumbScrollbarLayerImpl& layer,
    viz::mojom::NinePatchThumbScrollbarLayerExtraPtr& extra) {
  SerializeScrollbarLayerBaseExtra(static_cast<ScrollbarLayerImplBase&>(layer),
                                   extra->scrollbar_base_extra);

  extra->thumb_thickness = layer.thumb_thickness();
  extra->thumb_length = layer.thumb_length();
  extra->track_start = layer.track_start();
  extra->track_length = layer.track_length();
  extra->image_bounds = layer.image_bounds();
  extra->aperture = layer.aperture();
  extra->thumb_ui_resource_id = layer.thumb_ui_resource_id();
  extra->track_and_buttons_ui_resource_id =
      layer.track_and_buttons_ui_resource_id();
}

void SerializePaintedScrollbarLayerExtra(
    PaintedScrollbarLayerImpl& layer,
    viz::mojom::PaintedScrollbarLayerExtraPtr& extra) {
  SerializeScrollbarLayerBaseExtra(static_cast<ScrollbarLayerImplBase&>(layer),
                                   extra->scrollbar_base_extra);
  extra->internal_contents_scale = layer.internal_contents_scale();
  extra->internal_content_bounds = layer.internal_content_bounds();
  extra->jump_on_track_click = layer.jump_on_track_click();
  extra->supports_drag_snap_back = layer.supports_drag_snap_back();
  extra->thumb_thickness = layer.thumb_thickness();
  extra->thumb_length = layer.thumb_length();
  extra->back_button_rect = layer.back_button_rect();
  extra->forward_button_rect = layer.forward_button_rect();
  extra->track_rect = layer.track_rect();
  extra->track_and_buttons_ui_resource_id =
      layer.track_and_buttons_ui_resource_id();
  extra->thumb_ui_resource_id = layer.thumb_ui_resource_id();
  extra->uses_nine_patch_track_and_buttons =
      layer.uses_nine_patch_track_and_buttons();
  extra->painted_opacity = layer.painted_opacity();
  extra->thumb_color = layer.thumb_color();
  extra->track_and_buttons_image_bounds =
      layer.track_and_buttons_image_bounds();
  extra->track_and_buttons_aperture = layer.track_and_buttons_aperture();
}

void SerializeSolidColorScrollbarLayerExtra(
    SolidColorScrollbarLayerImpl& layer,
    viz::mojom::SolidColorScrollbarLayerExtraPtr& extra) {
  SerializeScrollbarLayerBaseExtra(static_cast<ScrollbarLayerImplBase&>(layer),
                                   extra->scrollbar_base_extra);
  extra->thumb_thickness = layer.thumb_thickness();
  extra->track_start = layer.track_start();
  extra->color = layer.color();
}

void SerializeViewTransitionContentLayerExtra(
    ViewTransitionContentLayerImpl& layer,
    viz::mojom::ViewTransitionContentLayerExtraPtr& extra) {
  extra->resource_id = layer.resource_id();
  extra->is_live_content_layer = layer.is_live_content_layer();
  extra->max_extents_rect = layer.max_extents_rect();
}

void SerializeSurfaceLayerExtra(SurfaceLayerImpl& layer,
                                viz::mojom::SurfaceLayerExtraPtr& extra) {
  extra->surface_range = layer.range();
  if (layer.deadline_in_frames().has_value()) {
    extra->deadline_in_frames = layer.deadline_in_frames().value();
  }
  extra->stretch_content_to_fill_bounds =
      layer.stretch_content_to_fill_bounds();
  extra->surface_hit_testable = layer.surface_hit_testable();
  extra->has_pointer_events_none = layer.has_pointer_events_none();
  extra->is_reflection = layer.is_reflection();
  extra->will_draw_needs_reset = layer.will_draw_needs_reset();
  extra->override_child_paint_flags = layer.override_child_paint_flags();
}

void SerializeLayer(LayerImpl& layer,
                    viz::ClientResourceProvider& resource_provider,
                    viz::RasterContextProvider& context_provider,
                    viz::mojom::LayerTreeUpdate& update) {
  auto& wire = *update.layers.emplace_back(viz::mojom::Layer::New());
  wire.id = layer.id();
  wire.element_id = layer.element_id();
  wire.type = layer.GetLayerType();
  wire.bounds = layer.bounds();
  wire.is_drawable = layer.draws_content();
  wire.layer_property_changed_not_from_property_trees =
      layer.LayerPropertyChangedNotFromPropertyTrees();
  wire.layer_property_changed_from_property_trees =
      layer.LayerPropertyChangedFromPropertyTrees();
  wire.contents_opaque = layer.contents_opaque();
  wire.contents_opaque_for_text = layer.contents_opaque_for_text();
  wire.hit_test_opaqueness = layer.hit_test_opaqueness();
  wire.background_color = layer.background_color();
  wire.safe_opaque_background_color = layer.safe_opaque_background_color();
  wire.update_rect = layer.update_rect();
  wire.offset_to_transform_parent = layer.offset_to_transform_parent();
  wire.transform_tree_index = layer.transform_tree_index();
  wire.clip_tree_index = layer.clip_tree_index();
  wire.effect_tree_index = layer.effect_tree_index();
  wire.scroll_tree_index = layer.scroll_tree_index();
  wire.should_check_backface_visibility =
      layer.should_check_backface_visibility();
  if (layer.HasAnyRarePropertySet()) {
    auto rare_properties = viz::mojom::RareProperties::New();
    rare_properties->filter_quality = layer.GetFilterQuality();
    rare_properties->dynamic_range_limit = layer.GetDynamicRangeLimit();

    // NOTE: If the layer's RareProperties is present, then `capture_bounds()`
    // is guaranteed to be non-null.
    rare_properties->capture_bounds = CHECK_DEREF(layer.capture_bounds());
    wire.rare_properties = std::move(rare_properties);
  }
  wire.may_contain_video = layer.may_contain_video();
  switch (layer.GetLayerType()) {
    case mojom::LayerType::kMirror: {
      auto mirror_layer_extra = viz::mojom::MirrorLayerExtra::New();
      SerializeMirrorLayerExtra(static_cast<MirrorLayerImpl&>(layer),
                                mirror_layer_extra);
      wire.layer_extra = viz::mojom::LayerExtra::NewMirrorLayerExtra(
          std::move(mirror_layer_extra));
      break;
    }
    case mojom::LayerType::kNinePatchThumbScrollbar: {
      auto nine_patch_thumb_scrollbar_layer_extra =
          viz::mojom::NinePatchThumbScrollbarLayerExtra::New();
      SerializeNinePatchThumbScrollbarLayerExtra(
          static_cast<NinePatchThumbScrollbarLayerImpl&>(layer),
          nine_patch_thumb_scrollbar_layer_extra);
      wire.layer_extra =
          viz::mojom::LayerExtra::NewNinePatchThumbScrollbarLayerExtra(
              std::move(nine_patch_thumb_scrollbar_layer_extra));
      break;
    }
    case mojom::LayerType::kPaintedScrollbar: {
      auto painted_scrollbar_layer_extra =
          viz::mojom::PaintedScrollbarLayerExtra::New();
      SerializePaintedScrollbarLayerExtra(
          static_cast<PaintedScrollbarLayerImpl&>(layer),
          painted_scrollbar_layer_extra);
      wire.layer_extra = viz::mojom::LayerExtra::NewPaintedScrollbarLayerExtra(
          std::move(painted_scrollbar_layer_extra));
      break;
    }
    case mojom::LayerType::kSolidColorScrollbar: {
      auto solid_color_scrollbar_layer_extra =
          viz::mojom::SolidColorScrollbarLayerExtra::New();
      SerializeSolidColorScrollbarLayerExtra(
          static_cast<SolidColorScrollbarLayerImpl&>(layer),
          solid_color_scrollbar_layer_extra);
      wire.layer_extra =
          viz::mojom::LayerExtra::NewSolidColorScrollbarLayerExtra(
              std::move(solid_color_scrollbar_layer_extra));
      break;
    }
    case mojom::LayerType::kSurface: {
      auto surface_layer_extra = viz::mojom::SurfaceLayerExtra::New();
      SerializeSurfaceLayerExtra(static_cast<SurfaceLayerImpl&>(layer),
                                 surface_layer_extra);
      wire.layer_extra = viz::mojom::LayerExtra::NewSurfaceLayerExtra(
          std::move(surface_layer_extra));
      break;
    }
    case mojom::LayerType::kPicture: {
      PictureLayerImpl& picture_layer = static_cast<PictureLayerImpl&>(layer);
      wire.is_backdrop_filter_mask = picture_layer.is_backdrop_filter_mask();

      if (picture_layer.GetRasterSource()->IsSolidColor()) {
        wire.solid_color = picture_layer.GetRasterSource()->GetSolidColor();
      }
      SerializePictureLayerTileUpdates(picture_layer, resource_provider,
                                       context_provider, update.tilings);
      break;
    }
    case mojom::LayerType::kTexture: {
      auto texture_layer_extra = viz::mojom::TextureLayerExtra::New();
      SerializeTextureLayerExtra(static_cast<TextureLayerImpl&>(layer),
                                 texture_layer_extra, resource_provider,
                                 context_provider);
      wire.layer_extra = viz::mojom::LayerExtra::NewTextureLayerExtra(
          std::move(texture_layer_extra));
      break;
    }
    case mojom::LayerType::kViewTransitionContent: {
      auto view_transition_content_layer_extra =
          viz::mojom::ViewTransitionContentLayerExtra::New();
      SerializeViewTransitionContentLayerExtra(
          static_cast<ViewTransitionContentLayerImpl&>(layer),
          view_transition_content_layer_extra);
      wire.layer_extra =
          viz::mojom::LayerExtra::NewViewTransitionContentLayerExtra(
              std::move(view_transition_content_layer_extra));
      break;
    }
    default:
      // TODO(zmo): handle other types of LayerImpl.
      break;
  }
}

viz::mojom::TimingStepPosition SerializeTimingStepPosition(
    gfx::StepsTimingFunction::StepPosition step_position) {
  switch (step_position) {
    case gfx::StepsTimingFunction::StepPosition::START:
      return viz::mojom::TimingStepPosition::kStart;
    case gfx::StepsTimingFunction::StepPosition::END:
      return viz::mojom::TimingStepPosition::kEnd;
    case gfx::StepsTimingFunction::StepPosition::JUMP_BOTH:
      return viz::mojom::TimingStepPosition::kJumpBoth;
    case gfx::StepsTimingFunction::StepPosition::JUMP_END:
      return viz::mojom::TimingStepPosition::kJumpEnd;
    case gfx::StepsTimingFunction::StepPosition::JUMP_NONE:
      return viz::mojom::TimingStepPosition::kJumpNone;
    case gfx::StepsTimingFunction::StepPosition::JUMP_START:
      return viz::mojom::TimingStepPosition::kJumpStart;
  }
}

viz::mojom::TimingFunctionPtr SerializeTimingFunction(
    const gfx::TimingFunction& fn) {
  viz::mojom::TimingFunctionPtr wire;
  switch (fn.GetType()) {
    case gfx::TimingFunction::Type::LINEAR: {
      const auto& linear = static_cast<const gfx::LinearTimingFunction&>(fn);
      std::vector<viz::mojom::LinearEasingPointPtr> points;
      points.reserve(linear.Points().size());
      for (const auto& point : linear.Points()) {
        points.push_back(
            viz::mojom::LinearEasingPoint::New(point.input, point.output));
      }
      wire = viz::mojom::TimingFunction::NewLinear(std::move(points));
      break;
    }
    case gfx::TimingFunction::Type::CUBIC_BEZIER: {
      const auto& bezier =
          static_cast<const gfx::CubicBezierTimingFunction&>(fn);
      auto wire_bezier = viz::mojom::CubicBezierTimingFunction::New();
      wire_bezier->x1 = bezier.bezier().GetX1();
      wire_bezier->y1 = bezier.bezier().GetY1();
      wire_bezier->x2 = bezier.bezier().GetX2();
      wire_bezier->y2 = bezier.bezier().GetY2();
      wire = viz::mojom::TimingFunction::NewCubicBezier(std::move(wire_bezier));
      break;
    }
    case gfx::TimingFunction::Type::STEPS: {
      const auto& steps = static_cast<const gfx::StepsTimingFunction&>(fn);
      auto wire_steps = viz::mojom::StepsTimingFunction::New();
      wire_steps->num_steps = base::checked_cast<uint32_t>(steps.steps());
      wire_steps->step_position =
          SerializeTimingStepPosition(steps.step_position());
      wire = viz::mojom::TimingFunction::NewSteps(std::move(wire_steps));
      break;
    }
    default:
      NOTREACHED();
  }
  return wire;
}

std::vector<viz::mojom::TransformOperationPtr> SerializeTransformOperations(
    const gfx::TransformOperations& ops) {
  std::vector<viz::mojom::TransformOperationPtr> wire_ops;
  wire_ops.reserve(ops.size());
  for (size_t i = 0; i < ops.size(); ++i) {
    const auto& op = ops.at(i);
    switch (op.type) {
      case gfx::TransformOperation::TRANSFORM_OPERATION_TRANSLATE:
        wire_ops.push_back(viz::mojom::TransformOperation::NewTranslate(
            gfx::Vector3dF(op.translate.x, op.translate.y, op.translate.z)));
        break;
      case gfx::TransformOperation::TRANSFORM_OPERATION_ROTATE:
        wire_ops.push_back(viz::mojom::TransformOperation::NewRotate(
            viz::mojom::AxisAngle::New(
                gfx::Vector3dF(op.rotate.axis.x, op.rotate.axis.y,
                               op.rotate.axis.z),
                op.rotate.angle)));
        break;
      case gfx::TransformOperation::TRANSFORM_OPERATION_SCALE:
        wire_ops.push_back(viz::mojom::TransformOperation::NewScale(
            gfx::Vector3dF(op.scale.x, op.scale.y, op.scale.z)));
        break;
      case gfx::TransformOperation::TRANSFORM_OPERATION_SKEWX:
      case gfx::TransformOperation::TRANSFORM_OPERATION_SKEWY:
      case gfx::TransformOperation::TRANSFORM_OPERATION_SKEW:
        wire_ops.push_back(viz::mojom::TransformOperation::NewSkew(
            gfx::Vector2dF(op.skew.x, op.skew.y)));
        break;
      case gfx::TransformOperation::TRANSFORM_OPERATION_PERSPECTIVE:
        wire_ops.push_back(viz::mojom::TransformOperation::NewPerspectiveDepth(
            op.perspective_m43 ? -1.0f / op.perspective_m43 : 0.0f));
        break;
      case gfx::TransformOperation::TRANSFORM_OPERATION_MATRIX:
        wire_ops.push_back(
            viz::mojom::TransformOperation::NewMatrix(op.matrix));
        break;
      case gfx::TransformOperation::TRANSFORM_OPERATION_IDENTITY:
        wire_ops.push_back(viz::mojom::TransformOperation::NewIdentity(true));
        break;
      default:
        NOTREACHED();
    }
  }
  return wire_ops;
}

template <typename KeyframeValueType>
viz::mojom::AnimationKeyframeValuePtr SerializeKeyframeValue(
    KeyframeValueType&& value) {
  using ValueType = std::remove_cvref_t<KeyframeValueType>;
  if constexpr (std::is_same_v<ValueType, float>) {
    return viz::mojom::AnimationKeyframeValue::NewScalar(value);
  } else if constexpr (std::is_same_v<ValueType, SkColor>) {
    return viz::mojom::AnimationKeyframeValue::NewColor(value);
  } else if constexpr (std::is_same_v<ValueType, gfx::SizeF>) {
    return viz::mojom::AnimationKeyframeValue::NewSize(value);
  } else if constexpr (std::is_same_v<ValueType, gfx::Rect>) {
    return viz::mojom::AnimationKeyframeValue::NewRect(value);
  } else if constexpr (std::is_same_v<ValueType, gfx::TransformOperations>) {
    return viz::mojom::AnimationKeyframeValue::NewTransform(
        SerializeTransformOperations(value));
  } else {
    static_assert(false, "Unsupported curve type");
  }
}

viz::mojom::AnimationDirection SerializeAnimationDirection(
    const KeyframeModel& model) {
  switch (model.direction()) {
    case KeyframeModel::Direction::NORMAL:
      return viz::mojom::AnimationDirection::kNormal;
    case KeyframeModel::Direction::REVERSE:
      return viz::mojom::AnimationDirection::kReverse;
    case KeyframeModel::Direction::ALTERNATE_NORMAL:
      return viz::mojom::AnimationDirection::kAlternateNormal;
    case KeyframeModel::Direction::ALTERNATE_REVERSE:
      return viz::mojom::AnimationDirection::kAlternateReverse;
  }
}

viz::mojom::AnimationFillMode SerializeAnimationFillMode(
    const KeyframeModel& model) {
  switch (model.fill_mode()) {
    case KeyframeModel::FillMode::NONE:
      return viz::mojom::AnimationFillMode::kNone;
    case KeyframeModel::FillMode::FORWARDS:
      return viz::mojom::AnimationFillMode::kForwards;
    case KeyframeModel::FillMode::BACKWARDS:
      return viz::mojom::AnimationFillMode::kBackwards;
    case KeyframeModel::FillMode::BOTH:
      return viz::mojom::AnimationFillMode::kBoth;
    case KeyframeModel::FillMode::AUTO:
      return viz::mojom::AnimationFillMode::kAuto;
  }
}
template <typename CurveType>
void SerializeAnimationCurve(const KeyframeModel& model,
                             viz::mojom::AnimationKeyframeModel& wire) {
  const auto& curve = static_cast<const CurveType&>(*model.curve());
  const auto* timing_function = curve.timing_function();
  CHECK(!curve.keyframes().empty());
  CHECK(timing_function);

  wire.timing_function = SerializeTimingFunction(*timing_function);
  wire.scaled_duration = curve.scaled_duration();
  wire.direction = SerializeAnimationDirection(model);
  wire.fill_mode = SerializeAnimationFillMode(model);
  wire.playback_rate = model.playback_rate();
  wire.iterations = model.iterations();
  wire.iteration_start = model.iteration_start();
  wire.time_offset = model.time_offset();
  wire.keyframes.reserve(curve.keyframes().size());
  for (const auto& keyframe : curve.keyframes()) {
    auto wire_keyframe = viz::mojom::AnimationKeyframe::New();
    wire_keyframe->value = SerializeKeyframeValue(keyframe->Value());
    wire_keyframe->start_time = keyframe->Time();
    if (keyframe->timing_function()) {
      wire_keyframe->timing_function =
          SerializeTimingFunction(*keyframe->timing_function());
    }
    wire.keyframes.push_back(std::move(wire_keyframe));
  }
}

viz::mojom::AnimationKeyframeModelPtr SerializeAnimationKeyframeModel(
    const KeyframeModel& model) {
  auto wire = viz::mojom::AnimationKeyframeModel::New();
  wire->id = base::checked_cast<int32_t>(model.id());
  wire->group_id = base::checked_cast<int32_t>(model.group());
  wire->target_property_type =
      base::checked_cast<int32_t>(model.TargetProperty());
  wire->element_id = model.element_id();

  switch (static_cast<gfx::AnimationCurve::CurveType>(model.curve()->Type())) {
    case gfx::AnimationCurve::COLOR:
      SerializeAnimationCurve<gfx::KeyframedColorAnimationCurve>(model, *wire);
      break;
    case gfx::AnimationCurve::FLOAT:
      SerializeAnimationCurve<gfx::KeyframedFloatAnimationCurve>(model, *wire);
      break;
    case gfx::AnimationCurve::TRANSFORM:
      SerializeAnimationCurve<gfx::KeyframedTransformAnimationCurve>(model,
                                                                     *wire);
      break;
    case gfx::AnimationCurve::SIZE:
      SerializeAnimationCurve<gfx::KeyframedSizeAnimationCurve>(model, *wire);
      break;
    case gfx::AnimationCurve::RECT:
      SerializeAnimationCurve<gfx::KeyframedRectAnimationCurve>(model, *wire);
      break;
    case gfx::AnimationCurve::FILTER:
    case gfx::AnimationCurve::SCROLL_OFFSET:
      // TODO(rockot): Support these curve types too.
      return nullptr;
    default:
      NOTREACHED();
  }
  return wire;
}

viz::mojom::AnimationPtr SerializeAnimation(cc::Animation& animation) {
  auto wire = viz::mojom::Animation::New();
  wire->id = animation.id();
  wire->element_id = animation.element_id();
  for (const auto& model : animation.keyframe_effect()->keyframe_models()) {
    auto wire_model = SerializeAnimationKeyframeModel(
        *KeyframeModel::ToCcKeyframeModel(model.get()));
    if (wire_model) {
      wire->keyframe_models.push_back(std::move(wire_model));
    }
  }
  return wire;
}

viz::mojom::ViewTransitionRequestPtr SerializeViewTransitionRequest(
    const cc::ViewTransitionRequest& request) {
  auto wire = viz::mojom::ViewTransitionRequest::New();
  switch (request.type()) {
    case cc::ViewTransitionRequest::Type::kSave:
      wire->type = viz::mojom::CompositorFrameTransitionDirectiveType::kSave;
      break;
    case cc::ViewTransitionRequest::Type::kAnimateRenderer:
      wire->type =
          viz::mojom::CompositorFrameTransitionDirectiveType::kAnimateRenderer;
      break;
    case cc::ViewTransitionRequest::Type::kRelease:
      wire->type = viz::mojom::CompositorFrameTransitionDirectiveType::kRelease;
      break;
  }
  wire->transition_token = request.token();
  wire->maybe_cross_frame_sink = request.maybe_cross_frame_sink();
  wire->sequence_id = request.sequence_id();
  if (request.type() == cc::ViewTransitionRequest::Type::kSave &&
      !request.capture_resource_ids().empty()) {
    wire->capture_resource_ids.reserve(request.capture_resource_ids().size());
    for (const auto& id : request.capture_resource_ids()) {
      wire->capture_resource_ids.push_back(id);
    }
  } else {
    DCHECK(request.capture_resource_ids().empty());
  }
  return wire;
}

}  // namespace

VizLayerContext::VizLayerContext(viz::mojom::CompositorFrameSink& frame_sink,
                                 LayerTreeHostImpl& host_impl)
    : host_impl_(host_impl) {
  auto context = viz::mojom::PendingLayerContext::New();
  context->receiver = service_.BindNewEndpointAndPassReceiver();
  context->client = client_receiver_.BindNewEndpointAndPassRemote();
  bool draw_mode_is_gpu = host_impl.GetDrawMode() == DRAW_MODE_HARDWARE;
  frame_sink.BindLayerContext(std::move(context), draw_mode_is_gpu);
}

VizLayerContext::~VizLayerContext() = default;

void VizLayerContext::SetVisible(bool visible) {
  service_->SetVisible(visible);
}

void VizLayerContext::UpdateDisplayTreeFrom(
    LayerTreeImpl& tree,
    viz::ClientResourceProvider& resource_provider,
    viz::RasterContextProvider& context_provider,
    const gfx::Rect& viewport_damage_rect) {
  auto& property_trees = *tree.property_trees();
  auto update = viz::mojom::LayerTreeUpdate::New();
  update->begin_frame_args = tree.CurrentBeginFrameArgs();
  update->source_frame_number = tree.source_frame_number();
  update->trace_id = tree.trace_id().value();
  update->primary_main_frame_item_sequence_number =
      tree.primary_main_frame_item_sequence_number();
  update->page_scale_factor = tree.page_scale_factor()->Current(true);
  update->min_page_scale_factor = tree.min_page_scale_factor();
  update->max_page_scale_factor = tree.max_page_scale_factor();
  update->external_page_scale_factor = tree.external_page_scale_factor();
  update->device_viewport = tree.GetDeviceViewport();
  update->device_scale_factor = tree.device_scale_factor();
  update->painted_device_scale_factor = tree.painted_device_scale_factor();
  update->display_color_spaces = tree.display_color_spaces();
  if (tree.local_surface_id_from_parent().is_valid()) {
    update->local_surface_id_from_parent = tree.local_surface_id_from_parent();
  }
  update->background_color = tree.background_color();

  const ViewportPropertyIds& property_ids = tree.viewport_property_ids();
  update->overscroll_elasticity_transform =
      property_ids.overscroll_elasticity_transform;
  update->page_scale_transform = property_ids.page_scale_transform;
  update->inner_scroll = property_ids.inner_scroll;
  update->outer_clip = property_ids.outer_clip;
  update->outer_scroll = property_ids.outer_scroll;

  update->viewport_damage_rect = viewport_damage_rect;

  // Sync changes to UI resources
  {
    auto resource_changes = host_impl_->TakeUIResourceChanges(needs_full_sync_);
    for (const auto& [uid, change] : resource_changes) {
      if (change.resource_deleted) {
        SerializeUIResourceRequest(
            *host_impl_, context_provider, *update, uid,
            viz::mojom::TransferableUIResourceRequest::Type::kDelete);
      }
      if (change.resource_created) {
        SerializeUIResourceRequest(
            *host_impl_, context_provider, *update, uid,
            viz::mojom::TransferableUIResourceRequest::Type::kCreate);
      }
    }
  }

  // This flag will be set if and only if a new layer list was pushed to the
  // active tree during activation, implying that at least one layer addition or
  // removal happened since our last update. In this case only, we push the full
  // ordered list of layer IDs.
  if (tree.needs_full_tree_sync() || needs_full_sync_) {
    update->layer_order.emplace();
    update->layer_order->reserve(tree.NumLayers());
    for (LayerImpl* layer : tree) {
      update->layer_order->push_back(layer->id());
    }
    tree.set_needs_full_tree_sync(false);
  }

  if (needs_full_sync_) {
    for (LayerImpl* layer : tree) {
      SerializeLayer(*layer, resource_provider, context_provider, *update);
    }
  } else {
    for (LayerImpl* layer : tree.LayersThatShouldPushProperties()) {
      SerializeLayer(*layer, resource_provider, context_provider, *update);
    }
  }
  tree.ClearLayersThatShouldPushProperties();

  // TODO(rockot): Granular change tracking for property trees, so we aren't
  // diffing every time.
  PropertyTrees& old_trees = last_committed_property_trees_;
  ComputePropertyTreeUpdate(
      old_trees.transform_tree(), property_trees.transform_tree(),
      update->transform_nodes, update->num_transform_nodes);
  ComputePropertyTreeUpdate(old_trees.clip_tree(), property_trees.clip_tree(),
                            update->clip_nodes, update->num_clip_nodes);
  ComputeEffectTreeUpdate(old_trees.effect_tree(),
                          property_trees.effect_tree_mutable(),
                          update->effect_nodes, update->num_effect_nodes);
  ComputePropertyTreeUpdate(old_trees.scroll_tree(),
                            property_trees.scroll_tree(), update->scroll_nodes,
                            update->num_scroll_nodes);
  update->transform_tree_update = ComputeTransformTreePropertiesUpdate(
      old_trees.transform_tree(), property_trees.transform_tree());

  update->scroll_tree_update = ComputeScrollTreePropertiesUpdate(
      old_trees.scroll_tree(), property_trees.scroll_tree());

  last_committed_property_trees_ = property_trees;

  if (tree.needs_surface_ranges_sync() || needs_full_sync_) {
    update->surface_ranges.emplace();
    update->surface_ranges->reserve(tree.SurfaceRanges().size());
    for (const auto& surface_range : tree.SurfaceRanges()) {
      update->surface_ranges->push_back(surface_range);
    }
    tree.set_needs_surface_ranges_sync(false);
  }
  if (tree.HasViewTransitionRequests()) {
    auto requests = tree.TakeViewTransitionRequests(
        /*should_set_needs_update_draw_properties=*/false);
    update->view_transition_requests.emplace();
    update->view_transition_requests->reserve(requests.size());
    for (const auto& request : requests) {
      auto data = SerializeViewTransitionRequest(*request);
      update->view_transition_requests->push_back(std::move(data));
    }
  }

  if (base::FeatureList::IsEnabled(features::kTreeAnimationsInViz)) {
    SerializeAnimationUpdates(tree, *update);
  }
  service_->UpdateDisplayTree(std::move(update));

  needs_full_sync_ = false;
}

// Sends a single-tile update to the Viz service by serializing it as a tiling.
void VizLayerContext::UpdateDisplayTile(
    PictureLayerImpl& layer,
    const Tile& tile,
    viz::ClientResourceProvider& resource_provider,
    viz::RasterContextProvider& context_provider,
    bool update_damage) {
  // Create a one-element update list for the given tile.
  TileIndex index(tile.tiling_i_index(), tile.tiling_j_index());
  const Tile* tile_ptr = &tile;
  std::pair<TileIndex, const Tile*> tile_updates[] = {{index, tile_ptr}};

  // Serialize the tile and send it to the display service.
  if (auto tiling =
          SerializeTiling(layer, tile.tiling(), tile.contents_scale_key(),
                          tile_updates, resource_provider, context_provider)) {
    service_->UpdateDisplayTiling(std::move(tiling), update_damage);
  }
}

void VizLayerContext::OnRequestCommitForFrame(const viz::BeginFrameArgs& args) {
}

void VizLayerContext::SerializeAnimationUpdates(
    LayerTreeImpl& tree,
    viz::mojom::LayerTreeUpdate& update) {
  // Safe downcast: AnimationHost is the only subclass of MutatorHost.
  AnimationHost* const animation_host =
      static_cast<AnimationHost*>(tree.mutator_host());
  CHECK(animation_host);
  if (!animation_host->needs_push_properties()) {
    return;
  }

  animation_host->ResetNeedsPushProperties();

  const auto& current_timelines = animation_host->timelines();
  auto& pushed_timelines = pushed_animation_timelines_;
  std::vector<int32_t> removed_timelines;
  for (auto it = pushed_timelines.begin(); it != pushed_timelines.end();) {
    if (!base::Contains(current_timelines, it->first)) {
      removed_timelines.push_back(it->first);
      it = pushed_timelines.erase(it);
    } else {
      ++it;
    }
  }
  if (!removed_timelines.empty()) {
    update.removed_animation_timelines = std::move(removed_timelines);
  }
  std::vector<viz::mojom::AnimationTimelinePtr> timelines;
  for (const auto& [id, timeline] : current_timelines) {
    if (auto wire = MaybeSerializeAnimationTimeline(*timeline)) {
      timelines.push_back(std::move(wire));
    }
  }
  if (!timelines.empty()) {
    update.animation_timelines = std::move(timelines);
  }
}

viz::mojom::AnimationTimelinePtr
VizLayerContext::MaybeSerializeAnimationTimeline(
    cc::AnimationTimeline& timeline) {
  const auto& current_animations = timeline.animations();
  auto& pushed_animations = pushed_animation_timelines_[timeline.id()];
  std::vector<int32_t> removed_animations;
  for (auto it = pushed_animations.begin(); it != pushed_animations.end();) {
    if (!base::Contains(current_animations, *it)) {
      removed_animations.push_back(*it);
      it = pushed_animations.erase(it);
    } else {
      ++it;
    }
  }

  std::vector<viz::mojom::AnimationPtr> new_animations;
  for (const auto& [id, animation] : current_animations) {
    if (pushed_animations.insert(animation->id()).second) {
      new_animations.push_back(SerializeAnimation(*animation));
    }
  }

  if (removed_animations.empty() && new_animations.empty()) {
    return nullptr;
  }

  auto wire = viz::mojom::AnimationTimeline::New();
  wire->id = timeline.id();
  wire->new_animations = std::move(new_animations);
  wire->removed_animations = std::move(removed_animations);
  return wire;
}

}  // namespace cc::mojo_embedder
