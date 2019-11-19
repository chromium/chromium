// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_wallpaper_controller.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_view.h"
#include "ash/wallpaper/wallpaper_widget_controller.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ui/aura/window_tree_host.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

// Do not change the wallpaper when entering or exiting overview mode when this
// is true.
bool g_disable_wallpaper_change_for_tests = false;

constexpr int kBlurSlideDurationMs = 250;

bool IsWallpaperChangeAllowed() {
  return !g_disable_wallpaper_change_for_tests &&
         Shell::Get()->wallpaper_controller()->IsBlurAllowed();
}

WallpaperWidgetController* GetWallpaperWidgetController(aura::Window* root) {
  return RootWindowController::ForWindow(root)->wallpaper_widget_controller();
}

ui::LayerAnimator* GetAnimator(
    WallpaperWidgetController* wallpaper_widget_controller) {
  return wallpaper_widget_controller->GetWidget()
      ->GetNativeWindow()
      ->layer()
      ->GetAnimator();
}

}  // namespace

OverviewWallpaperController::OverviewWallpaperController()
    : use_cross_fade_(base::FeatureList::IsEnabled(
          features::kOverviewCrossFadeWallpaperBlur)) {}

OverviewWallpaperController::~OverviewWallpaperController() {
  if (compositor_)
    compositor_->RemoveAnimationObserver(this);
  for (aura::Window* root : roots_to_animate_)
    root->RemoveObserver(this);

  StopObservingImplicitAnimations();
}

// static
void OverviewWallpaperController::SetDoNotChangeWallpaperForTests() {
  g_disable_wallpaper_change_for_tests = true;
}

void OverviewWallpaperController::Blur(bool animate_only) {
  if (!IsWallpaperChangeAllowed())
    return;
  OnBlurChange(WallpaperAnimationState::kAddingBlur, animate_only);
}

void OverviewWallpaperController::Unblur() {
  if (!IsWallpaperChangeAllowed())
    return;
  OnBlurChange(WallpaperAnimationState::kRemovingBlur,
               /*animate_only=*/false);
}

bool OverviewWallpaperController::HasBlurAnimationForTesting() const {
  if (!use_cross_fade_)
    return !!compositor_;

  for (aura::Window* root : Shell::Get()->GetAllRootWindows()) {
    auto* wallpaper_widget_controller = GetWallpaperWidgetController(root);
    if (GetAnimator(wallpaper_widget_controller)->is_animating())
      return true;
  }
  return false;
}

void OverviewWallpaperController::StopBlurAnimationsForTesting() {
  DCHECK(use_cross_fade_);
  for (auto& layer_tree : animating_copies_)
    layer_tree->root()->GetAnimator()->StopAnimating();
  for (aura::Window* root : Shell::Get()->GetAllRootWindows()) {
    auto* wallpaper_widget_controller = GetWallpaperWidgetController(root);
    wallpaper_widget_controller->EndPendingAnimation();
    GetAnimator(wallpaper_widget_controller)->StopAnimating();
  }
}

void OverviewWallpaperController::Stop() {
  if (compositor_) {
    compositor_->RemoveAnimationObserver(this);
    compositor_ = nullptr;
  }
  state_ = WallpaperAnimationState::kNormal;
}

void OverviewWallpaperController::Start() {
  DCHECK(!compositor_);
  compositor_ = Shell::GetPrimaryRootWindow()->GetHost()->compositor();
  compositor_->AddAnimationObserver(this);
  start_time_ = base::TimeTicks();
}

void OverviewWallpaperController::AnimationProgressed(float value) {
  // Animate only to even numbers to reduce the load.
  int ivalue = static_cast<int>(value * kWallpaperBlurSigma) / 2 * 2;
  for (aura::Window* root : roots_to_animate_)
    ApplyBlurAndOpacity(root, ivalue);
}

void OverviewWallpaperController::OnAnimationStep(base::TimeTicks timestamp) {
  DCHECK(!use_cross_fade_);
  if (start_time_ == base::TimeTicks()) {
    start_time_ = timestamp;
    return;
  }
  const float progress = (timestamp - start_time_).InMilliseconds() /
                         static_cast<float>(kBlurSlideDurationMs);
  const bool adding = state_ == WallpaperAnimationState::kAddingBlur;
  if (progress > 1.0f) {
    AnimationProgressed(adding ? 1.0f : 0.f);
    Stop();
  } else {
    AnimationProgressed(adding ? progress : 1.f - progress);
  }
}

void OverviewWallpaperController::OnCompositingShuttingDown(
    ui::Compositor* compositor) {
  DCHECK(!use_cross_fade_);
  if (compositor_ == compositor)
    Stop();
}

void OverviewWallpaperController::OnWindowDestroying(aura::Window* window) {
  DCHECK(!use_cross_fade_);
  window->RemoveObserver(this);
  auto it =
      std::find(roots_to_animate_.begin(), roots_to_animate_.end(), window);
  if (it != roots_to_animate_.end())
    roots_to_animate_.erase(it);
}

void OverviewWallpaperController::OnImplicitAnimationsCompleted() {
  DCHECK(use_cross_fade_);
  animating_copies_.clear();
  state_ = WallpaperAnimationState::kNormal;
}

void OverviewWallpaperController::ApplyBlurAndOpacity(aura::Window* root,
                                                      int value) {
  DCHECK_GE(value, 0);
  DCHECK_LE(value, 10);
  const float opacity =
      gfx::Tween::FloatValueBetween(value / 10.0, 1.f, kShieldOpacity);
  auto* wallpaper_widget_controller =
      RootWindowController::ForWindow(root)->wallpaper_widget_controller();
  if (wallpaper_widget_controller->wallpaper_view()) {
    wallpaper_widget_controller->wallpaper_view()->RepaintBlurAndOpacity(
        value, opacity);
  }
}

void OverviewWallpaperController::OnBlurChange(WallpaperAnimationState state,
                                               bool animate_only) {
  if (use_cross_fade_) {
    OnBlurChangeCrossFade(state, animate_only);
    return;
  }

  Stop();
  for (aura::Window* root : roots_to_animate_)
    root->RemoveObserver(this);
  roots_to_animate_.clear();

  state_ = state;
  const bool should_blur = state_ == WallpaperAnimationState::kAddingBlur;
  if (animate_only)
    DCHECK(should_blur);

  const float value =
      should_blur ? kWallpaperBlurSigma : kWallpaperClearBlurSigma;

  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  for (aura::Window* root : Shell::Get()->GetAllRootWindows()) {
    // No need to animate the blur on exiting as this should only be called
    // after overview animations are finished.
    if (should_blur) {
      DCHECK(overview_session);
      OverviewGrid* grid = overview_session->GetGridWithRootWindow(root);
      bool should_animate = grid && grid->ShouldAnimateWallpaper();
      auto* wallpaper_view = RootWindowController::ForWindow(root)
                                 ->wallpaper_widget_controller()
                                 ->wallpaper_view();
      float blur_sigma = wallpaper_view ? wallpaper_view->repaint_blur() : 0.f;
      if (should_animate && animate_only && blur_sigma != kWallpaperBlurSigma) {
        root->AddObserver(this);
        roots_to_animate_.push_back(root);
        continue;
      }
      if (should_animate == animate_only)
        ApplyBlurAndOpacity(root, value);
    } else {
      ApplyBlurAndOpacity(root, value);
    }
  }

  // Run the animation if one of the roots needs to be animated.
  if (roots_to_animate_.empty())
    state_ = WallpaperAnimationState::kNormal;
  else
    Start();
}

void OverviewWallpaperController::OnBlurChangeCrossFade(
    WallpaperAnimationState state,
    bool animate_only) {
  state_ = state;
  const bool should_blur = state_ == WallpaperAnimationState::kAddingBlur;
  if (animate_only)
    DCHECK(should_blur);

  OverviewSession* overview_session =
      Shell::Get()->overview_controller()->overview_session();
  for (aura::Window* root : Shell::Get()->GetAllRootWindows()) {
    // |overview_session| may be null on overview exit because we call this
    // after the animations are done running. We don't support animation on exit
    // so just set |should_animate| to false.
    OverviewGrid* grid = overview_session
                             ? overview_session->GetGridWithRootWindow(root)
                             : nullptr;
    bool should_animate = grid && grid->ShouldAnimateWallpaper();
    if (should_animate != animate_only)
      continue;

    auto* wallpaper_widget_controller = GetWallpaperWidgetController(root);
    wallpaper_widget_controller->EndPendingAnimation();
    auto* wallpaper_window =
        wallpaper_widget_controller->GetWidget()->GetNativeWindow();

    // No need to animate the blur on exiting as this should only be called
    // after overview animations are finished.
    std::unique_ptr<ui::LayerTreeOwner> copy_layer_tree;
    if (should_blur && should_animate) {
      // On animating, create the copy that of the wallpaper. The original
      // wallpaper layer will then get blurred and faded in. The copy is
      // deleted after animating.
      copy_layer_tree = ::wm::RecreateLayers(wallpaper_window);
      copy_layer_tree->root()->SetOpacity(1.f);
      copy_layer_tree->root()->parent()->StackAtBottom(copy_layer_tree->root());
    }

    ui::Layer* original_layer = wallpaper_window->layer();
    original_layer->GetAnimator()->StopAnimating();
    // Tablet mode wallpaper is already dimmed, so no need to change the
    // opacity.
    const float dimming =
        Shell::Get()->tablet_mode_controller()->InTabletMode() || !should_blur
            ? 1.f
            : kShieldOpacity;
    wallpaper_widget_controller->wallpaper_view()->RepaintBlurAndOpacity(
        should_blur ? kWallpaperBlurSigma : kWallpaperClearBlurSigma, dimming);
    original_layer->SetOpacity(should_blur ? 0.f : 1.f);

    ui::Layer* copy_layer = copy_layer_tree ? copy_layer_tree->root() : nullptr;
    if (copy_layer)
      copy_layer->GetAnimator()->StopAnimating();

    std::unique_ptr<ui::ScopedLayerAnimationSettings> original_settings;
    std::unique_ptr<ui::ScopedLayerAnimationSettings> copy_settings;
    if (should_blur && should_animate) {
      original_settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
          original_layer->GetAnimator());
      original_settings->SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(kBlurSlideDurationMs));
      original_settings->SetTweenType(gfx::Tween::EASE_OUT);

      DCHECK(copy_layer);
      copy_settings = std::make_unique<ui::ScopedLayerAnimationSettings>(
          copy_layer->GetAnimator());
      copy_settings->SetTransitionDuration(
          base::TimeDelta::FromMilliseconds(kBlurSlideDurationMs));
      copy_settings->SetTweenType(gfx::Tween::EASE_IN);
      copy_settings->AddObserver(this);

      animating_copies_.emplace_back(std::move(copy_layer_tree));
    } else {
      state_ = WallpaperAnimationState::kNormal;
    }

    original_layer->SetOpacity(1.f);
    if (copy_layer)
      copy_layer->SetOpacity(0.f);
  }

  if (animating_copies_.empty())
    state_ = WallpaperAnimationState::kNormal;
}

}  // namespace ash
