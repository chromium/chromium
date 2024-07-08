// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/views/wallpaper_widget_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/style/color_util.h"
#include "ash/wallpaper/views/wallpaper_view.h"
#include "ui/aura/window.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/core/window_util.h"

namespace ash {

WallpaperWidgetController::WallpaperWidgetController(aura::Window* root_window)
    : root_window_(root_window) {
  Observe(ColorUtil::GetColorProviderSourceForWindow(root_window_));
}

WallpaperWidgetController::~WallpaperWidgetController() {
  widget_->CloseNow();
}

void WallpaperWidgetController::Init(bool locked) {
  widget_ = CreateWallpaperWidget(root_window_, wallpaper_constants::kClear,
                                  locked, &wallpaper_view_);
  CreateWallpaperUnderlayLayer();
}

views::Widget* WallpaperWidgetController::GetWidget() {
  return widget_.get();
}

bool WallpaperWidgetController::IsAnimating() const {
  return old_layer_tree_owner_ &&
         old_layer_tree_owner_->root()->GetAnimator()->is_animating();
}

void WallpaperWidgetController::StopAnimating() {
  if (old_layer_tree_owner_) {
    old_layer_tree_owner_->root()->GetAnimator()->StopAnimating();
    old_layer_tree_owner_.reset();
  }
}

void WallpaperWidgetController::AddAnimationEndCallback(
    base::OnceClosure callback) {
  animation_end_callbacks_.emplace_back(std::move(callback));
}

bool WallpaperWidgetController::Reparent(int container) {
  auto* parent = GetWidget()->GetNativeWindow()->parent();
  auto* root_window = parent->GetRootWindow();
  aura::Window* new_parent = root_window->GetChildById(container);

  if (parent == new_parent) {
    return false;
  }
  new_parent->AddChild(GetWidget()->GetNativeWindow());
  return true;
}

bool WallpaperWidgetController::SetWallpaperBlur(
    float blur,
    const base::TimeDelta& animation_duration) {
  if (!widget_->GetNativeWindow()) {
    return false;
  }

  StopAnimating();
  bool blur_changed = wallpaper_view_->blur_sigma() != blur;

  wallpaper_view_->set_blur_sigma(blur);
  // Show the widget when we have something to show.
  if (!widget_->IsVisible()) {
    widget_->Show();
  }
  if (!animation_duration.is_zero()) {
    ApplyCrossFadeAnimation(animation_duration);
  } else {
    wallpaper_view_->SchedulePaint();
    // Since there is no actual animation scheduled, just call completed method.
    OnImplicitAnimationsCompleted();
  }
  return blur_changed;
}

float WallpaperWidgetController::GetWallpaperBlur() const {
  return wallpaper_view_->blur_sigma();
}

void WallpaperWidgetController::OnImplicitAnimationsCompleted() {
  StopAnimating();
  wallpaper_view_->SetLockShieldEnabled(
      wallpaper_view_->GetWidget()->GetNativeWindow()->parent()->GetId() ==
      kShellWindowId_LockScreenWallpaperContainer);
  RunAnimationEndCallbacks();
}

void WallpaperWidgetController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t metrics) {
  if (!wallpaper_underlay_layer_) {
    return;
  }

  if (!(metrics &
        (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_ROTATION |
         DISPLAY_METRIC_DEVICE_SCALE_FACTOR | DISPLAY_METRIC_WORK_AREA))) {
    return;
  }

  if (root_window_ != Shell::GetRootWindowForDisplayId(display.id())) {
    return;
  }

  // Bounds have to be in parent. Since these are set on the layer directly, and
  // layer bounds are relative to the layer's parent.
  wallpaper_underlay_layer_->SetBounds(root_window_->bounds());
}

void WallpaperWidgetController::OnColorProviderChanged() {
  if (wallpaper_underlay_layer_) {
    wallpaper_underlay_layer_->SetColor(
        GetColorProviderSource()->GetColorProvider()->GetColor(
            cros_tokens::kCrosSysSystemBase));
  }
}

void WallpaperWidgetController::CreateWallpaperUnderlayLayer() {
  if (!features::IsForestFeatureEnabled()) {
    return;
  }

  wallpaper_underlay_layer_ =
      std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  wallpaper_underlay_layer_->SetName("WallpaperUnderlayLayer");
  auto* wallpaper_view_layer = wallpaper_view_->layer();
  auto* wallpaper_view_layer_parent = wallpaper_view_layer->parent();
  wallpaper_view_layer_parent->Add(wallpaper_underlay_layer_.get());
  wallpaper_view_layer_parent->StackBelow(wallpaper_underlay_layer_.get(),
                                          wallpaper_view_layer);
  wallpaper_underlay_layer_->SetBounds(root_window_->bounds());

  OnColorProviderChanged();

  // The `wallpaper_underlay_layer_` should be invisible by default. This
  // prevents the compositor from unnecessarily considering it during occlusion
  // calculations, potentially improving performance. The layer should only
  // become visible when needed (i.e. when entering overview).
  wallpaper_underlay_layer_->SetVisible(false);
}

void WallpaperWidgetController::RunAnimationEndCallbacks() {
  std::list<base::OnceClosure> callbacks;
  animation_end_callbacks_.swap(callbacks);
  for (auto& callback : callbacks) {
    std::move(callback).Run();
  }
}

void WallpaperWidgetController::ApplyCrossFadeAnimation(
    base::TimeDelta duration) {
  DCHECK(wallpaper_view_);

  old_layer_tree_owner_ = wm::RecreateLayers(wallpaper_view_);

  ui::Layer* old_layer = old_layer_tree_owner_->root();
  auto* old_layer_parent = old_layer->parent();
  ui::Layer* new_layer = wallpaper_view_->layer();
  DCHECK_EQ(old_layer_parent, new_layer->parent());
  old_layer_parent->StackAbove(old_layer, new_layer);
  if (wallpaper_underlay_layer_) {
    old_layer_parent->StackBelow(wallpaper_underlay_layer_.get(), new_layer);
  }

  old_layer->SetOpacity(1.f);
  new_layer->SetOpacity(1.f);

  // Fade out the old layer. When clearing the blur, use the opposite tween so
  // that the animations are mirrors of each other.
  const bool clearing =
      wallpaper_view_->blur_sigma() == wallpaper_constants::kClear;
  ui::ScopedLayerAnimationSettings settings(old_layer->GetAnimator());
  settings.SetTransitionDuration(duration);
  settings.SetTweenType(clearing ? gfx::Tween::EASE_IN : gfx::Tween::EASE_OUT);
  settings.AddObserver(this);

  old_layer->SetOpacity(0.f);
}

}  // namespace ash
