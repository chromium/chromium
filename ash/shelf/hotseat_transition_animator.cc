// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/hotseat_transition_animator.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rounded_corners_f.h"

namespace ash {

HotseatTransitionAnimator::HotseatTransitionAnimator(ShelfWidget* shelf_widget)
    : shelf_widget_(shelf_widget) {
  Shell::Get()->tablet_mode_controller()->AddObserver(this);
}

HotseatTransitionAnimator::~HotseatTransitionAnimator() {
  StopObservingImplicitAnimations();
  if (Shell::Get()->tablet_mode_controller())
    Shell::Get()->tablet_mode_controller()->RemoveObserver(this);
}

void HotseatTransitionAnimator::OnHotseatStateChanged(HotseatState old_state,
                                                      HotseatState new_state) {
  DoAnimation(old_state, new_state);
}

void HotseatTransitionAnimator::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void HotseatTransitionAnimator::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void HotseatTransitionAnimator::OnImplicitAnimationsCompleted() {
  std::move(animation_complete_callback_).Run();
}

void HotseatTransitionAnimator::OnTabletModeStarting() {
  tablet_mode_transitioning_ = true;
}

void HotseatTransitionAnimator::OnTabletModeStarted() {
  tablet_mode_transitioning_ = false;
}

void HotseatTransitionAnimator::OnTabletModeEnding() {
  tablet_mode_transitioning_ = true;
}

void HotseatTransitionAnimator::OnTabletModeEnded() {
  tablet_mode_transitioning_ = false;
}

void HotseatTransitionAnimator::DoAnimation(HotseatState old_state,
                                            HotseatState new_state) {
  if (!ShouldDoAnimation(old_state, new_state))
    return;

  SetAnimationStartProperties(old_state, new_state);
  StartAnimation(old_state, new_state);

  for (auto& observer : observers_)
    observer.OnHotseatTransitionAnimationStarted(old_state, new_state);
}

bool HotseatTransitionAnimator::ShouldDoAnimation(HotseatState old_state,
                                                  HotseatState new_state) {
  // The first HotseatState change when going to tablet mode should not be
  // animated.
  if (tablet_mode_transitioning_)
    return false;

  return (new_state == HotseatState::kShown ||
          old_state == HotseatState::kShown) &&
         Shell::Get()->tablet_mode_controller()->InTabletMode();
}

void HotseatTransitionAnimator::SetAnimationStartProperties(
    HotseatState old_state,
    HotseatState new_state) {
  // The hotseat is either changing to, or away from, the kShown hotseat.
  // If it is animating away from kShown, the animating background should
  // appear to morph from the hotseat background to the in-app shelf.
  // If the Shelf is animating to kShown, the animating background should
  // appear to morph from the in-app shelf into the hotseat background.
  const bool animate_to_shown_hotseat = new_state == HotseatState::kShown;

  gfx::Rect background_bounds;
  if (animate_to_shown_hotseat) {
    // For both kHidden and kExtended to kShown, the |animating_background_|
    // should animate from the in-ap shelf into the hotseat background in kShown
    // state.
    background_bounds = shelf_widget_->GetOpaqueBackground()->bounds();
    const int offset = ShelfConfig::Get()->shelf_size() -
                       ShelfConfig::Get()->in_app_shelf_size();
    background_bounds.Offset(0, offset);
    background_bounds.set_height(ShelfConfig::Get()->in_app_shelf_size());
  } else {
    background_bounds =
        shelf_widget_->hotseat_widget()->GetHotseatBackgroundBounds();
  }
  shelf_widget_->GetAnimatingBackground()->SetBounds(background_bounds);
  shelf_widget_->GetAnimatingBackground()->SetColor(
      animate_to_shown_hotseat ? ShelfConfig::Get()->GetMaximizedShelfColor()
                               : ShelfConfig::Get()->GetDefaultShelfColor());
  shelf_widget_->GetAnimatingBackground()->SetRoundedCornerRadius(
      animate_to_shown_hotseat ? gfx::RoundedCornersF()
                               : shelf_widget_->hotseat_widget()
                                     ->GetOpaqueBackground()
                                     ->rounded_corner_radii());
}

void HotseatTransitionAnimator::StartAnimation(HotseatState old_state,
                                               HotseatState new_state) {
  StopObservingImplicitAnimations();
  ui::ScopedLayerAnimationSettings shelf_bg_animation_setter(
      shelf_widget_->GetAnimatingBackground()->GetAnimator());
  shelf_bg_animation_setter.SetTransitionDuration(
      ShelfConfig::Get()->hotseat_background_animation_duration());
  shelf_bg_animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
  shelf_bg_animation_setter.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animation_complete_callback_ = base::BindOnce(
      &HotseatTransitionAnimator::NotifyHotseatTransitionAnimationEnded,
      weak_ptr_factory_.GetWeakPtr(), old_state, new_state);
  shelf_bg_animation_setter.AddObserver(this);

  const bool animating_to_shown_hotseat = new_state == HotseatState::kShown;
  gfx::Rect target_bounds;
  if (animating_to_shown_hotseat) {
    // The animating background should animate from in-app shelf into the
    // hotseat.
    gfx::Rect shown_hotseat_bounds_in_shelf =
        shelf_widget_->hotseat_widget()->GetHotseatBackgroundBounds();
    shown_hotseat_bounds_in_shelf.set_y(
        shelf_widget_->shelf_layout_manager()->CalculateHotseatYInShelf(
            new_state));
    target_bounds = shown_hotseat_bounds_in_shelf;
  } else {
    target_bounds = gfx::Rect(
        gfx::Point(),
        gfx::Size(shelf_widget_->GetOpaqueBackground()->bounds().size()));
  }

  shelf_widget_->GetAnimatingBackground()->SetBounds(target_bounds);
  shelf_widget_->GetAnimatingBackground()->SetRoundedCornerRadius(
      animating_to_shown_hotseat ? shelf_widget_->hotseat_widget()
                                       ->GetOpaqueBackground()
                                       ->rounded_corner_radii()
                                 : gfx::RoundedCornersF());
  shelf_widget_->GetAnimatingBackground()->SetColor(
      animating_to_shown_hotseat
          ? ShelfConfig::Get()->GetDefaultShelfColor()
          : ShelfConfig::Get()->GetMaximizedShelfColor());
}

void HotseatTransitionAnimator::NotifyHotseatTransitionAnimationEnded(
    HotseatState old_state,
    HotseatState new_state) {
  for (auto& observer : observers_)
    observer.OnHotseatTransitionAnimationEnded(old_state, new_state);
}

}  // namespace ash