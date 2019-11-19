// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/property_tree_test_utils.h"

#include "cc/layers/picture_layer.h"
#include "cc/layers/picture_layer_impl.h"
#include "cc/trees/clip_node.h"
#include "cc/trees/effect_node.h"
#include "cc/trees/layer_tree_host.h"
#include "cc/trees/layer_tree_impl.h"
#include "cc/trees/property_tree.h"
#include "cc/trees/scroll_node.h"
#include "cc/trees/transform_node.h"

namespace cc {

namespace {

template <typename LayerType>
void SetupRootPropertiesInternal(LayerType* root) {
  root->set_property_tree_sequence_number(
      GetPropertyTrees(root)->sequence_number);
  root->SetElementId(LayerIdToElementIdForTesting(root->id()));

  auto& root_transform_node =
      CreateTransformNode(root, TransformTree::kRootNodeId);
  DCHECK_EQ(root_transform_node.id, TransformTree::kContentsRootNodeId);

  auto& root_clip_node = CreateClipNode(root, ClipTree::kRootNodeId);
  DCHECK_EQ(root_clip_node.id, ClipTree::kViewportNodeId);
  root_clip_node.clip = gfx::RectF(gfx::SizeF(root->bounds()));
  // Root clip is in the real root transform space instead of the root layer's
  // transform space.
  root_clip_node.transform_id = TransformTree::kRootNodeId;

  auto& root_effect_node = CreateEffectNode(root, EffectTree::kRootNodeId);
  DCHECK_EQ(root_effect_node.id, EffectTree::kContentsRootNodeId);
  root_effect_node.render_surface_reason = RenderSurfaceReason::kRoot;
  // Root effect is in the real root transform space instead of the root layer's
  // transform space.
  root_effect_node.transform_id = TransformTree::kRootNodeId;

  auto& root_scroll_node = CreateScrollNode(root, ScrollTree::kRootNodeId);
  DCHECK_EQ(root_scroll_node.id, ScrollTree::kSecondaryRootNodeId);
}

template <typename LayerType>
void CopyPropertiesInternal(const LayerType* from, LayerType* to) {
  to->SetTransformTreeIndex(from->transform_tree_index());
  to->SetClipTreeIndex(from->clip_tree_index());
  to->SetEffectTreeIndex(from->effect_tree_index());
  to->SetScrollTreeIndex(from->scroll_tree_index());
}

// We use a macro instead of a function to avoid |default_id| (which is
// |layer->xxx_tree_index()|) from being evaluated when it's not used because
// |layer| may be null when |id| is valid.
#define ID_OR_DEFAULT(id, default_id) \
  ((id) == TransformTree::kInvalidNodeId ? (default_id) : (id))

template <typename LayerType>
TransformNode& CreateTransformNodeInternal(LayerType* layer,
                                           PropertyTrees* property_trees,
                                           int parent_id) {
  auto& transform_tree = property_trees->transform_tree;
  int id = transform_tree.Insert(
      TransformNode(), ID_OR_DEFAULT(parent_id, layer->transform_tree_index()));
  auto* node = transform_tree.Node(id);
  if (layer) {
    layer->SetTransformTreeIndex(id);
    layer->SetHasTransformNode(true);
    node->element_id = layer->element_id();
    if (node->element_id) {
      property_trees->element_id_to_transform_node_index[node->element_id] =
          node->id;
    }
  }
  if (const auto* parent_node = transform_tree.Node(node->parent_id)) {
    node->in_subtree_of_page_scale_layer =
        parent_node->in_subtree_of_page_scale_layer;
  }
  transform_tree.set_needs_update(true);
  return *node;
}

template <typename LayerType>
ClipNode& CreateClipNodeInternal(
    LayerType* layer,
    PropertyTrees* property_trees,
    int parent_id,
    int transform_id = TransformTree::kInvalidNodeId) {
  auto& clip_tree = property_trees->clip_tree;
  int id = clip_tree.Insert(ClipNode(),
                            ID_OR_DEFAULT(parent_id, layer->clip_tree_index()));
  auto* node = clip_tree.Node(id);
  node->clip_type = ClipNode::ClipType::APPLIES_LOCAL_CLIP;
  node->transform_id =
      ID_OR_DEFAULT(transform_id, layer->transform_tree_index());
  if (layer) {
    layer->SetClipTreeIndex(id);
    node->clip = gfx::RectF(
        gfx::PointAtOffsetFromOrigin(layer->offset_to_transform_parent()),
        gfx::SizeF(layer->bounds()));
  }
  clip_tree.set_needs_update(true);
  return *node;
}

template <typename LayerType>
EffectNode& CreateEffectNodeInternal(
    LayerType* layer,
    PropertyTrees* property_trees,
    int parent_id,
    int transform_id = TransformTree::kInvalidNodeId,
    int clip_id = ClipTree::kInvalidNodeId) {
  auto& effect_tree = property_trees->effect_tree;
  int id = effect_tree.Insert(
      EffectNode(), ID_OR_DEFAULT(parent_id, layer->effect_tree_index()));
  auto* node = effect_tree.Node(id);
  if (layer) {
    layer->SetEffectTreeIndex(id);
    node->stable_id = layer->id();
    if (layer->element_id()) {
      property_trees->element_id_to_effect_node_index[layer->element_id()] =
          node->id;
    }
  }
  node->transform_id =
      ID_OR_DEFAULT(transform_id, layer->transform_tree_index());
  node->clip_id = ID_OR_DEFAULT(clip_id, layer->clip_tree_index());
  effect_tree.set_needs_update(true);
  return *node;
}

template <typename LayerType>
ScrollNode& CreateScrollNodeInternal(LayerType* layer, int parent_id) {
  auto* property_trees = GetPropertyTrees(layer);
  auto& scroll_tree = property_trees->scroll_tree;
  int id = scroll_tree.Insert(
      ScrollNode(), ID_OR_DEFAULT(parent_id, layer->scroll_tree_index()));
  layer->SetScrollTreeIndex(id);
  auto* node = scroll_tree.Node(id);
  node->element_id = layer->element_id();
  if (node->element_id) {
    property_trees->element_id_to_scroll_node_index[node->element_id] =
        node->id;
  }
  node->container_bounds = layer->scroll_container_bounds();
  node->bounds = layer->bounds();
  node->scrollable = layer->scrollable();
  node->user_scrollable_horizontal = true;
  node->user_scrollable_vertical = true;

  DCHECK(layer->has_transform_node());
  node->transform_id = layer->transform_tree_index();
  auto* transform_node = GetTransformNode(layer);
  transform_node->should_be_snapped = true;
  transform_node->scrolls = true;

  scroll_tree.SetScrollOffset(layer->element_id(), gfx::ScrollOffset());
  scroll_tree.set_needs_update(true);
  return *node;
}

template <typename LayerType, typename MaskLayerType>
void SetupMaskPropertiesInternal(LayerType* masked_layer,
                                 MaskLayerType* mask_layer) {
  if (!GetEffectNode(masked_layer)->backdrop_filters.IsEmpty())
    mask_layer->SetIsBackdropFilterMask(true);
  mask_layer->SetBounds(masked_layer->bounds());
  auto* masked_effect = GetEffectNode(masked_layer);
  masked_effect->render_surface_reason = RenderSurfaceReason::kMask;
  masked_effect->has_masking_child = true;
  if (!masked_effect->backdrop_filters.IsEmpty()) {
    if (!mask_layer->element_id())
      mask_layer->SetElementId(LayerIdToElementIdForTesting(mask_layer->id()));
    masked_effect->backdrop_mask_element_id = mask_layer->element_id();
  }

  CopyProperties(masked_layer, mask_layer);
  mask_layer->SetOffsetToTransformParent(
      masked_layer->offset_to_transform_parent());
  CreateEffectNode(mask_layer).blend_mode = SkBlendMode::kDstIn;
}

template <typename LayerType>
void SetScrollOffsetInternal(LayerType* layer,
                             const gfx::ScrollOffset& scroll_offset) {
  DCHECK(layer->has_transform_node());
  auto* transform_node = GetTransformNode(layer);
  transform_node->scroll_offset = scroll_offset;
  SetLocalTransformChanged(layer);
  GetPropertyTrees(layer)->scroll_tree.SetScrollOffset(layer->element_id(),
                                                       scroll_offset);
}

// TODO(wangxianzhu): Viewport properties can exist without layers, but for now
// it's more convenient to create properties based on layers.
template <typename LayerType>
LayerTreeHost::ViewportPropertyIds SetupViewportProperties(
    LayerType* root,
    LayerType* inner_viewport_scroll_layer,
    LayerType* outer_viewport_scroll_layer) {
  LayerTreeHost::ViewportPropertyIds viewport_property_ids;
  auto* property_trees = GetPropertyTrees(root);

  viewport_property_ids.overscroll_elasticity_transform =
      CreateTransformNode(property_trees, root->transform_tree_index()).id;

  auto& page_scale_transform = CreateTransformNode(
      property_trees, viewport_property_ids.overscroll_elasticity_transform);
  page_scale_transform.in_subtree_of_page_scale_layer = true;
  viewport_property_ids.page_scale_transform = page_scale_transform.id;

  CopyProperties(root, inner_viewport_scroll_layer);
  CreateTransformNode(inner_viewport_scroll_layer,
                      viewport_property_ids.page_scale_transform);
  auto& inner_scroll = CreateScrollNode(inner_viewport_scroll_layer);
  inner_scroll.scrolls_inner_viewport = true;
  inner_scroll.max_scroll_offset_affected_by_page_scale = true;
  viewport_property_ids.inner_scroll = inner_scroll.id;

  auto& outer_clip = CreateClipNode(property_trees, root->clip_tree_index(),
                                    inner_scroll.transform_id);
  outer_clip.clip =
      gfx::RectF(gfx::SizeF(inner_viewport_scroll_layer->bounds()));
  viewport_property_ids.outer_clip = outer_clip.id;

  CopyProperties(inner_viewport_scroll_layer, outer_viewport_scroll_layer);
  CreateTransformNode(outer_viewport_scroll_layer);
  auto& outer_scroll = CreateScrollNode(outer_viewport_scroll_layer);
  outer_scroll.scrolls_outer_viewport = true;
  viewport_property_ids.outer_scroll = outer_scroll.id;

  return viewport_property_ids;
}

}  // anonymous namespace

void SetupRootProperties(Layer* root) {
  SetupRootPropertiesInternal(root);
}

void SetupRootProperties(LayerImpl* root) {
  SetupRootPropertiesInternal(root);
}

void CopyProperties(const Layer* from, Layer* to) {
  DCHECK(from->layer_tree_host()->IsUsingLayerLists());
  to->SetLayerTreeHost(from->layer_tree_host());
  to->set_property_tree_sequence_number(from->property_tree_sequence_number());
  CopyPropertiesInternal(from, to);
}

void CopyProperties(const LayerImpl* from, LayerImpl* to) {
  CopyPropertiesInternal(from, to);
}

TransformNode& CreateTransformNode(Layer* layer, int parent_id) {
  DCHECK(layer->layer_tree_host()->IsUsingLayerLists());
  return CreateTransformNodeInternal(layer, GetPropertyTrees(layer), parent_id);
}

TransformNode& CreateTransformNode(LayerImpl* layer, int parent_id) {
  return CreateTransformNodeInternal(layer, GetPropertyTrees(layer), parent_id);
}

TransformNode& CreateTransformNode(PropertyTrees* property_trees,
                                   int parent_id) {
  return CreateTransformNodeInternal<Layer>(nullptr, property_trees, parent_id);
}

ClipNode& CreateClipNode(Layer* layer, int parent_id) {
  DCHECK(layer->layer_tree_host()->IsUsingLayerLists());
  return CreateClipNodeInternal(layer, GetPropertyTrees(layer), parent_id);
}

ClipNode& CreateClipNode(LayerImpl* layer, int parent_id) {
  return CreateClipNodeInternal(layer, GetPropertyTrees(layer), parent_id);
}

ClipNode& CreateClipNode(PropertyTrees* property_trees,
                         int parent_id,
                         int transform_id) {
  return CreateClipNodeInternal<Layer>(nullptr, property_trees, parent_id,
                                       transform_id);
}

EffectNode& CreateEffectNode(Layer* layer, int parent_id) {
  DCHECK(layer->layer_tree_host()->IsUsingLayerLists());
  return CreateEffectNodeInternal(layer, GetPropertyTrees(layer), parent_id);
}

EffectNode& CreateEffectNode(LayerImpl* layer, int parent_id) {
  return CreateEffectNodeInternal(layer, GetPropertyTrees(layer), parent_id);
}

EffectNode& CreateEffectNode(PropertyTrees* property_trees,
                             int parent_id,
                             int transform_id,
                             int clip_id) {
  return CreateEffectNodeInternal<Layer>(nullptr, property_trees, parent_id,
                                         transform_id, clip_id);
}

ScrollNode& CreateScrollNode(Layer* layer, int parent_id) {
  DCHECK(layer->layer_tree_host()->IsUsingLayerLists());
  return CreateScrollNodeInternal(layer, parent_id);
}

ScrollNode& CreateScrollNode(LayerImpl* layer, int parent_id) {
  return CreateScrollNodeInternal(layer, parent_id);
}

void SetupMaskProperties(Layer* masked_layer, PictureLayer* mask_layer) {
  mask_layer->SetIsDrawable(true);
  SetupMaskPropertiesInternal(masked_layer, mask_layer);
}

void SetupMaskProperties(LayerImpl* masked_layer,
                         PictureLayerImpl* mask_layer) {
  mask_layer->SetDrawsContent(true);
  SetupMaskPropertiesInternal(masked_layer, mask_layer);
}

void SetScrollOffset(Layer* layer, const gfx::ScrollOffset& scroll_offset) {
  layer->SetScrollOffset(scroll_offset);
  SetScrollOffsetInternal(layer, scroll_offset);
}

void SetScrollOffset(LayerImpl* layer, const gfx::ScrollOffset& scroll_offset) {
  if (layer->IsActive())
    layer->SetCurrentScrollOffset(scroll_offset);
  SetScrollOffsetInternal(layer, scroll_offset);
}

void SetupViewport(Layer* root,
                   scoped_refptr<Layer> outer_viewport_scroll_layer,
                   const gfx::Size& outer_viewport_size) {
  DCHECK(root);
  DCHECK_EQ(root, root->layer_tree_host()->root_layer());
  DCHECK(root->layer_tree_host()->IsUsingLayerLists());

  scoped_refptr<Layer> inner_viewport_scroll_layer = Layer::Create();
  inner_viewport_scroll_layer->SetBounds(outer_viewport_size);
  inner_viewport_scroll_layer->SetScrollable(root->bounds());
  inner_viewport_scroll_layer->SetHitTestable(true);
  outer_viewport_scroll_layer->SetScrollable(outer_viewport_size);
  outer_viewport_scroll_layer->SetHitTestable(true);

  root->AddChild(inner_viewport_scroll_layer);
  root->AddChild(outer_viewport_scroll_layer);
  root->layer_tree_host()->SetElementIdsForTesting();

  auto viewport_property_ids =
      SetupViewportProperties(root, inner_viewport_scroll_layer.get(),
                              outer_viewport_scroll_layer.get());
  root->layer_tree_host()->RegisterViewportPropertyIds(viewport_property_ids);
}

void SetupViewport(Layer* root,
                   const gfx::Size& outer_viewport_size,
                   const gfx::Size& content_size) {
  scoped_refptr<Layer> outer_viewport_scroll_layer = Layer::Create();
  outer_viewport_scroll_layer->SetBounds(content_size);
  outer_viewport_scroll_layer->SetIsDrawable(true);
  SetupViewport(root, outer_viewport_scroll_layer, outer_viewport_size);
}

void SetupViewport(LayerImpl* root,
                   const gfx::Size& outer_viewport_size,
                   const gfx::Size& content_size) {
  DCHECK(root);

  LayerTreeImpl* layer_tree_impl = root->layer_tree_impl();
  DCHECK(!layer_tree_impl->InnerViewportScrollNode());
  DCHECK(layer_tree_impl->settings().use_layer_lists);
  DCHECK_EQ(root, layer_tree_impl->root_layer());

  std::unique_ptr<LayerImpl> inner_viewport_scroll_layer =
      LayerImpl::Create(layer_tree_impl, 10000);
  inner_viewport_scroll_layer->SetBounds(outer_viewport_size);
  inner_viewport_scroll_layer->SetScrollable(root->bounds());
  inner_viewport_scroll_layer->SetHitTestable(true);
  inner_viewport_scroll_layer->SetElementId(
      LayerIdToElementIdForTesting(inner_viewport_scroll_layer->id()));

  std::unique_ptr<LayerImpl> outer_viewport_scroll_layer =
      LayerImpl::Create(layer_tree_impl, 10001);
  outer_viewport_scroll_layer->SetBounds(content_size);
  outer_viewport_scroll_layer->SetDrawsContent(true);
  outer_viewport_scroll_layer->SetScrollable(outer_viewport_size);
  outer_viewport_scroll_layer->SetHitTestable(true);
  outer_viewport_scroll_layer->SetElementId(
      LayerIdToElementIdForTesting(outer_viewport_scroll_layer->id()));

  auto viewport_property_ids =
      SetupViewportProperties(root, inner_viewport_scroll_layer.get(),
                              outer_viewport_scroll_layer.get());
  layer_tree_impl->AddLayer(std::move(inner_viewport_scroll_layer));
  layer_tree_impl->AddLayer(std::move(outer_viewport_scroll_layer));
  layer_tree_impl->SetViewportPropertyIds(viewport_property_ids);
}

PropertyTrees* GetPropertyTrees(const Layer* layer) {
  return layer->layer_tree_host()->property_trees();
}

PropertyTrees* GetPropertyTrees(const LayerImpl* layer) {
  return layer->layer_tree_impl()->property_trees();
}

RenderSurfaceImpl* GetRenderSurface(const LayerImpl* layer) {
  auto& effect_tree = GetPropertyTrees(layer)->effect_tree;
  if (auto* surface = effect_tree.GetRenderSurface(layer->effect_tree_index()))
    return surface;
  return effect_tree.GetRenderSurface(GetEffectNode(layer)->target_id);
}

}  // namespace cc
