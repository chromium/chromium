// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/float/tablet_mode_tuck_education.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/rounded_label.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/property_change_reason.h"
#include "ui/display/display.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// The vertical distance from the window header bar to the nudge.
constexpr int kNudgeYOffset = 8;
// The time it takes for the nudge to fade in or out.
constexpr base::TimeDelta kNudgeFadeDuration = base::Milliseconds(100);
// The time after the second bounce finishes until the nudge starts to fade
// out.
constexpr base::TimeDelta kNudgeFadeOutDelay = base::Seconds(1);

// The maximum distance the window translates by during the bounce.
constexpr float kBounceDistance = 50.0f;
// The delay before each bounce.
constexpr base::TimeDelta kBounceDelay = base::Seconds(1);
// The time for the window to move from its starting position to its furthest
// position.
constexpr base::TimeDelta kBounceStartDuration = base::Milliseconds(400);
// The time for the window to move from its furthest position to back to its
// original position.
constexpr base::TimeDelta kBounceEndDuration = base::Milliseconds(700);

// RoundedLabel construction values.
constexpr int kLabelHorizontalPadding = 16;
constexpr int kLabelHeight = 36;
constexpr float kRoundedDivisor = 2.f;

// The nudge will not be shown if it already been shown 3 times, or if 24 hours
// have not yet passed since it was last shown.
constexpr int kNudgeMaxShownCount = 3;
constexpr base::TimeDelta kNudgeTimeBetweenShown = base::Hours(24);

// The name of an integer pref that counts the number of times we have shown
// the nudge.
const char kTuckEducationShownCount[] =
    "ash.wm_nudge.tuck_education_nudge_count";
// The name of a time pref that stores the time we last showed the nudge.
const char kTuckEducationLastShown[] =
    "ash.wm_nudge.tuck_education_nudge_last_shown";

// Clock that can be overridden for testing.
base::Clock* g_clock_override = nullptr;

base::Time GetTime() {
  return g_clock_override ? g_clock_override->Now() : base::Time::Now();
}

std::unique_ptr<views::Widget> CreateWidget(aura::Window* window) {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_POPUP);
  params.name = "TuckEducationNudgeWidget";
  params.accept_events = false;
  params.parent = window;
  params.child = true;

  auto widget = std::make_unique<views::Widget>(std::move(params));
  widget->GetLayer()->SetFillsBoundsOpaquely(false);

  auto nudge_label = std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(IDS_ASH_TUCK_EDUCATIONAL_NUDGE_LABEL));
  nudge_label->SetBackground(views::CreateThemedRoundedRectBackground(
      ui::kColorSysSurface3, kLabelHeight / kRoundedDivisor));
  nudge_label->SetPreferredSize(gfx::Size(
      nudge_label->GetPreferredSize().width() + kLabelHorizontalPadding * 2,
      kLabelHeight));

  widget->SetContentsView(std::move(nudge_label));

  return widget;
}

}  // namespace

TabletModeTuckEducation::TabletModeTuckEducation(aura::Window* floated_window) {
  // Observe for end of floating crossfade animation to begin education.
  window_ = floated_window;
  window_observation_.Observe(window_.get());
}

TabletModeTuckEducation::~TabletModeTuckEducation() = default;

// static
void TabletModeTuckEducation::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(kTuckEducationShownCount, 0);
  registry->RegisterTimePref(kTuckEducationLastShown, base::Time());
}

// static
bool TabletModeTuckEducation::CanActivateTuckEducation() {
  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  CHECK(pref_service);
  // Nudge has already been shown three times. No need to educate anymore.
  if (pref_service->GetInteger(kTuckEducationShownCount) >=
      kNudgeMaxShownCount) {
    return false;
  }

  // Nudge has been shown within the last 24 hours already.
  if (GetTime() - pref_service->GetTime(kTuckEducationLastShown) <
      kNudgeTimeBetweenShown) {
    return false;
  }

  return true;
}

// static
void TabletModeTuckEducation::OnWindowTucked() {
  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  // Set the count to the maximum value so the education doesn't show anymore.
  pref_service->SetInteger(kTuckEducationShownCount, kNudgeMaxShownCount);
}

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

  nudge_widget_ = CreateWidget(window_);
  nudge_widget_->Show();

  auto* nudge_layer = nudge_widget_->GetLayer();
  nudge_layer->SetOpacity(0.0f);

  const gfx::Size nudge_pref_size =
      nudge_widget_->GetContentsView()->GetPreferredSize();
  const gfx::Rect window_bounds = window_->GetBoundsInScreen();

  nudge_widget_->SetBounds(gfx::Rect(
      (window_bounds.width() - nudge_pref_size.width()) / 2,
      window_->GetProperty(aura::client::kTopViewInset) + kNudgeYOffset,
      nudge_pref_size.width(), nudge_pref_size.height()));

  // Move window towards edge of screen.
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window_);
  const bool bounce_right =
      window_bounds.CenterPoint().x() > display.bounds().CenterPoint().x();
  const gfx::Transform side_transform = gfx::Transform::MakeTranslation(
      bounce_right ? kBounceDistance : -kBounceDistance, 0);

  // Start nudge at the top of the window (not animated).
  const gfx::Transform nudge_start_transform = gfx::Transform::MakeTranslation(
      0, -window_->GetProperty(aura::client::kTopViewInset));

  // Move the window back to its starting position, and the nudge to just below
  // the header bar.
  const gfx::Transform reset_transform = gfx::Transform();

  views::AnimationBuilder()
      .OnAborted(base::BindOnce(&TabletModeTuckEducation::DismissNudge,
                                weak_factory_.GetWeakPtr()))
      .OnEnded(base::BindOnce(&TabletModeTuckEducation::DismissNudge,
                              weak_factory_.GetWeakPtr()))
      .SetPreemptionStrategy(ui::LayerAnimator::ENQUEUE_NEW_ANIMATION)
      // Set initial nudge position at the top of the window.
      .Once()
      .SetTransform(nudge_layer, nudge_start_transform)
      // Fade nudge in and drop down to actual position.
      .Then()
      .SetDuration(kNudgeFadeDuration)
      .SetOpacity(nudge_layer, 1.0f, gfx::Tween::LINEAR)
      .SetTransform(nudge_layer, reset_transform)
      // Delay before first bounce.
      .Then()
      .SetDuration(kBounceDelay)
      // First bounce.
      .Then()
      .SetDuration(kBounceStartDuration)
      .SetTransform(window_, side_transform, gfx::Tween::ACCEL_20_DECEL_100)
      .Then()
      .SetDuration(kBounceEndDuration)
      .SetTransform(window_, reset_transform, gfx::Tween::ACCEL_20_DECEL_100)
      // Delay before second bounce.
      .Then()
      .SetDuration(kBounceDelay)
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
      .SetOpacity(nudge_layer, 0.0f, gfx::Tween::LINEAR);

  // Update the preferences.
  auto* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  pref_service->SetInteger(
      kTuckEducationShownCount,
      pref_service->GetInteger(kTuckEducationShownCount) + 1);
  pref_service->SetTime(kTuckEducationLastShown, base::Time());
}

void TabletModeTuckEducation::DismissNudge() {
  window_ = nullptr;

  if (nudge_widget_ && !nudge_widget_->IsClosed()) {
    nudge_widget_->GetLayer()->GetAnimator()->AbortAllAnimations();
    nudge_widget_->CloseNow();
  }
}

// static
void TabletModeTuckEducation::SetOverrideClockForTesting(
    base::Clock* test_clock) {
  g_clock_override = test_clock;
}

}  // namespace ash
