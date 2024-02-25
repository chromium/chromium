// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_background_animator.h"

#include <algorithm>
#include <memory>

#include "ash/animation/animation_change_type.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_background_animator_observer.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/time/time.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/animation/tween.h"

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
    case ShelfBackgroundType::kDefaultBg:
    case ShelfBackgroundType::kHomeLauncher:
      duration = base::Milliseconds(500);
      break;
    case ShelfBackgroundType::kMaximized:
    case ShelfBackgroundType::kOobe:
    case ShelfBackgroundType::kLogin:
    case ShelfBackgroundType::kLoginNonBlurredWallpaper:
    case ShelfBackgroundType::kOverview:
    case ShelfBackgroundType::kInApp:
      duration = base::Milliseconds(250);
      break;
  }

  animator_ = std::make_unique<gfx::SlideAnimation>(this);
  animator_->SetSlideDuration(
      ui::ScopedAnimationDurationScaleMode::duration_multiplier() * duration);
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
  if (!shelf_)
    return shelf_background_values_.current_color();

  const auto* shelf_widget = shelf_->shelf_widget();
  DCHECK(shelf_widget);

  SkColor shelf_target_color =
      ShelfConfig::Get()->GetDefaultShelfColor(shelf_widget);
  switch (background_type) {
    case ShelfBackgroundType::kDefaultBg:
    case ShelfBackgroundType::kHomeLauncher:
      shelf_target_color =
          ShelfConfig::Get()->GetDefaultShelfColor(shelf_widget);
      break;
    case ShelfBackgroundType::kMaximized:
    case ShelfBackgroundType::kInApp:
      shelf_target_color =
          ShelfConfig::Get()->GetMaximizedShelfColor(shelf_widget);
      break;
    case ShelfBackgroundType::kOverview:
      shelf_target_color =
          display::Screen::GetScreen()->InTabletMode()
              ? ShelfConfig::Get()->GetMaximizedShelfColor(shelf_widget)
              : ShelfConfig::Get()->GetDefaultShelfColor(shelf_widget);
      break;
    case ShelfBackgroundType::kOobe:
      shelf_target_color = SK_ColorTRANSPARENT;
      break;
    case ShelfBackgroundType::kLogin:
      shelf_target_color = SK_ColorTRANSPARENT;
      break;
    case ShelfBackgroundType::kLoginNonBlurredWallpaper:
      shelf_target_color = shelf_->shelf_widget()->GetColorProvider()->GetColor(
          kColorAshShieldAndBase80);
      break;
  }
  return shelf_target_color;
}

void ShelfBackgroundAnimator::CompleteAnimationForTesting() {
  if (animator_)
    animator_->End();
}

void ShelfBackgroundAnimator::SetAnimationValues(double t) {
  DCHECK_GE(t, 0.0);
  DCHECK_LE(t, 1.0);
  shelf_background_values_.UpdateCurrentValues(t);
  NotifyObservers();
}

}  // namespace ash
