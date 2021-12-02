// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_dimmer.h"

#include <memory>

#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_animations.h"

namespace ash {
namespace {

const int kDefaultDimAnimationDurationMs = 200;

const float kDefaultDimOpacity = 0.5f;

}  // namespace

WindowDimmer::WindowDimmer(aura::Window* parent,
                           bool animate,
                           Delegate* delegate)
    : parent_(parent),
      window_(new aura::Window(nullptr, aura::client::WINDOW_TYPE_NORMAL)),
      delegate_(delegate) {
  window_->Init(ui::LAYER_SOLID_COLOR);
  window_->SetName("Dimming Window");
  if (animate) {
    ::wm::SetWindowVisibilityChangesAnimated(window_);
    ::wm::SetWindowVisibilityAnimationType(
        window_, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
    ::wm::SetWindowVisibilityAnimationDuration(
        window_, base::Milliseconds(kDefaultDimAnimationDurationMs));
  }
  window_->AddObserver(this);

  SetDimOpacity(kDefaultDimOpacity);

  parent->AddChild(window_);
  parent->AddObserver(this);
  parent->StackChildAtTop(window_);

  // The window is not fully opaque. Set the transparent bit so that it
  // interacts properly with aura::WindowOcclusionTracker.
  // https://crbug.com/833814
  window_->SetTransparent(true);

  window_->SetBounds(gfx::Rect(parent_->bounds().size()));
}

WindowDimmer::~WindowDimmer() {
  if (parent_)
    parent_->RemoveObserver(this);
  if (window_) {
    window_->RemoveObserver(this);
    // See class description for details on ownership.
    delete window_;
  }
}

void WindowDimmer::SetDimOpacity(float target_opacity) {
  DCHECK(window_);
  window_->layer()->SetColor(SkColorSetA(SK_ColorBLACK, 255 * target_opacity));
}

void WindowDimmer::SetDimColor(SkColor dimming_color) {
  DCHECK(window_);
  DCHECK_NE(SkColorGetA(dimming_color), SK_AlphaOPAQUE);
  window_->layer()->SetColor(dimming_color);
}

void WindowDimmer::OnWindowBoundsChanged(aura::Window* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds,
                                         ui::PropertyChangeReason reason) {
  if (window == parent_)
    window_->SetBounds(gfx::Rect(new_bounds.size()));
}

void WindowDimmer::OnWindowDestroying(aura::Window* window) {
  if (window == parent_) {
    parent_->RemoveObserver(this);
    parent_ = nullptr;
    if (delegate_) {
      delegate_->OnDimmedWindowDestroying(window);
      // `this` can be deleted above. So don't access any member after this.
    }
  } else {
    DCHECK_EQ(window_, window);
    window_->RemoveObserver(this);
    window_ = nullptr;
  }
}

void WindowDimmer::OnWindowHierarchyChanging(
    const HierarchyChangeParams& params) {
  if (params.receiver == window_ && params.target == params.receiver) {
    // This may happen on a display change or some unexpected condition. Hide
    // the window to ensure it isn't obscuring the wrong thing.
    window_->Hide();
  }
}

void WindowDimmer::OnWindowParentChanged(aura::Window* window,
                                         aura::Window* parent) {
  if (delegate_ && window == parent_)
    delegate_->OnDimmedWindowParentChanged(window);
}

}  // namespace ash
