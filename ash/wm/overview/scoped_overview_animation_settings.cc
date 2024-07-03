// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_overview_animation_settings.h"

#include "ash/metrics/histogram_macros.h"
#include "ash/public/cpp/metrics_util.h"
#include "ash/wm/overview/overview_constants.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"

namespace ash {

namespace {

// The time duration for fading out when closing an item.
constexpr base::TimeDelta kCloseFadeOut = base::Milliseconds(100);

// The time duration for scaling down when an item is closed.
constexpr base::TimeDelta kCloseScale = base::Milliseconds(100);

// The time duration for widgets to fade in.
constexpr base::TimeDelta kFadeInDelay = base::Milliseconds(83);
constexpr base::TimeDelta kFadeIn = base::Milliseconds(167);

// The time duration for informed restore dialog to fade in.
constexpr base::TimeDelta kShowInformedRestoreDialog = base::Milliseconds(800);

// The time duration for widgets to fade out.
constexpr base::TimeDelta kFadeOut = base::Milliseconds(100);

// The time duration for birch bar to fade out.
constexpr base::TimeDelta kBirchBarFadeOut = base::Milliseconds(50);

constexpr base::TimeDelta kFromHomeLauncherDelay = base::Milliseconds(250);
constexpr base::TimeDelta kHomeLauncherTransition = base::Milliseconds(350);

// Time duration of the show animation of the drop target.
constexpr base::TimeDelta kDropTargetFade = base::Milliseconds(250);

// Time duration to fade in overview windows when a window drag slows down or
// stops.
constexpr base::TimeDelta kFadeInOnWindowDrag = base::Milliseconds(350);

constexpr base::TimeDelta kWindowRestoreDuration = base::Milliseconds(350);

base::TimeDelta GetAnimationDuration(OverviewAnimationType animation_type) {
  switch (animation_type) {
    case OVERVIEW_ANIMATION_NONE:
      return base::TimeDelta();
    case OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN:
      return kFadeIn;
    case OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_FADE_OUT:
      return kFadeOut;
    case OVERVIEW_ANIMATION_SHOW_INFORMED_RESTORE_DIALOG_ON_ENTER:
      return kShowInformedRestoreDialog;
    case OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_EXIT:
      return kWindowRestoreDuration;
    case OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_ENTER:
    case OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW:
    case OVERVIEW_ANIMATION_RESTORE_WINDOW:
    case OVERVIEW_ANIMATION_RESTORE_WINDOW_ZERO:
    case OVERVIEW_ANIMATION_SPAWN_ITEM_IN_OVERVIEW:
      return kTransition;
    case OVERVIEW_ANIMATION_CLOSING_OVERVIEW_ITEM:
      return kCloseScale;
    case OVERVIEW_ANIMATION_CLOSE_OVERVIEW_ITEM:
      return kCloseFadeOut;
    case OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER:
    case OVERVIEW_ANIMATION_EXIT_TO_HOME_LAUNCHER:
      return kHomeLauncherTransition;
    case OVERVIEW_ANIMATION_DROP_TARGET_FADE:
      return kDropTargetFade;
    case OVERVIEW_ANIMATION_NO_RECENTS_FADE:
      return kTransition;
    case OVERVIEW_ANIMATION_OPACITY_ON_WINDOW_DRAG:
      return kFadeInOnWindowDrag;
    case OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_SAVED_DESK_GRID_FADE_OUT:
      return kFadeOut;
    case OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_BIRCH_BAR_FADE_OUT:
      return kBirchBarFadeOut;
  }
}

void ReportCloseSmoothness(int smoothness) {
  UMA_HISTOGRAM_PERCENTAGE_IN_CLAMSHELL(
      "Ash.Overview.AnimationSmoothness.Close.ClamshellMode", smoothness);
  UMA_HISTOGRAM_PERCENTAGE_IN_TABLET(
      "Ash.Overview.AnimationSmoothness.Close.TabletMode", smoothness);
}

}  // namespace

ScopedOverviewAnimationSettings::ScopedOverviewAnimationSettings(
    OverviewAnimationType animation_type,
    aura::Window* window)
    : ScopedOverviewAnimationSettings(animation_type,
                                      window->layer()->GetAnimator()) {}

ScopedOverviewAnimationSettings::ScopedOverviewAnimationSettings(
    OverviewAnimationType animation_type,
    ui::LayerAnimator* animator)
    : animation_settings_(
          std::make_unique<ui::ScopedLayerAnimationSettings>(animator)) {
  switch (animation_type) {
    case OVERVIEW_ANIMATION_NONE:
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
      break;
    case OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN:
      animation_settings_->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
      animator->SchedulePauseForProperties(kFadeInDelay,
                                           ui::LayerAnimationElement::OPACITY);
      break;
    case OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_FADE_OUT:
      animation_settings_->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
      break;
    case OVERVIEW_ANIMATION_SHOW_INFORMED_RESTORE_DIALOG_ON_ENTER:
      animation_settings_->SetTweenType(gfx::Tween::EASE_IN_OUT_EMPHASIZED);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
      break;
    case OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_EXIT:
      animation_settings_->SetTweenType(gfx::Tween::ACCEL_20_DECEL_100);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      break;
    case OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_ON_ENTER:
    case OVERVIEW_ANIMATION_LAYOUT_OVERVIEW_ITEMS_IN_OVERVIEW:
    case OVERVIEW_ANIMATION_RESTORE_WINDOW:
    case OVERVIEW_ANIMATION_SPAWN_ITEM_IN_OVERVIEW:
      animation_settings_->SetTweenType(gfx::Tween::EASE_OUT);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      break;
    case OVERVIEW_ANIMATION_RESTORE_WINDOW_ZERO:
      animation_settings_->SetTweenType(gfx::Tween::ZERO);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      break;
    case OVERVIEW_ANIMATION_CLOSING_OVERVIEW_ITEM:
    case OVERVIEW_ANIMATION_CLOSE_OVERVIEW_ITEM:
      animation_settings_->SetTweenType(gfx::Tween::EASE_OUT);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
      break;
    case OVERVIEW_ANIMATION_ENTER_FROM_HOME_LAUNCHER:
      animation_settings_->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::ENQUEUE_NEW_ANIMATION);
      // Add animation delay when entering from home launcher.
      animator->SchedulePauseForProperties(kFromHomeLauncherDelay,
                                           ui::LayerAnimationElement::OPACITY);
      break;
    case OVERVIEW_ANIMATION_EXIT_TO_HOME_LAUNCHER:
      animation_settings_->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::REPLACE_QUEUED_ANIMATIONS);
      break;
    case OVERVIEW_ANIMATION_DROP_TARGET_FADE:
      animation_settings_->SetTweenType(gfx::Tween::EASE_IN);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      break;
    case OVERVIEW_ANIMATION_NO_RECENTS_FADE:
      animation_settings_->SetTweenType(gfx::Tween::EASE_IN_OUT);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      break;
    case OVERVIEW_ANIMATION_OPACITY_ON_WINDOW_DRAG:
      animation_settings_->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      break;
    case OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_SAVED_DESK_GRID_FADE_OUT:
      animation_settings_->SetTweenType(gfx::Tween::LINEAR);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      break;
    case OVERVIEW_ANIMATION_EXIT_OVERVIEW_MODE_BIRCH_BAR_FADE_OUT:
      animation_settings_->SetTweenType(gfx::Tween::LINEAR);
      animation_settings_->SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      break;
  }
  animation_settings_->SetTransitionDuration(
      GetAnimationDuration(animation_type));
  if (animation_type == OVERVIEW_ANIMATION_CLOSING_OVERVIEW_ITEM ||
      animation_type == OVERVIEW_ANIMATION_CLOSE_OVERVIEW_ITEM) {
    close_reporter_.emplace(animation_settings_->GetAnimator(),
                            metrics_util::ForSmoothnessV3(
                                base::BindRepeating(&ReportCloseSmoothness)));
  }
}

ScopedOverviewAnimationSettings::~ScopedOverviewAnimationSettings() = default;

void ScopedOverviewAnimationSettings::AddObserver(
    ui::ImplicitAnimationObserver* observer) {
  animation_settings_->AddObserver(observer);
}

void ScopedOverviewAnimationSettings::CacheRenderSurface() {
  animation_settings_->CacheRenderSurface();
}

void ScopedOverviewAnimationSettings::DeferPaint() {
  animation_settings_->DeferPaint();
}

void ScopedOverviewAnimationSettings::TrilinearFiltering() {
  animation_settings_->TrilinearFiltering();
}

ui::LayerAnimator* ScopedOverviewAnimationSettings::GetAnimator() {
  return animation_settings_->GetAnimator();
}

}  // namespace ash
