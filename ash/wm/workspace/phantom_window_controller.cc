// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/phantom_window_controller.h"

#include <math.h>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/background.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/shadow_types.h"

namespace ash {
namespace {

// The duration of the show animation.
const int kAnimationDurationMs = 200;

// The size of the phantom window at the beginning of the show animation in
// relation to the size of the phantom window at the end of the animation.
const float kStartBoundsRatio = 0.85f;

// The elevation of the shadow for the phantom window should match that of an
// active window.
// The shadow ninebox requires a minimum size to work well. See
// ui/compositor_extra/shadow.cc
constexpr int kMinWidthWithShadow = 2 * ::wm::kShadowElevationActiveWindow;
constexpr int kMinHeightWithShadow = 4 * ::wm::kShadowElevationActiveWindow;

}  // namespace

// PhantomWindowController ----------------------------------------------------

PhantomWindowController::PhantomWindowController(aura::Window* window)
    : window_(window) {}

PhantomWindowController::~PhantomWindowController() = default;

void PhantomWindowController::Show(const gfx::Rect& bounds_in_screen) {
  if (bounds_in_screen == target_bounds_in_screen_)
    return;
  target_bounds_in_screen_ = bounds_in_screen;

  gfx::Rect start_bounds_in_screen = target_bounds_in_screen_;
  int start_width = std::max(
      kMinWidthWithShadow,
      static_cast<int>(start_bounds_in_screen.width() * kStartBoundsRatio));
  int start_height = std::max(
      kMinHeightWithShadow,
      static_cast<int>(start_bounds_in_screen.height() * kStartBoundsRatio));
  start_bounds_in_screen.Inset(
      floor((start_bounds_in_screen.width() - start_width) / 2.0f),
      floor((start_bounds_in_screen.height() - start_height) / 2.0f));
  phantom_widget_ = CreatePhantomWidget(
      window_util::GetRootWindowMatching(target_bounds_in_screen_),
      start_bounds_in_screen);
}

std::unique_ptr<views::Widget> PhantomWindowController::CreatePhantomWidget(
    aura::Window* root_window,
    const gfx::Rect& bounds_in_screen) {
  std::unique_ptr<views::Widget> phantom_widget(new views::Widget);
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  // PhantomWindowController is used by FrameMaximizeButton to highlight the
  // launcher button. Put the phantom in the same window as the launcher so that
  // the phantom is visible.
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.name = "PhantomWindow";
  params.layer_type = ui::LAYER_SOLID_COLOR;
  params.shadow_type = views::Widget::InitParams::ShadowType::kDrop;
  params.shadow_elevation = ::wm::kShadowElevationActiveWindow;
  params.parent = root_window->GetChildById(kShellWindowId_ShelfContainer);
  phantom_widget->set_focus_on_creation(false);
  phantom_widget->Init(std::move(params));
  phantom_widget->SetVisibilityChangedAnimationsEnabled(false);
  aura::Window* phantom_widget_window = phantom_widget->GetNativeWindow();
  phantom_widget_window->set_id(kShellWindowId_PhantomWindow);
  phantom_widget->SetBounds(bounds_in_screen);
  // TODO(sky): I suspect this is never true, verify that.
  if (phantom_widget_window->parent() == window_->parent()) {
    phantom_widget_window->parent()->StackChildAbove(phantom_widget_window,
                                                     window_);
  }
  ui::Layer* widget_layer = phantom_widget_window->layer();
  widget_layer->SetColor(SkColorSetA(SK_ColorWHITE, 0.4 * 255));

  phantom_widget->Show();

  // Fade the window in.
  widget_layer->SetOpacity(0);
  ui::ScopedLayerAnimationSettings scoped_setter(widget_layer->GetAnimator());
  scoped_setter.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kAnimationDurationMs));
  scoped_setter.SetTweenType(gfx::Tween::EASE_IN);
  scoped_setter.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  widget_layer->SetOpacity(1);
  phantom_widget->SetBounds(target_bounds_in_screen_);

  return phantom_widget;
}

}  // namespace ash
