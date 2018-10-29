// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_background_animator.h"

#include <algorithm>
#include <memory>

#include "ash/animation/animation_change_type.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/login_constants.h"
#include "ash/public/cpp/wallpaper_types.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_background_animator_observer.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "base/command_line.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/color_utils.h"

using ColorProfile = color_utils::ColorProfile;
using LumaRange = color_utils::LumaRange;
using SaturationRange = color_utils::SaturationRange;

namespace ash {

namespace {

// Returns color profile used for shelf based on the kAshShelfColorScheme
// command line arg.
ColorProfile GetShelfColorProfile() {
  const std::string switch_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAshShelfColorScheme);

  ColorProfile color_profile(LumaRange::DARK, SaturationRange::MUTED);

  if (switch_value.find("light") != std::string::npos)
    color_profile.luma = LumaRange::LIGHT;
  else if (switch_value.find("normal") != std::string::npos)
    color_profile.luma = LumaRange::NORMAL;
  else if (switch_value.find("dark") != std::string::npos)
    color_profile.luma = LumaRange::DARK;

  if (switch_value.find("vibrant") != std::string::npos)
    color_profile.saturation = SaturationRange::VIBRANT;
  else if (switch_value.find("muted") != std::string::npos)
    color_profile.saturation = SaturationRange::MUTED;

  return color_profile;
}

// Gets the target color alpha value of shelf and shelf item according to the
// given |background_type|.
std::pair<int, int> GetTargetColorAlphaValues(
    ShelfBackgroundType background_type) {
  int target_shelf_color_alpha = SK_AlphaTRANSPARENT;
  int target_item_color_alpha = SK_AlphaTRANSPARENT;

  switch (background_type) {
    case SHELF_BACKGROUND_DEFAULT:
      target_shelf_color_alpha = kShelfTranslucentAlpha;
      target_item_color_alpha = SK_AlphaTRANSPARENT;
      break;
    case SHELF_BACKGROUND_OVERLAP:
      target_shelf_color_alpha = kShelfTranslucentAlpha;
      target_item_color_alpha = SK_AlphaTRANSPARENT;
      break;
    case SHELF_BACKGROUND_MAXIMIZED:
      target_shelf_color_alpha = kShelfTranslucentMaximizedWindow;
      target_item_color_alpha = SK_AlphaTRANSPARENT;
      break;
    case SHELF_BACKGROUND_APP_LIST:
      target_shelf_color_alpha = kShelfTranslucentOverAppList;
      target_item_color_alpha = SK_AlphaTRANSPARENT;
      break;
    case SHELF_BACKGROUND_SPLIT_VIEW:
      target_shelf_color_alpha = ShelfBackgroundAnimator::kMaxAlpha;
      target_item_color_alpha = SK_AlphaTRANSPARENT;
      break;
    case SHELF_BACKGROUND_OOBE:
      target_shelf_color_alpha = SK_AlphaTRANSPARENT;
      target_item_color_alpha = SK_AlphaOPAQUE;
      break;
    case SHELF_BACKGROUND_LOGIN:
      target_shelf_color_alpha = SK_AlphaTRANSPARENT;
      target_item_color_alpha = SK_AlphaTRANSPARENT;
      break;
    case SHELF_BACKGROUND_LOGIN_NONBLURRED_WALLPAPER:
      target_shelf_color_alpha =
          login_constants::kNonBlurredWallpaperBackgroundAlpha;
      target_item_color_alpha = SK_AlphaTRANSPARENT;
      break;
  }
  return std::pair<int, int>(target_shelf_color_alpha, target_item_color_alpha);
}

}  // namespace

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
    ShelfBackgroundType background_type,
    Shelf* shelf,
    WallpaperController* wallpaper_controller)
    : shelf_(shelf), wallpaper_controller_(wallpaper_controller) {
  if (wallpaper_controller_)
    wallpaper_controller_->AddObserver(this);
  if (shelf_)
    shelf_->AddObserver(this);

  // Initialize animators so that adding observers get notified with consistent
  // values.
  AnimateBackground(background_type, AnimationChangeType::IMMEDIATE);
}

ShelfBackgroundAnimator::~ShelfBackgroundAnimator() {
  if (wallpaper_controller_)
    wallpaper_controller_->RemoveObserver(this);
  if (shelf_)
    shelf_->RemoveObserver(this);
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
  observer->UpdateShelfItemBackground(item_background_values_.current_color());
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
}

int ShelfBackgroundAnimator::GetBackgroundAlphaValue(
    ShelfBackgroundType background_type) const {
  return GetTargetColorAlphaValues(background_type).first;
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
  AnimationValues target_item_background_values;
  GetTargetValues(background_type, &target_shelf_background_values,
                  &target_item_background_values);

  return previous_background_type_ == background_type &&
         shelf_background_values_.InitialValuesEqualTargetValuesOf(
             target_shelf_background_values) &&
         item_background_values_.InitialValuesEqualTargetValuesOf(
             target_item_background_values);
}

void ShelfBackgroundAnimator::CreateAnimator(
    ShelfBackgroundType background_type) {
  int duration_ms = 0;

  switch (background_type) {
    case SHELF_BACKGROUND_DEFAULT:
    case SHELF_BACKGROUND_OVERLAP:
    case SHELF_BACKGROUND_APP_LIST:
      duration_ms = 500;
      break;
    case SHELF_BACKGROUND_MAXIMIZED:
    case SHELF_BACKGROUND_SPLIT_VIEW:
    case SHELF_BACKGROUND_OOBE:
    case SHELF_BACKGROUND_LOGIN:
    case SHELF_BACKGROUND_LOGIN_NONBLURRED_WALLPAPER:
      duration_ms = 250;
      break;
  }

  animator_ = std::make_unique<gfx::SlideAnimation>(this);
  animator_->SetSlideDuration(duration_ms);
}

void ShelfBackgroundAnimator::StopAnimator() {
  if (animator_)
    animator_->Stop();
}

void ShelfBackgroundAnimator::SetTargetValues(
    ShelfBackgroundType background_type) {
  GetTargetValues(background_type, &shelf_background_values_,
                  &item_background_values_);
}

void ShelfBackgroundAnimator::GetTargetValues(
    ShelfBackgroundType background_type,
    AnimationValues* shelf_background_values,
    AnimationValues* item_background_values) const {
  // Fetches wallpaper color and darkens it.
  auto darken_wallpaper = [&](int darkening_alpha) {
    if (!wallpaper_controller_)
      return kShelfDefaultBaseColor;
    SkColor target_color =
        wallpaper_controller_->GetProminentColor(GetShelfColorProfile());
    if (target_color == kInvalidWallpaperColor)
      return kShelfDefaultBaseColor;
    return color_utils::GetResultingPaintColor(
        SkColorSetA(kShelfDefaultBaseColor, darkening_alpha), target_color);
  };

  SkColor shelf_target_color = kShelfDefaultBaseColor;
  SkColor item_target_color = kShelfDefaultBaseColor;
  switch (background_type) {
    case SHELF_BACKGROUND_DEFAULT:
    case SHELF_BACKGROUND_OVERLAP:
    case SHELF_BACKGROUND_APP_LIST:
      shelf_target_color = darken_wallpaper(kShelfTranslucentColorDarkenAlpha);
      item_target_color = shelf_target_color;
      break;
    case SHELF_BACKGROUND_MAXIMIZED:
      shelf_target_color = darken_wallpaper(kShelfOpaqueColorDarkenAlpha);
      item_target_color = shelf_target_color;
      break;
    case SHELF_BACKGROUND_SPLIT_VIEW:
      shelf_target_color = darken_wallpaper(ShelfBackgroundAnimator::kMaxAlpha);
      item_target_color = shelf_target_color;
      break;
    case SHELF_BACKGROUND_OOBE:
      shelf_target_color = SK_ColorTRANSPARENT;
      item_target_color = gfx::kGoogleGrey100;
      break;
    case SHELF_BACKGROUND_LOGIN:
      shelf_target_color = SK_ColorTRANSPARENT;
      item_target_color = SK_ColorTRANSPARENT;
      break;
    case SHELF_BACKGROUND_LOGIN_NONBLURRED_WALLPAPER:
      shelf_target_color = login_constants::kDefaultBaseColor;
      item_target_color = login_constants::kDefaultBaseColor;
      break;
  }

  std::pair<int, int> target_color_alpha_values =
      GetTargetColorAlphaValues(background_type);
  shelf_background_values->SetTargetValues(
      SkColorSetA(shelf_target_color, target_color_alpha_values.first));
  item_background_values->SetTargetValues(
      SkColorSetA(item_target_color, target_color_alpha_values.second));
}

void ShelfBackgroundAnimator::SetAnimationValues(double t) {
  DCHECK_GE(t, 0.0);
  DCHECK_LE(t, 1.0);
  shelf_background_values_.UpdateCurrentValues(t);
  item_background_values_.UpdateCurrentValues(t);
  NotifyObservers();
}

}  // namespace ash
