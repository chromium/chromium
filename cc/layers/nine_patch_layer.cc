// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/nine_patch_layer.h"

#include "base/trace_event/trace_event.h"
#include "cc/layers/nine_patch_layer_impl.h"
#include "cc/resources/scoped_ui_resource.h"
#include "cc/resources/ui_resource_bitmap.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

scoped_refptr<NinePatchLayer> NinePatchLayer::Create() {
  return base::WrapRefCounted(new NinePatchLayer());
}

NinePatchLayer::NinePatchLayer()
    : UIResourceLayer(), fill_center_(false), nearest_neighbor_(false) {}

NinePatchLayer::~NinePatchLayer() = default;

std::unique_ptr<LayerImpl> NinePatchLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) const {
  return NinePatchLayerImpl::Create(tree_impl, id());
}

void NinePatchLayer::SetBorder(const gfx::Rect& border) {
  if (border == border_.Read(*this))
    return;
  border_.Write(*this) = border;
  SetNeedsCommit();
}

void NinePatchLayer::SetAperture(const gfx::Rect& aperture) {
  if (image_aperture_.Read(*this) == aperture)
    return;

  image_aperture_.Write(*this) = aperture;
  SetNeedsCommit();
}

void NinePatchLayer::SetFillCenter(bool fill_center) {
  if (fill_center_.Read(*this) == fill_center)
    return;

  fill_center_.Write(*this) = fill_center;
  SetNeedsCommit();
}

void NinePatchLayer::SetNearestNeighbor(bool nearest_neighbor) {
  if (nearest_neighbor_.Read(*this) == nearest_neighbor)
    return;

  nearest_neighbor_.Write(*this) = nearest_neighbor;
  SetNeedsCommit();
}

void NinePatchLayer::SetLayerOcclusion(const gfx::Rect& occlusion) {
  if (layer_occlusion_.Read(*this) == occlusion)
    return;

  layer_occlusion_.Write(*this) = occlusion;
  SetNeedsCommit();
}

void NinePatchLayer::PushPropertiesTo(
    LayerImpl* layer,
    const CommitState& commit_state,
    const ThreadUnsafeCommitState& unsafe_state) {
  UIResourceLayer::PushPropertiesTo(layer, commit_state, unsafe_state);
  TRACE_EVENT0("cc", "NinePatchLayer::PushPropertiesTo");
  NinePatchLayerImpl* layer_impl = static_cast<NinePatchLayerImpl*>(layer);

  if (resource_id()) {
    DCHECK(IsAttached());
    layer_impl->SetLayout(image_aperture_.Read(*this), border_.Read(*this),
                          layer_occlusion_.Read(*this),
                          fill_center_.Read(*this),
                          nearest_neighbor_.Read(*this));
  }
}

}  // namespace cc
