// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_dimmer.h"

#include "ash/root_window_controller.h"
#include "ash/style/ash_color_provider_source.h"
#include "ash/style/color_util.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/wm/core/visibility_controller.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/public/activation_delegate.h"

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
  wm::SetActivationDelegate(window_, this);
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

  // `this` may already start observing the color provider source through
  // `OnWindowAddedToRootWindow` if `window_` is alreadyed added to the root
  // window.
  if (!GetColorProviderSource()) {
    auto* color_provider_source =
        ColorUtil::GetColorProviderSourceForWindow(window_);
    if (color_provider_source)
      ui::ColorProviderSourceObserver::Observe(color_provider_source);
  }
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
  // Once this function is called, reset the `dim_color_type_`, which means we
  // don't need to update the color on window's layer on native theme updated.
  // Since after this call the color on the layer will be updated to the default
  // dimming color which is Black.
  dim_color_type_.reset();

  DCHECK(window_);
  window_->layer()->SetColor(SkColorSetA(SK_ColorBLACK, 255 * target_opacity));
}

void WindowDimmer::SetDimColor(ui::ColorId color_id) {
  DCHECK(window_);
  dim_color_type_ = color_id;
  UpdateDimColor();
}

bool WindowDimmer::ShouldActivate() const {
  // The dimming window should never be activate-able.
  return false;
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
  if (params.receiver == window_.get() && params.target == params.receiver) {
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

void WindowDimmer::OnWindowAddedToRootWindow(aura::Window* window) {
  if (GetColorProviderSource())
    return;

  // There's a chance when `this` is being created, `window_` is not added to
  // the root window yet, hence we should observe the `color_provider_source`
  // which is owned by the `RootWindowController` here.
  auto* color_provider_source =
      ColorUtil::GetColorProviderSourceForWindow(window);
  DCHECK(color_provider_source);
  ui::ColorProviderSourceObserver::Observe(color_provider_source);
  UpdateDimColor();
}

void WindowDimmer::OnColorProviderChanged() {
  UpdateDimColor();
}

void WindowDimmer::UpdateDimColor() {
  if (!window_)
    return;

  if (!dim_color_type_.has_value())
    return;

  auto* color_provider_source = GetColorProviderSource();
  if (!color_provider_source)
    return;

  auto dimming_color = color_provider_source->GetColorProvider()->GetColor(
      dim_color_type_.value());
  DCHECK_NE(SkColorGetA(dimming_color), SK_AlphaOPAQUE);
  window_->layer()->SetColor(dimming_color);
}

}  // namespace ash
