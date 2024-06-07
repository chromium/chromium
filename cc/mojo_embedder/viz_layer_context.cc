// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojo_embedder/viz_layer_context.h"

#include <cstdint>
#include <utility>
#include <vector>

#include "base/numerics/safe_conversions.h"
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
      old_node->scroll_offset == new_node.scroll_offset &&
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
  wire->scroll_offset = new_node.scroll_offset;
  wire->visible_frame_element_id = new_node.visible_frame_element_id;
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

void VizLayerContext::UpdateDisplayTreeFrom(LayerTreeImpl& tree) {
  auto& property_trees = *tree.property_trees();
  auto update = viz::mojom::LayerTreeUpdate::New();
  update->source_frame_number = tree.source_frame_number();
  update->trace_id = tree.trace_id();
  update->device_viewport = tree.GetDeviceViewport();
  update->device_scale_factor = tree.painted_device_scale_factor();
  if (tree.local_surface_id_from_parent().is_valid()) {
    update->local_surface_id_from_parent = tree.local_surface_id_from_parent();
  }
  update->background_color = tree.background_color();

  // TODO(rockot): Update clip, effect, and scroll trees. Also granular change
  // tracking so we aren't diffing trees every time.
  PropertyTrees& old_trees = last_committed_property_trees_;
  ComputePropertyTreeUpdate(
      old_trees.transform_tree(), property_trees.transform_tree(),
      update->transform_nodes, update->num_transform_nodes);
  last_committed_property_trees_ = property_trees;

  service_->UpdateDisplayTree(std::move(update));
}

void VizLayerContext::OnRequestCommitForFrame(const viz::BeginFrameArgs& args) {
}

}  // namespace cc::mojo_embedder
