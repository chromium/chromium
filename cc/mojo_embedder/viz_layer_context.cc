// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojo_embedder/viz_layer_context.h"

#include <cstdint>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/numerics/safe_conversions.h"
#include "cc/layers/layer_impl.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree.h"
#include "services/viz/public/mojom/compositing/layer.mojom.h"
#include "services/viz/public/mojom/compositing/layer_context.mojom.h"

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
      old_node->sticky_position_constraint_id ==
          new_node.sticky_position_constraint_id &&
      old_node->anchor_position_scroll_data_id ==
          new_node.anchor_position_scroll_data_id &&
      old_node->sorting_context_id == new_node.sorting_context_id &&
      old_node->scroll_offset == new_node.scroll_offset &&
      old_node->snap_amount == new_node.snap_amount &&
      old_node->needs_local_transform_update ==
          new_node.needs_local_transform_update &&
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
  if (new_node.sticky_position_constraint_id >= 0) {
    wire->sticky_position_constraint_id =
        base::checked_cast<uint32_t>(new_node.sticky_position_constraint_id);
  }
  if (new_node.anchor_position_scroll_data_id >= 0) {
    wire->anchor_position_scroll_data_id =
        base::checked_cast<uint32_t>(new_node.anchor_position_scroll_data_id);
  }
  wire->sorting_context_id = new_node.sorting_context_id;
  wire->scroll_offset = new_node.scroll_offset;
  wire->snap_amount = new_node.snap_amount;
  wire->needs_local_transform_update = new_node.needs_local_transform_update;
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
    std::vector<viz::mojom::EffectNodePtr>& container) {
  if (old_node && old_node->id == new_node.id &&
      old_node->parent_id == new_node.parent_id &&
      old_node->transform_id == new_node.transform_id &&
      old_node->clip_id == new_node.clip_id &&
      old_node->element_id == new_node.element_id &&
      old_node->opacity == new_node.opacity &&
      old_node->render_surface_reason == new_node.render_surface_reason &&
      old_node->surface_contents_scale == new_node.surface_contents_scale &&
      old_node->blend_mode == new_node.blend_mode &&
      old_node->target_id == new_node.target_id) {
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
  wire->blend_mode = base::checked_cast<uint32_t>(new_node.blend_mode);
  wire->target_id = new_node.target_id;
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
  wire->is_premultiplied = draw_info.is_premultiplied();
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
          viz::mojom::MissingTileReason::kOutOfMemory);
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
            viz::mojom::MissingTileReason::kResourceNotReady);
      }
      break;
  }
  return wire;
}

viz::mojom::TilingPtr SerializeTiling(
    PictureLayerImpl& layer,
    const PictureLayerTiling& tiling,
    base::span<const Tile*> tiles,
    viz::ClientResourceProvider& resource_provider,
    viz::RasterContextProvider& context_provider) {
  std::vector<viz::mojom::TilePtr> wire_tiles;
  for (const Tile* tile : tiles) {
    if (auto wire_tile =
            SerializeTile(*tile, resource_provider, context_provider)) {
      wire_tiles.push_back(std::move(wire_tile));
    }
  }
  if (wire_tiles.empty()) {
    return nullptr;
  }

  auto wire = viz::mojom::Tiling::New();
  wire->layer_id = layer.id();
  wire->raster_translation = tiling.raster_transform().translation();
  wire->raster_scale = tiling.raster_transform().scale();
  wire->tile_size = tiling.tile_size();
  wire->tiling_rect = tiling.tiling_rect();
  wire->tiles = std::move(wire_tiles);
  return wire;
}

void SerializePictureLayerTileUpdates(
    PictureLayerImpl& layer,
    viz::ClientResourceProvider& resource_provider,
    viz::RasterContextProvider& context_provider,
    std::vector<viz::mojom::TilingPtr>& tilings) {
  auto updates = layer.TakeUpdatedTiles();
  for (const auto& [scale_key, tile_indices] : updates) {
    if (const auto* tiling =
            layer.picture_layer_tiling_set()->FindTilingWithScaleKey(
                scale_key)) {
      std::vector<const Tile*> tiles;
      tiles.reserve(tile_indices.size());
      for (const auto& index : tile_indices) {
        if (auto* tile = tiling->TileAt(index)) {
          tiles.push_back(tile);
        }
      }

      if (auto wire_tiling = SerializeTiling(
              layer, *tiling, tiles, resource_provider, context_provider)) {
        tilings.push_back(std::move(wire_tiling));
      }
    }
  }
}

void SerializeLayer(LayerImpl& layer,
                    viz::ClientResourceProvider& resource_provider,
                    viz::RasterContextProvider& context_provider,
                    viz::mojom::LayerTreeUpdate& update) {
  auto& wire = *update.layers.emplace_back(viz::mojom::Layer::New());
  wire.id = layer.id();
  wire.type = layer.GetLayerType();
  wire.bounds = layer.bounds();
  wire.is_drawable = layer.draws_content();
  wire.contents_opaque = layer.contents_opaque();
  wire.contents_opaque_for_text = layer.contents_opaque_for_text();
  wire.background_color = layer.background_color();
  wire.safe_opaque_background_color = layer.safe_opaque_background_color();
  wire.update_rect = layer.update_rect();
  wire.offset_to_transform_parent = layer.offset_to_transform_parent();
  wire.transform_tree_index = layer.transform_tree_index();
  wire.clip_tree_index = layer.clip_tree_index();
  wire.effect_tree_index = layer.effect_tree_index();
  wire.scroll_tree_index = layer.scroll_tree_index();
  if (layer.GetLayerType() == mojom::LayerType::kPicture) {
    SerializePictureLayerTileUpdates(static_cast<PictureLayerImpl&>(layer),
                                     resource_provider, context_provider,
                                     update.tilings);
  }
}

}  // namespace

VizLayerContext::VizLayerContext(viz::mojom::CompositorFrameSink& frame_sink,
                                 LayerTreeHostImpl& host_impl)
    : host_impl_(host_impl) {
  auto context = viz::mojom::PendingLayerContext::New();
  context->receiver = service_.BindNewEndpointAndPassReceiver();
  context->client = client_receiver_.BindNewEndpointAndPassRemote();
  frame_sink.BindLayerContext(std::move(context));
}

VizLayerContext::~VizLayerContext() = default;

void VizLayerContext::SetVisible(bool visible) {
  service_->SetVisible(visible);
}

void VizLayerContext::UpdateDisplayTreeFrom(
    LayerTreeImpl& tree,
    viz::ClientResourceProvider& resource_provider,
    viz::RasterContextProvider& context_provider) {
  auto& property_trees = *tree.property_trees();
  auto update = viz::mojom::LayerTreeUpdate::New();
  update->source_frame_number = tree.source_frame_number();
  update->trace_id = tree.trace_id().value();
  update->device_viewport = tree.GetDeviceViewport();
  update->device_scale_factor = tree.painted_device_scale_factor();
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

  // This flag will be set if and only if a new layer list was pushed to the
  // active tree during activation, implying that at least one layer addition or
  // removal happened since our last update. In this case only, we push the full
  // ordered list of layer IDs.
  if (tree.needs_full_tree_sync()) {
    update->layer_order.emplace();
    update->layer_order->reserve(tree.NumLayers());
    for (LayerImpl* layer : tree) {
      update->layer_order->push_back(layer->id());
    }
  }

  for (LayerImpl* layer : tree.LayersThatShouldPushProperties()) {
    SerializeLayer(*layer, resource_provider, context_provider, *update);
  }

  // TODO(rockot): Granular change tracking for property trees, so we aren't
  // diffing every time.
  PropertyTrees& old_trees = last_committed_property_trees_;
  ComputePropertyTreeUpdate(
      old_trees.transform_tree(), property_trees.transform_tree(),
      update->transform_nodes, update->num_transform_nodes);
  ComputePropertyTreeUpdate(old_trees.clip_tree(), property_trees.clip_tree(),
                            update->clip_nodes, update->num_clip_nodes);
  ComputePropertyTreeUpdate(old_trees.effect_tree(),
                            property_trees.effect_tree(), update->effect_nodes,
                            update->num_effect_nodes);
  ComputePropertyTreeUpdate(old_trees.scroll_tree(),
                            property_trees.scroll_tree(), update->scroll_nodes,
                            update->num_scroll_nodes);
  update->transform_tree_update = ComputeTransformTreePropertiesUpdate(
      old_trees.transform_tree(), property_trees.transform_tree());

  last_committed_property_trees_ = property_trees;

  service_->UpdateDisplayTree(std::move(update));
}

void VizLayerContext::UpdateDisplayTile(
    PictureLayerImpl& layer,
    const Tile& tile,
    viz::ClientResourceProvider& resource_provider,
    viz::RasterContextProvider& context_provider) {
  const Tile* tiles[] = {&tile};
  if (auto tiling = SerializeTiling(layer, *tile.tiling(), tiles,
                                    resource_provider, context_provider)) {
    service_->UpdateDisplayTiling(std::move(tiling));
  }
}

void VizLayerContext::OnRequestCommitForFrame(const viz::BeginFrameArgs& args) {
}

}  // namespace cc::mojo_embedder
