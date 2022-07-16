// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_nudge_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/tray/system_nudge.h"
#include "base/bind.h"
#include "base/time/time.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {
constexpr base::TimeDelta kNudgeShowTime = base::Seconds(10);
constexpr float kNudgeFadeAnimationScale = 1.2f;
constexpr base::TimeDelta kNudgeFadeAnimationTime = base::Milliseconds(250);
constexpr gfx::Tween::Type kNudgeFadeOpacityAnimationTweenType =
    gfx::Tween::LINEAR;
constexpr gfx::Tween::Type kNudgeFadeScalingAnimationTweenType =
    gfx::Tween::LINEAR_OUT_SLOW_IN;
}  // namespace

// A class for observing the nudge fade out animation. Once the fade
// out animation is complete the nudge will be destroyed.
class ImplicitNudgeHideAnimationObserver
    : public ui::ImplicitAnimationObserver {
 public:
  ImplicitNudgeHideAnimationObserver(std::unique_ptr<SystemNudge> nudge,
                                     SystemNudgeController* controller)
      : nudge_(std::move(nudge)), controller_(controller) {
    DCHECK(nudge_);
    DCHECK(controller_);
  }
  ImplicitNudgeHideAnimationObserver(
      const ImplicitNudgeHideAnimationObserver&) = delete;
  ImplicitNudgeHideAnimationObserver& operator=(
      const ImplicitNudgeHideAnimationObserver&) = delete;
  ~ImplicitNudgeHideAnimationObserver() override {
    StopObservingImplicitAnimations();
    nudge_->Close();
  }

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override {
    // |this| is deleted by the controller which owns the observer.
    controller_->ForceCloseAnimatingNudge();
  }

 private:
  std::unique_ptr<SystemNudge> nudge_;
  // Owned by the shell.
  SystemNudgeController* const controller_;
};

SystemNudgeController::SystemNudgeController() = default;

SystemNudgeController::~SystemNudgeController() {
  hide_nudge_animation_observer_.reset();
}

void SystemNudgeController::ShowNudge() {
  if (nudge_ && !nudge_->widget()->IsClosed()) {
    hide_nudge_timer_.AbandonAndStop();
    nudge_->Close();
  }

  // Create and show the nudge.
  nudge_ = CreateSystemNudge();
  nudge_->Show();
  StartFadeAnimation(/*show=*/true);

  // Start a timer to close the nudge after a set amount of time.
  hide_nudge_timer_.Start(FROM_HERE, kNudgeShowTime,
                          base::BindOnce(&SystemNudgeController::HideNudge,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void SystemNudgeController::ForceCloseAnimatingNudge() {
  hide_nudge_animation_observer_.reset();
}

void SystemNudgeController::FireHideNudgeTimerForTesting() {
  hide_nudge_timer_.FireNow();
}

void SystemNudgeController::HideNudge() {
  StartFadeAnimation(/*show=*/false);
}

void SystemNudgeController::StartFadeAnimation(bool show) {
  // Clean any pending animation observer.
  hide_nudge_animation_observer_.reset();

  ui::Layer* layer = nudge_->widget()->GetLayer();
  gfx::Rect widget_bounds = layer->bounds();

  gfx::Transform scaled_nudge_transform;
  float x_offset =
      widget_bounds.width() * (1.0f - kNudgeFadeAnimationScale) / 2.0f;
  float y_offset =
      widget_bounds.height() * (1.0f - kNudgeFadeAnimationScale) / 2.0f;
  scaled_nudge_transform.Translate(x_offset, y_offset);
  scaled_nudge_transform.Scale(kNudgeFadeAnimationScale,
                               kNudgeFadeAnimationScale);

  layer->SetOpacity(show ? 0.0f : 1.0f);
  layer->SetTransform(show ? scaled_nudge_transform : gfx::Transform());

  {
    // Perform the scaling animation on the nudge.
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(kNudgeFadeAnimationTime);
    settings.SetTweenType(kNudgeFadeScalingAnimationTweenType);
    layer->SetTransform(show ? gfx::Transform() : scaled_nudge_transform);
  }
  {
    // Perform the opacity animation on the nudge.
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.SetTransitionDuration(kNudgeFadeAnimationTime);
    settings.SetTweenType(kNudgeFadeOpacityAnimationTweenType);
    layer->SetOpacity(show ? 1.0f : 0.0f);
    if (!show) {
      hide_nudge_animation_observer_ =
          std::make_unique<ImplicitNudgeHideAnimationObserver>(
              std::move(nudge_), this);
      settings.AddObserver(hide_nudge_animation_observer_.get());
    }
  }
}

}  // namespace ash
