// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layers/heads_up_display_layer.h"

#include <algorithm>

#include "base/trace_event/trace_event.h"
#include "cc/layers/heads_up_display_layer_impl.h"
#include "cc/trees/layer_tree_host.h"

namespace cc {

scoped_refptr<HeadsUpDisplayLayer> HeadsUpDisplayLayer::Create() {
  return base::WrapRefCounted(new HeadsUpDisplayLayer());
}

HeadsUpDisplayLayer::HeadsUpDisplayLayer()
    : typeface_(SkTypeface::MakeFromName("times new roman", SkFontStyle())) {
  if (!typeface_) {
    typeface_ = SkTypeface::MakeFromName("monospace", SkFontStyle::Bold());
  }
  DCHECK(typeface_.get());
  SetIsDrawable(true);
  UpdateDrawsContent(HasDrawableContent());
}

HeadsUpDisplayLayer::~HeadsUpDisplayLayer() = default;

void HeadsUpDisplayLayer::UpdateLocationAndSize(
    const gfx::Size& device_viewport,
    float device_scale_factor) {
  gfx::Size device_viewport_in_layout_pixels =
      gfx::Size(device_viewport.width() / device_scale_factor,
                device_viewport.height() / device_scale_factor);

  gfx::Size bounds;
  gfx::Transform matrix;
  matrix.MakeIdentity();

  if (layer_tree_host()->GetDebugState().ShowHudRects()) {
    bounds = device_viewport_in_layout_pixels;
  } else {
    // If the HUD is not displaying full-viewport rects (e.g., it is showing the
    // FPS meter), use a fixed size.
    constexpr int kDefaultHUDSize = 256;
    bounds.SetSize(kDefaultHUDSize, kDefaultHUDSize);
    // Put the HUD on the top-left side instead of the top-right side because
    // the HUD sometimes can be drawn on out of the screen when it works on
    // embedded devices.
    matrix.Translate(0.0, 0.0);
  }

  SetBounds(bounds);
  SetTransform(matrix);
}

bool HeadsUpDisplayLayer::HasDrawableContent() const {
  return true;
}

std::unique_ptr<LayerImpl> HeadsUpDisplayLayer::CreateLayerImpl(
    LayerTreeImpl* tree_impl) {
  return HeadsUpDisplayLayerImpl::Create(tree_impl, id());
}

const std::vector<gfx::Rect>& HeadsUpDisplayLayer::LayoutShiftRects() const {
  return layout_shift_rects_;
}

void HeadsUpDisplayLayer::SetLayoutShiftRects(
    const std::vector<gfx::Rect>& rects) {
  layout_shift_rects_ = rects;
}

void HeadsUpDisplayLayer::PushPropertiesTo(LayerImpl* layer) {
  Layer::PushPropertiesTo(layer);
  TRACE_EVENT0("cc", "HeadsUpDisplayLayer::PushPropertiesTo");
  HeadsUpDisplayLayerImpl* layer_impl =
      static_cast<HeadsUpDisplayLayerImpl*>(layer);

  layer_impl->SetHUDTypeface(typeface_);
  layer_impl->SetLayoutShiftRects(layout_shift_rects_);
  layout_shift_rects_.clear();
}

}  // namespace cc
