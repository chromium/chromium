// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_widget_controller.h"

#include <utility>

#include "ash/ash_export.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "base/scoped_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/core/window_util.h"

namespace ash {

WallpaperWidgetController::WallpaperWidgetController(
    aura::Window* root_window,
    base::OnceClosure wallpaper_set_callback)
    : root_window_(root_window),
      wallpaper_set_callback_(std::move(wallpaper_set_callback)) {}

WallpaperWidgetController::~WallpaperWidgetController() {
  widget_->CloseNow();
}

void WallpaperWidgetController::Init(bool locked) {
  widget_ = CreateWallpaperWidget(root_window_, wallpaper_constants::kClear,
                                  locked, &wallpaper_view_);
}

views::Widget* WallpaperWidgetController::GetWidget() {
  return widget_.get();
}

bool WallpaperWidgetController::IsAnimating() const {
  return wallpaper_view_->layer()->GetAnimator()->is_animating();
}

void WallpaperWidgetController::StopAnimating() {
  wallpaper_view_->layer()->GetAnimator()->StopAnimating();
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

  if (parent == new_parent)
    return false;
  new_parent->AddChild(GetWidget()->GetNativeWindow());
  return true;
}

bool WallpaperWidgetController::SetWallpaperProperty(
    const WallpaperProperty& property,
    const base::TimeDelta& animation_duration) {
  StopAnimating();
  bool property_changed = !(wallpaper_view_->property() == property);

  wallpaper_view_->set_wallpaper_property(property);
  // Show the widget when we have something to show.
  if (!widget_->IsVisible())
    widget_->Show();
  if (!animation_duration.is_zero()) {
    ApplyCrossFadeAnimation(animation_duration);
  } else {
    wallpaper_view_->SchedulePaint();
    // Since there is no actual animation scheduled, just call completed method.
    OnImplicitAnimationsCompleted();
  }
  return property_changed;
}

const WallpaperProperty& WallpaperWidgetController::GetWallpaperProperty()
    const {
  return wallpaper_view_->property();
}

void WallpaperWidgetController::OnImplicitAnimationsCompleted() {
  StopAnimating();
  wallpaper_view_->SetLockShieldEnabled(
      wallpaper_view_->GetWidget()->GetNativeWindow()->parent()->id() ==
      kShellWindowId_LockScreenWallpaperContainer);
  RunAnimationEndCallbacks();
}

void WallpaperWidgetController::RunAnimationEndCallbacks() {
  std::list<base::OnceClosure> callbacks;
  animation_end_callbacks_.swap(callbacks);
  for (auto& callback : callbacks)
    std::move(callback).Run();
}

void WallpaperWidgetController::ApplyCrossFadeAnimation(
    base::TimeDelta duration) {
  DCHECK(wallpaper_view_);

  old_layer_tree_owner_ = ::wm::RecreateLayers(wallpaper_view_);
  old_layer_tree_owner_->root()->parent()->StackAbove(
      old_layer_tree_owner_->root(), wallpaper_view_->layer());

  ui::Layer* old_layer = old_layer_tree_owner_->root();
  ui::Layer* new_layer = wallpaper_view_->layer();
  old_layer->SetOpacity(1.f);
  new_layer->SetOpacity(0.f);

  ui::ScopedLayerAnimationSettings original_settings(new_layer->GetAnimator());
  ui::ScopedLayerAnimationSettings copy_settings(old_layer->GetAnimator());

  original_settings.SetTransitionDuration(duration);
  original_settings.SetTweenType(gfx::Tween::EASE_OUT);
  original_settings.AddObserver(this);

  copy_settings.SetTransitionDuration(duration);
  copy_settings.SetTweenType(gfx::Tween::EASE_IN);

  new_layer->SetOpacity(1.f);
  old_layer->SetOpacity(0.f);
}

}  // namespace ash
