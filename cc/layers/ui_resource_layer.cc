// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/ui_resource_layer.h"

#include "base/trace_event/trace_event.h"
#include "cc/layers/ui_resource_layer_impl.h"
#include "cc/resources/scoped_ui_resource.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/resources/ui_resource_manager.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

scoped_refptr<UIResourceLayer> UIResourceLayer::Create() {
  return base::WrapRefCounted(new UIResourceLayer());
}

UIResourceLayer::UIResourceLayer()
    : uv_top_left_(0.f, 0.f), uv_bottom_right_(1.f, 1.f) {
  vertex_opacity_[0] = 1.0f;
  vertex_opacity_[1] = 1.0f;
  vertex_opacity_[2] = 1.0f;
  vertex_opacity_[3] = 1.0f;
}

UIResourceLayer::~UIResourceLayer() = default;

std::unique_ptr<LayerImpl> UIResourceLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return UIResourceLayerImpl::Create(tree_impl, id());
}

void UIResourceLayer::SetUV(const gfx::PointF& top_left,
                            const gfx::PointF& bottom_right) {
  DCHECK(IsMutationAllowed());
  if (uv_top_left_ == top_left && uv_bottom_right_ == bottom_right)
    return;
  uv_top_left_ = top_left;
  uv_bottom_right_ = bottom_right;
  SetNeedsCommit();
}

void UIResourceLayer::SetVertexOpacity(float bottom_left,
                                       float top_left,
                                       float top_right,
                                       float bottom_right) {
  DCHECK(IsMutationAllowed());
  // Indexing according to the quad vertex generation:
  // 1--2
  // |  |
  // 0--3
  if (vertex_opacity_[0] == bottom_left &&
      vertex_opacity_[1] == top_left &&
      vertex_opacity_[2] == top_right &&
      vertex_opacity_[3] == bottom_right)
    return;
  vertex_opacity_[0] = bottom_left;
  vertex_opacity_[1] = top_left;
  vertex_opacity_[2] = top_right;
  vertex_opacity_[3] = bottom_right;
  SetNeedsCommit();
}

void UIResourceLayer::SetLayerTreeHost(LayerTreeHost* host) {
  if (host == layer_tree_host())
    return;

  Layer::SetLayerTreeHost(host);

  // Recreate the resource held against the new LTH.
  RecreateUIResourceIdFromBitmap();

  UpdateDrawsContent(HasDrawableContent());
}

void UIResourceLayer::SetBitmap(const SkBitmap& bitmap) {
  DCHECK(IsMutationAllowed());
  bitmap_ = bitmap;
  if (!layer_tree_host())
    return;
  SetUIResourceIdInternal(
      layer_tree_host()->GetUIResourceManager()->GetOrCreateUIResource(bitmap));
}

void UIResourceLayer::SetUIResourceId(UIResourceId resource_id) {
  DCHECK(IsMutationAllowed());
  // Even if the ID is not changing we should drop the bitmap. The ID is 0 when
  // there's no layer tree. When setting an id (even if to 0), we should no
  // longer keep the bitmap.
  bitmap_.reset();
  if (resource_id_ == resource_id)
    return;
  SetUIResourceIdInternal(resource_id);
}

bool UIResourceLayer::HasDrawableContent() const {
  return resource_id_ && Layer::HasDrawableContent();
}

void UIResourceLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);
  TRACE_EVENT0("cc", "UIResourceLayer::PushPropertiesTo");
  UIResourceLayerImpl* layer_impl = static_cast<UIResourceLayerImpl*>(layer);

  layer_impl->SetUIResourceId(resource_id_);
  if (resource_id_) {
    DCHECK(layer_tree_host());

    gfx::Size image_size =
        layer_tree_host()->GetUIResourceManager()->GetUIResourceSize(
            resource_id_);
    layer_impl->SetImageBounds(image_size);
    layer_impl->SetUV(uv_top_left_, uv_bottom_right_);
    layer_impl->SetVertexOpacity(vertex_opacity_);
  }
}

void UIResourceLayer::RecreateUIResourceIdFromBitmap() {
  if (!bitmap_.empty())
    SetBitmap(bitmap_);
}

void UIResourceLayer::SetUIResourceIdInternal(UIResourceId resource_id) {
  resource_id_ = resource_id;
  UpdateDrawsContent(HasDrawableContent());
  SetNeedsCommit();
}

}  // namespace cc
