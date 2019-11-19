// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PROPERTY_TREE_TEST_UTILS_H_
#define CC_TEST_PROPERTY_TREE_TEST_UTILS_H_

#include "cc/trees/clip_node.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"
#include "ui/gfx/geometry/scroll_offset.h"

namespace cc {

class Layer;
class LayerImpl;
class PictureLayer;
class PictureLayerImpl;

// Sets up properties that apply to the root layer.
void SetupRootProperties(Layer* root);
void SetupRootProperties(LayerImpl* root);

// Copies property tree indexes from |from| to |to|. For the |Layer| form, also
// copies |from|'s layer_host_host and property_tree_sequence_number to |to|.
void CopyProperties(const Layer* from, Layer* to);
void CopyProperties(const LayerImpl* from, LayerImpl* to);

// Each of the following functions creates a property node for the layer,
// and sets the new node as the layer's property node of the type.
// The new property node's parent will be |parent_id| if it's specified.
// Otherwise the layer's current property node of the corresponding type will
// be the parent. The latter case is useful to create property nodes after
// CopyProperties() under the copied properties.
TransformNode& CreateTransformNode(
    Layer*,
    int parent_id = TransformTree::kInvalidNodeId);
TransformNode& CreateTransformNode(
    LayerImpl*,
    int parent_id = TransformTree::kInvalidNodeId);
ClipNode& CreateClipNode(Layer*, int parent_id = ClipTree::kInvalidNodeId);
ClipNode& CreateClipNode(LayerImpl*, int parent_id = ClipTree::kInvalidNodeId);
EffectNode& CreateEffectNode(Layer*,
                             int parent_id = EffectTree::kInvalidNodeId);
EffectNode& CreateEffectNode(LayerImpl*,
                             int parent_id = EffectTree::kInvalidNodeId);
ScrollNode& CreateScrollNode(Layer*,
                             int parent_id = ScrollTree::kInvalidNodeId);
ScrollNode& CreateScrollNode(LayerImpl*,
                             int parent_id = ScrollTree::kInvalidNodeId);

// These functions create property nodes not associated with layers.
TransformNode& CreateTransformNode(PropertyTrees*, int parent_id);
ClipNode& CreateClipNode(PropertyTrees*, int parent_id, int transform_id);
EffectNode& CreateEffectNode(PropertyTrees*,
                             int parent_id,
                             int transform_id,
                             int clip_id);

void SetupMaskProperties(LayerImpl* masked_layer, PictureLayerImpl* mask_layer);
void SetupMaskProperties(Layer* masked_layer, PictureLayer* mask_layer);

PropertyTrees* GetPropertyTrees(const Layer* layer);
PropertyTrees* GetPropertyTrees(const LayerImpl* layer);

template <typename LayerType>
TransformNode* GetTransformNode(const LayerType* layer) {
  return GetPropertyTrees(layer)->transform_tree.Node(
      layer->transform_tree_index());
}
template <typename LayerType>
ClipNode* GetClipNode(const LayerType* layer) {
  return GetPropertyTrees(layer)->clip_tree.Node(layer->clip_tree_index());
}
template <typename LayerType>
EffectNode* GetEffectNode(const LayerType* layer) {
  return GetPropertyTrees(layer)->effect_tree.Node(layer->effect_tree_index());
}
template <typename LayerType>
ScrollNode* GetScrollNode(const LayerType* layer) {
  return GetPropertyTrees(layer)->scroll_tree.Node(layer->scroll_tree_index());
}

void SetScrollOffset(Layer*, const gfx::ScrollOffset&);
void SetScrollOffset(LayerImpl*, const gfx::ScrollOffset&);

template <typename LayerType>
void SetLocalTransformChanged(const LayerType* layer) {
  DCHECK(layer->has_transform_node());
  auto* transform_node = GetTransformNode(layer);
  transform_node->needs_local_transform_update = true;
  transform_node->transform_changed = true;
  GetPropertyTrees(layer)->transform_tree.set_needs_update(true);
}

template <typename LayerType>
void SetTransform(const LayerType* layer, const gfx::Transform& transform) {
  GetTransformNode(layer)->local = transform;
  SetLocalTransformChanged(layer);
}

template <typename LayerType>
void SetTransformOrigin(const LayerType* layer, const gfx::Point3F& origin) {
  GetTransformNode(layer)->origin = origin;
  SetLocalTransformChanged(layer);
}

template <typename LayerType>
void SetPostTranslation(const LayerType* layer,
                        const gfx::Vector2dF& post_translation) {
  GetTransformNode(layer)->post_translation = post_translation;
  SetLocalTransformChanged(layer);
}

// This will affect all layers associated with this layer's effect node.
template <typename LayerType>
void SetOpacity(const LayerType* layer, float opacity) {
  auto* effect_node = GetEffectNode(layer);
  effect_node->opacity = opacity;
  effect_node->effect_changed = true;
  GetPropertyTrees(layer)->effect_tree.set_needs_update(true);
}

// This will affect all layers associated with this layer's effect node.
template <typename LayerType>
void SetFilter(const LayerType* layer, const FilterOperations& filters) {
  auto* effect_node = GetEffectNode(layer);
  effect_node->filters = filters;
  effect_node->effect_changed = true;
  GetPropertyTrees(layer)->effect_tree.set_needs_update(true);
}

// This will affect all layers associated with this layer's effect node.
template <typename LayerType>
void SetRenderSurfaceReason(const LayerType* layer,
                            RenderSurfaceReason reason) {
  auto* effect_node = GetEffectNode(layer);
  effect_node->render_surface_reason = reason;
  effect_node->effect_changed = true;
  GetPropertyTrees(layer)->effect_tree.set_needs_update(true);
}

// This will affect all layers associated with this layer's effect node.
template <typename LayerType>
void SetBackdropFilter(const LayerType* layer,
                       const FilterOperations& filters) {
  auto* effect_node = GetEffectNode(layer);
  effect_node->backdrop_filters = filters;
  effect_node->effect_changed = true;
  GetPropertyTrees(layer)->effect_tree.set_needs_update(true);
}

// This will affect all layers associated with this layer's clip node.
template <typename LayerType>
void SetClipRect(const LayerType* layer, const gfx::RectF& clip) {
  auto* clip_node = GetClipNode(layer);
  clip_node->clip = clip;
  GetPropertyTrees(layer)->clip_tree.set_needs_update(true);
}

// Creates viewport layers and (in layer list mode) paint properties.
// Convenient overload of the method below that creates a scrolling layer as
// the outer viewport scroll layer. The inner viewport size will be
// root->bounds().
void SetupViewport(Layer* root,
                   const gfx::Size& outer_viewport_size,
                   const gfx::Size& content_size);
// Creates viewport layers and (in layer list mode) paint properties.
// Uses the given scroll layer as the content "outer viewport scroll layer".
void SetupViewport(Layer* root,
                   scoped_refptr<Layer> outer_viewport_scroll_layer,
                   const gfx::Size& outer_viewport_size);

// The impl-side counterpart of the first version of SetupViewport().
void SetupViewport(LayerImpl* root,
                   const gfx::Size& outer_viewport_size,
                   const gfx::Size& content_size);

// Returns the RenderSurfaceImpl into which the given layer draws.
RenderSurfaceImpl* GetRenderSurface(const LayerImpl* layer);

}  // namespace cc

#endif  // CC_TEST_PROPERTY_TREE_TEST_UTILS_H_
