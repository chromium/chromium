// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_background_animator.h"

#include <algorithm>
#include <memory>

#include "ash/animation/animation_change_type.h"
#include "ash/public/cpp/login_constants.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/wallpaper_types.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_background_animator_observer.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

using ColorProfile = color_utils::ColorProfile;
using LumaRange = color_utils::LumaRange;
using SaturationRange = color_utils::SaturationRange;

namespace ash {

ShelfBackgroundAnimator::AnimationValues::AnimationValues() = default;

ShelfBackgroundAnimator::AnimationValues::~AnimationValues() = default;

void ShelfBackgroundAnimator::AnimationValues::UpdateCurrentValues(double t) {
  current_color_ =
      gfx::Tween::ColorValueBetween(t, initial_color_, target_color_);
}

void ShelfBackgroundAnimator::AnimationValues::SetTargetValues(
    SkColor target_color) {
  initial_color_ = current_color_;
  target_color_ = target_color;
}

bool ShelfBackgroundAnimator::AnimationValues::InitialValuesEqualTargetValuesOf(
    const AnimationValues& other) const {
  return initial_color_ == other.target_color_;
}

ShelfBackgroundAnimator::ShelfBackgroundAnimator(
    Shelf* shelf,
    WallpaperControllerImpl* wallpaper_controller)
    : shelf_(shelf), wallpaper_controller_(wallpaper_controller) {}

ShelfBackgroundAnimator::~ShelfBackgroundAnimator() {
  if (wallpaper_controller_)
    wallpaper_controller_->RemoveObserver(this);
  if (shelf_)
    shelf_->RemoveObserver(this);
}

void ShelfBackgroundAnimator::Init(ShelfBackgroundType background_type) {
  if (wallpaper_controller_)
    wallpaper_controller_->AddObserver(this);
  if (shelf_)
    shelf_->AddObserver(this);

  // Initialize animators so that adding observers get notified with consistent
  // values.
  AnimateBackground(background_type, AnimationChangeType::IMMEDIATE);
}

void ShelfBackgroundAnimator::AddObserver(
    ShelfBackgroundAnimatorObserver* observer) {
  observers_.AddObserver(observer);
  NotifyObserver(observer);
}

void ShelfBackgroundAnimator::RemoveObserver(
    ShelfBackgroundAnimatorObserver* observer) {
  observers_.RemoveObserver(observer);
}

void ShelfBackgroundAnimator::NotifyObserver(
    ShelfBackgroundAnimatorObserver* observer) {
  observer->UpdateShelfBackground(shelf_background_values_.current_color());
}

void ShelfBackgroundAnimator::PaintBackground(
    ShelfBackgroundType background_type,
    AnimationChangeType change_type) {
  if (target_background_type_ == background_type &&
      change_type == AnimationChangeType::ANIMATE) {
    return;
  }

  AnimateBackground(background_type, change_type);
}

void ShelfBackgroundAnimator::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK_EQ(animation, animator_.get());
  SetAnimationValues(animation->GetCurrentValue());
}

void ShelfBackgroundAnimator::AnimationEnded(const gfx::Animation* animation) {
  DCHECK_EQ(animation, animator_.get());
  SetAnimationValues(animation->GetCurrentValue());
  animator_.reset();

  for (auto& observer : observers_)
    observer.OnShelfBackgroundAnimationEnded();
}

void ShelfBackgroundAnimator::OnWallpaperColorsChanged() {
  AnimateBackground(target_background_type_, AnimationChangeType::ANIMATE);
}

void ShelfBackgroundAnimator::OnBackgroundTypeChanged(
    ShelfBackgroundType background_type,
    AnimationChangeType change_type) {
  PaintBackground(background_type, change_type);
}

void ShelfBackgroundAnimator::NotifyObservers() {
  for (auto& observer : observers_)
    NotifyObserver(&observer);
}

void ShelfBackgroundAnimator::AnimateBackground(
    ShelfBackgroundType background_type,
    AnimationChangeType change_type) {
  StopAnimator();

  if (change_type == AnimationChangeType::IMMEDIATE) {
    animator_.reset();
    SetTargetValues(background_type);
    SetAnimationValues(1.0);
  } else if (CanReuseAnimator(background_type)) {
    // |animator_| should not be null here as CanReuseAnimator() returns false
    // when it is null.
    if (animator_->IsShowing())
      animator_->Hide();
    else
      animator_->Show();
  } else {
    CreateAnimator(background_type);
    SetTargetValues(background_type);
    animator_->Show();
  }

  if (target_background_type_ != background_type) {
    previous_background_type_ = target_background_type_;
    target_background_type_ = background_type;
  }
}

bool ShelfBackgroundAnimator::CanReuseAnimator(
    ShelfBackgroundType background_type) const {
  if (!animator_)
    return false;

  AnimationValues target_shelf_background_values;
  GetTargetValues(background_type, &target_shelf_background_values);

  return previous_background_type_ == background_type &&
         shelf_background_values_.InitialValuesEqualTargetValuesOf(
             target_shelf_background_values);
}

void ShelfBackgroundAnimator::CreateAnimator(
    ShelfBackgroundType background_type) {
  base::TimeDelta duration;

  switch (background_type) {
    case SHELF_BACKGROUND_DEFAULT:
    case SHELF_BACKGROUND_APP_LIST:
    case SHELF_BACKGROUND_HOME_LAUNCHER:
    case SHELF_BACKGROUND_MAXIMIZED_WITH_APP_LIST:
      duration = base::TimeDelta::FromMilliseconds(500);
      break;
    case SHELF_BACKGROUND_MAXIMIZED:
    case SHELF_BACKGROUND_OOBE:
    case SHELF_BACKGROUND_LOGIN:
    case SHELF_BACKGROUND_LOGIN_NONBLURRED_WALLPAPER:
    case SHELF_BACKGROUND_OVERVIEW:
      duration = base::TimeDelta::FromMilliseconds(250);
      break;
  }

  animator_ = std::make_unique<gfx::SlideAnimation>(this);
  animator_->SetSlideDuration(duration);
}

void ShelfBackgroundAnimator::StopAnimator() {
  if (animator_)
    animator_->Stop();
}

void ShelfBackgroundAnimator::SetTargetValues(
    ShelfBackgroundType background_type) {
  GetTargetValues(background_type, &shelf_background_values_);
}

void ShelfBackgroundAnimator::GetTargetValues(
    ShelfBackgroundType background_type,
    AnimationValues* shelf_background_values) const {
  shelf_background_values->SetTargetValues(GetBackgroundColor(background_type));
}

SkColor ShelfBackgroundAnimator::GetBackgroundColor(
    ShelfBackgroundType background_type) const {
  SkColor shelf_target_color = ShelfConfig::Get()->GetDefaultShelfColor();
  switch (background_type) {
    case SHELF_BACKGROUND_APP_LIST:
    case SHELF_BACKGROUND_MAXIMIZED_WITH_APP_LIST:
      shelf_target_color = ShelfConfig::Get()->GetShelfWithAppListColor();
      break;
    case SHELF_BACKGROUND_DEFAULT:
    case SHELF_BACKGROUND_HOME_LAUNCHER:
      shelf_target_color = ShelfConfig::Get()->GetDefaultShelfColor();
      break;
    case SHELF_BACKGROUND_MAXIMIZED:
    case SHELF_BACKGROUND_OVERVIEW:
      shelf_target_color = ShelfConfig::Get()->GetMaximizedShelfColor();
      break;
    case SHELF_BACKGROUND_OOBE:
      shelf_target_color = SK_ColorTRANSPARENT;
      break;
    case SHELF_BACKGROUND_LOGIN:
      shelf_target_color = SK_ColorTRANSPARENT;
      break;
    case SHELF_BACKGROUND_LOGIN_NONBLURRED_WALLPAPER:
      shelf_target_color =
          SkColorSetA(login_constants::kDefaultBaseColor,
                      login_constants::kNonBlurredWallpaperBackgroundAlpha);
      break;
  }
  return shelf_target_color;
}

void ShelfBackgroundAnimator::SetAnimationValues(double t) {
  DCHECK_GE(t, 0.0);
  DCHECK_LE(t, 1.0);
  shelf_background_values_.UpdateCurrentValues(t);
  NotifyObservers();
}

}  // namespace ash
