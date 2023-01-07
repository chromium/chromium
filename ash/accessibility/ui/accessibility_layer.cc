// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/ui/accessibility_layer.h"

#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/canvas.h"

namespace ui {
class Compositor;
}

namespace ash {

AccessibilityLayer::AccessibilityLayer(AccessibilityLayerDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate);
}

AccessibilityLayer::~AccessibilityLayer() = default;

void AccessibilityLayer::Set(aura::Window* root_window,
                             const gfx::Rect& bounds,
                             bool stack_at_top) {
  DCHECK(root_window);
  layer_rect_ = bounds;
  gfx::Rect layer_bounds = bounds;
  int inset = -(GetInset());
  layer_bounds.Inset(inset);
  CreateOrUpdateLayer(root_window, "AccessibilityLayer", layer_bounds,
                      stack_at_top);
}

void AccessibilityLayer::SetOpacity(float opacity) {
  // Clamp to 0. It's possible for floating-point math to produce opacity
  // slightly less than 0.
  layer_->SetOpacity(std::max(0.f, opacity));
}

void AccessibilityLayer::SetSubpixelPositionOffset(
    const gfx::Vector2dF& offset) {
  layer_->SetSubpixelPositionOffset(offset);
}

void AccessibilityLayer::CreateOrUpdateLayer(aura::Window* root_window,
                                             const char* layer_name,
                                             const gfx::Rect& bounds,
                                             bool stack_at_top) {
  if (!layer_ || root_window != root_window_) {
    root_window_ = root_window;
    ui::Layer* root_layer = root_window->layer();
    layer_ = std::make_unique<ui::Layer>(ui::LAYER_TEXTURED);
    layer_->SetName(layer_name);
    layer_->SetFillsBoundsOpaquely(false);
    root_layer->Add(layer_.get());
    // Adding |layer_| to |root_layer| will trigger a DeviceScaleFactorChanged.
    // AccessibilityFocusRingControllerImpl doesn't need to react to this
    // initial DSF change, so set the delegate after Add().
    layer_->set_delegate(this);
  }

  // Keep moving it to the top in case new layers have been added
  // since we created this layer.
  if (stack_at_top) {
    layer_->parent()->StackAtTop(layer_.get());
  } else {
    layer_->parent()->StackAtBottom(layer_.get());
  }

  layer_->SetBounds(bounds);
  gfx::Rect layer_bounds(0, 0, bounds.width(), bounds.height());
  layer_->SchedulePaint(layer_bounds);
}

void AccessibilityLayer::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {
  if (delegate_)
    delegate_->OnDeviceScaleFactorChanged();
}

}  // namespace ash
