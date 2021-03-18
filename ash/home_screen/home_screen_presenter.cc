// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/home_screen/home_screen_presenter.h"

#include <string>
#include <utility>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/home_launcher_animation_info.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"

namespace ash {
namespace {

// The target scale to which (or from which) the home screen will animate when
// overview is being shown (or hidden) using fade transitions while home screen
// is shown.
constexpr float kOverviewFadeAnimationScale = 0.92f;

// The home screen animation duration for transitions that accompany overview
// fading transitions.
constexpr base::TimeDelta kOverviewFadeAnimationDuration =
    base::TimeDelta::FromMilliseconds(350);

// TODO(https://crbug.com/1137452): Eliminate this method and inline the value.
base::TimeDelta GetAnimationDurationForTransition(
    HomeScreenPresenter::TransitionType transition) {
  switch (transition) {
    case HomeScreenPresenter::TransitionType::kScaleHomeIn:
    case HomeScreenPresenter::TransitionType::kScaleHomeOut:
      return kOverviewFadeAnimationDuration;
  }
}

HomeScreenPresenter::TransitionType GetOppositeTransition(
    HomeScreenPresenter::TransitionType transition) {
  switch (transition) {
    case HomeScreenPresenter::TransitionType::kScaleHomeIn:
      return HomeScreenPresenter::TransitionType::kScaleHomeOut;
    case HomeScreenPresenter::TransitionType::kScaleHomeOut:
      return HomeScreenPresenter::TransitionType::kScaleHomeIn;
  }
}

// TODO(https://crbug.com/1137452): Eliminate this method and inline the value.
HomeLauncherAnimationTrigger GetAnimationTrigger(
    HomeScreenPresenter::TransitionType transition) {
  return HomeLauncherAnimationTrigger::kOverviewModeFade;
}

bool IsShowingHomeTransition(HomeScreenPresenter::TransitionType transition) {
  return transition == HomeScreenPresenter::TransitionType::kScaleHomeIn;
}

void UpdateOverviewSettings(base::TimeDelta duration,
                            ui::ScopedLayerAnimationSettings* settings) {
  settings->SetTransitionDuration(duration);
  settings->SetTweenType(gfx::Tween::FAST_OUT_SLOW_IN);
  settings->SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
}

}  // namespace

HomeScreenPresenter::HomeScreenPresenter() = default;

HomeScreenPresenter::~HomeScreenPresenter() = default;

void HomeScreenPresenter::ScheduleOverviewModeAnimation(
    TransitionType transition,
    bool animate) {
  // If animating, set the source parameters first.
  if (animate) {
    // Force the home view into the expected initial state without animation,
    // except when transitioning out from home screen. Gesture handling for the
    // gesture to move to overview can update the scale before triggering
    // transition to overview - undoing these changes here would make the UI
    // jump during the transition.
    if (transition != TransitionType::kScaleHomeOut) {
      SetFinalHomeTransformForTransition(GetOppositeTransition(transition),
                                         base::TimeDelta());
    }
  }

  // Hide all transient child windows in the app list (e.g. uninstall dialog)
  // before starting the overview mode transition, and restore them when
  // reshowing the app list.
  aura::Window* app_list_window =
      Shell::Get()->app_list_controller()->GetHomeScreenWindow();
  if (app_list_window) {
    const bool showing_home = IsShowingHomeTransition(transition);
    for (auto* child : wm::GetTransientChildren(app_list_window)) {
      if (showing_home)
        child->Show();
      else
        child->Hide();
    }
  }

  SetFinalHomeTransformForTransition(
      transition, animate ? GetAnimationDurationForTransition(transition)
                          : base::TimeDelta());
}

void HomeScreenPresenter::SetFinalHomeTransformForTransition(
    TransitionType transition,
    base::TimeDelta animation_duration) {
  base::RepeatingCallback<void(ui::ScopedLayerAnimationSettings * settings)>
      animation_settings_updater =
          !animation_duration.is_zero()
              ? base::BindRepeating(&UpdateOverviewSettings, animation_duration)
              : base::NullCallback();

  base::Optional<HomeLauncherAnimationInfo> animation_info =
      !animation_duration.is_zero()
          ? base::make_optional<HomeLauncherAnimationInfo>(
                GetAnimationTrigger(transition),
                IsShowingHomeTransition(transition))
          : base::nullopt;

  AppListControllerImpl* app_list_controller =
      Shell::Get()->app_list_controller();
  switch (transition) {
    case TransitionType::kScaleHomeIn:
      app_list_controller->UpdateScaleAndOpacityForHomeLauncher(
          1.0 /*scale*/, 1.0 /*opacity*/, std::move(animation_info),
          animation_settings_updater);
      break;
    case TransitionType::kScaleHomeOut:
      app_list_controller->UpdateScaleAndOpacityForHomeLauncher(
          kOverviewFadeAnimationScale /*scale*/, 0.0 /*opacity*/,
          std::move(animation_info), animation_settings_updater);
      break;
  }
}

}  // namespace ash
