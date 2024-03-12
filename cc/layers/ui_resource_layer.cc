// Copyright 2013 The Chromium Authors
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
    : resource_id_(0), uv_top_left_(0.f, 0.f), uv_bottom_right_(1.f, 1.f) {
}

UIResourceLayer::~UIResourceLayer() = default;

std::unique_ptr<LayerImpl> UIResourceLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return UIResourceLayerImpl::Create(tree_impl, id());
}

void UIResourceLayer::SetUV(const gfx::PointF& top_left,
                            const gfx::PointF& bottom_right) {
  if (uv_top_left_.Read(*this) == top_left &&
      uv_bottom_right_.Read(*this) == bottom_right)
    return;
  uv_top_left_.Write(*this) = top_left;
  uv_bottom_right_.Write(*this) = bottom_right;
  SetNeedsCommit();
}

void UIResourceLayer::SetLayerTreeHost(LayerTreeHost* host) {
  if (host == layer_tree_host())
    return;

  Layer::SetLayerTreeHost(host);

  // Recreate the resource held against the new LTH.
  RecreateUIResourceIdFromBitmap();

  UpdateDrawsContent();
}

void UIResourceLayer::SetBitmap(const SkBitmap& bitmap) {
  bitmap_.Write(*this) = bitmap;
  if (!layer_tree_host())
    return;
  SetUIResourceIdInternal(
      layer_tree_host()->GetUIResourceManager()->GetOrCreateUIResource(bitmap));
}

void UIResourceLayer::SetUIResourceId(UIResourceId resource_id) {
  // Even if the ID is not changing we should drop the bitmap. The ID is 0 when
  // there's no layer tree. When setting an id (even if to 0), we should no
  // longer keep the bitmap.
  bitmap_.Write(*this).reset();
  if (resource_id_.Read(*this) == resource_id)
    return;
  SetUIResourceIdInternal(resource_id);
}

bool UIResourceLayer::HasDrawableContent() const {
  return resource_id_.Read(*this) && Layer::HasDrawableContent();
}

void UIResourceLayer::PushPropertiesTo(
    LayerImpl* layer,
    const CommitState& commit_state,
    const ThreadUnsafeCommitState& unsafe_state) {
  Layer::PushPropertiesTo(layer, commit_state, unsafe_state);
  TRACE_EVENT0("cc", "UIResourceLayer::PushPropertiesTo");
  UIResourceLayerImpl* layer_impl = static_cast<UIResourceLayerImpl*>(layer);

  UIResourceId resource_id = resource_id_.Read(*this);
  layer_impl->SetUIResourceId(resource_id);
  if (resource_id) {
    auto iter = commit_state.ui_resource_sizes.find(resource_id);
    gfx::Size image_bounds = (iter == commit_state.ui_resource_sizes.end())
                                 ? gfx::Size()
                                 : iter->second;
    layer_impl->SetImageBounds(image_bounds);
    layer_impl->SetUV(uv_top_left_.Read(*this), uv_bottom_right_.Read(*this));
  }
}

void UIResourceLayer::RecreateUIResourceIdFromBitmap() {
  if (!bitmap_.Read(*this).empty())
    SetBitmap(bitmap_.Read(*this));
}

void UIResourceLayer::SetUIResourceIdInternal(UIResourceId resource_id) {
  resource_id_.Write(*this) = resource_id;
  UpdateDrawsContent();
  SetNeedsCommit();
}

}  // namespace cc
