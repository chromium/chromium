// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/accessibility_layer.h"

#include "ui/aura/window.h"
#include "ui/compositor/compositor_animation_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"

namespace ui {
class Compositor;
}

namespace ash {

AccessibilityLayer::AccessibilityLayer(AccessibilityLayerDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate);
}

AccessibilityLayer::~AccessibilityLayer() {
  if (compositor_ && compositor_->HasAnimationObserver(this))
    compositor_->RemoveAnimationObserver(this);
}

void AccessibilityLayer::Set(aura::Window* root_window,
                             const gfx::Rect& bounds) {
  DCHECK(root_window);
  layer_rect_ = bounds;
  gfx::Rect layer_bounds = bounds;
  int inset = -(GetInset());
  layer_bounds.Inset(inset, inset, inset, inset);
  CreateOrUpdateLayer(root_window, "AccessibilityLayer", layer_bounds);
}

void AccessibilityLayer::SetOpacity(float opacity) {
  // Clamp to 0. It's possible for floating-point math to produce opacity
  // slightly less than 0.
  layer_->SetOpacity(std::max(0.f, opacity));
}

void AccessibilityLayer::CreateOrUpdateLayer(aura::Window* root_window,
                                             const char* layer_name,
                                             const gfx::Rect& bounds) {
  if (!layer_ || root_window != root_window_) {
    root_window_ = root_window;
    ui::Layer* root_layer = root_window->layer();
    layer_ = std::make_unique<ui::Layer>(ui::LAYER_TEXTURED);
    layer_->set_name(layer_name);
    layer_->SetFillsBoundsOpaquely(false);
    root_layer->Add(layer_.get());
    // Adding |layer_| to |root_layer| will trigger a DeviceScaleFactorChanged.
    // AccessibilityFocusRingControllerImpl doesn't need to react to this
    // initial DSF change, so set the delegate after Add().
    layer_->set_delegate(this);
  }

  // Keep moving it to the top in case new layers have been added
  // since we created this layer.
  layer_->parent()->StackAtTop(layer_.get());

  layer_->SetBounds(bounds);
  gfx::Rect layer_bounds(0, 0, bounds.width(), bounds.height());
  layer_->SchedulePaint(layer_bounds);

  if (NeedToAnimate()) {
    // Update the animation observer.
    display::Display display =
        display::Screen::GetScreen()->GetDisplayMatching(bounds);
    ui::Compositor* compositor = root_window->layer()->GetCompositor();
    if (compositor != compositor_) {
      if (compositor_ && compositor_->HasAnimationObserver(this))
        compositor_->RemoveAnimationObserver(this);
      compositor_ = compositor;
      if (compositor_ && !compositor_->HasAnimationObserver(this))
        compositor_->AddAnimationObserver(this);
    }
  }
}

void AccessibilityLayer::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {
  if (delegate_)
    delegate_->OnDeviceScaleFactorChanged();
}

void AccessibilityLayer::OnAnimationStep(base::TimeTicks timestamp) {
  delegate_->OnAnimationStep(timestamp);
}

void AccessibilityLayer::OnCompositingShuttingDown(ui::Compositor* compositor) {
  if (compositor == compositor_) {
    compositor->RemoveAnimationObserver(this);
    compositor_ = nullptr;
  }
}

}  // namespace ash
