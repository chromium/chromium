// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/float/tablet_mode_tuck_education.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/rounded_label.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/property_change_reason.h"
#include "ui/display/display.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The vertical distance from the top of `window_` to the nudge.
constexpr int kNudgeYOffset = 38;
// The time it takes for the nudge to fade in or out.
constexpr base::TimeDelta kNudgeFadeDuration = base::Milliseconds(100);
// The timer after the second bounce finishes until the nudge starts to fade
// out.
constexpr base::TimeDelta kNudgeFadeOutDelay = base::Milliseconds(150);

// The maximum distance the window translates by during the bounce.
constexpr float kBounceDistance = 50.0f;
// The time after the first bounce ends and the second bounce begins.
constexpr base::TimeDelta kSecondBounceDelay = base::Seconds(1);
// The time for the window to move from its starting position to its furthest
// position.
constexpr base::TimeDelta kBounceStartDuration = base::Milliseconds(400);
// The time for the window to move from its furthest position to back to its
// original position.
constexpr base::TimeDelta kBounceEndDuration = base::Milliseconds(700);

// RoundedLabel construction values.
constexpr int kLabelPadding = 8;
constexpr int kLabelHeight = 28;
constexpr float kRoundedDivisor = 2.f;

std::unique_ptr<views::Widget> CreateWidget(aura::Window* window) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.name = "TuckEducationNudgeWidget";
  params.accept_events = false;
  params.parent = window;
  params.child = true;

  auto widget = std::make_unique<views::Widget>(std::move(params));

  auto nudge_label = std::make_unique<RoundedLabel>(
      kLabelPadding, kLabelPadding, kLabelHeight / kRoundedDivisor,
      kLabelHeight,
      l10n_util::GetStringUTF16(IDS_ASH_TUCK_EDUCATIONAL_NUDGE_LABEL));

  // TODO(hewer): Update label color to `ui::kColorSysSurface3` to match other
  // nudges.
  widget->SetContentsView(std::move(nudge_label));

  return widget;
}

}  // namespace

TabletModeTuckEducation::TabletModeTuckEducation(aura::Window* floated_window) {
  // Observe for end of floating crossfade animation to begin education.
  window_ = floated_window;
  window_observation_.Observe(window_);
}

TabletModeTuckEducation::~TabletModeTuckEducation() = default;

void TabletModeTuckEducation::OnWindowTransformed(
    aura::Window* window,
    ui::PropertyChangeReason reason) {
  // Floating a window causes a crossfade animation that can interfere with
  // other animations or actions performed on the window at the same time. We
  // observe for the crossfade to finish before performing the bounce.
  bool animating = window->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::TRANSFORM);
  if (!nudge_widget_ && !animating &&
      reason == ui::PropertyChangeReason::FROM_ANIMATION) {
    ActivateTuckEducation();
  }
}

void TabletModeTuckEducation::ActivateTuckEducation() {
  // No need to observe for the transform animation (crossfade) to finish as
  // soon as it has happened once.
  window_observation_.Reset();

  // TODO(b/275255478): Add checks so the nudge only shows 3 times max with at
  // least 24h between.

  nudge_widget_ = CreateWidget(window_);

  nudge_widget_->Show();
  auto* nudge_layer = nudge_widget_->GetLayer();
  nudge_layer->SetOpacity(0.0f);

  gfx::Size nudge_pref_size =
      nudge_widget_->GetContentsView()->GetPreferredSize();
  gfx::Rect window_bounds = window_->GetBoundsInScreen();
  gfx::Rect new_bounds((window_bounds.width() - nudge_pref_size.width()) / 2,
                       kNudgeYOffset, nudge_pref_size.width(),
                       nudge_pref_size.height());
  nudge_widget_->SetBounds(new_bounds);

  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window_);

  bool bounce_right =
      window_bounds.CenterPoint().x() > display.bounds().CenterPoint().x();

  // Move towards edge of screen.
  const gfx::Transform side_transform = gfx::Transform::MakeTranslation(
      bounce_right ? kBounceDistance : -kBounceDistance, 0);

  // Move back to starting position.
  const gfx::Transform reset_transform = gfx::Transform();

  views::AnimationBuilder()
      .OnAborted(base::BindOnce(&TabletModeTuckEducation::DismissNudge,
                                weak_factory_.GetWeakPtr()))
      .OnEnded(base::BindOnce(&TabletModeTuckEducation::DismissNudge,
                              weak_factory_.GetWeakPtr()))
      .SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION)
      // Fade nudge in.
      .Once()
      .SetDuration(kNudgeFadeDuration)
      .SetOpacity(nudge_widget_->GetLayer(), 1.0f, gfx::Tween::LINEAR)
      // First bounce.
      .Then()
      .SetDuration(kBounceStartDuration)
      .SetTransform(window_, side_transform, gfx::Tween::ACCEL_20_DECEL_100)
      .Then()
      .SetDuration(kBounceEndDuration)
      .SetTransform(window_, reset_transform, gfx::Tween::ACCEL_20_DECEL_100)
      // Delay before second bounce.
      .Then()
      .SetDuration(kSecondBounceDelay)
      // Second bounce.
      .Then()
      .SetDuration(kBounceStartDuration)
      .SetTransform(window_, side_transform, gfx::Tween::ACCEL_20_DECEL_100)
      .Then()
      .SetDuration(kBounceEndDuration)
      .SetTransform(window_, reset_transform, gfx::Tween::ACCEL_20_DECEL_100)
      // Delay before fading out.
      .Then()
      .SetDuration(kNudgeFadeOutDelay)
      // Fade nudge out.
      .Then()
      .SetDuration(kNudgeFadeDuration)
      .SetOpacity(nudge_widget_->GetLayer(), 0.0f, gfx::Tween::LINEAR);
}

void TabletModeTuckEducation::DismissNudge() {
  window_ = nullptr;

  if (nudge_widget_ && !nudge_widget_->IsClosed()) {
    nudge_widget_->GetLayer()->GetAnimator()->AbortAllAnimations();
    nudge_widget_->CloseNow();
  }

  // TODO(b/275420014): Destroy `this` once animations finish as no more actions
  // need to be performed.
}

}  // namespace ash
