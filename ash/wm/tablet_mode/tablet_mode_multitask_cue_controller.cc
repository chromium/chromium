// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_cue_controller.h"

#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/display/tablet_state.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Used to set the rounded corners of the cue.
constexpr float kCornerRadius = 2;

// Cue timing values.
constexpr base::TimeDelta kCueDismissTimeout = base::Seconds(6);
constexpr base::TimeDelta kFadeDuration = base::Milliseconds(100);

constexpr SkColor kCueColor = SK_ColorGRAY;

}  // namespace

TabletModeMultitaskCueController::TabletModeMultitaskCueController() {
  CHECK(Shell::Get()->IsInTabletMode());
  Shell::Get()->activation_client()->AddObserver(this);

  // If an app window is active before switching to tablet mode, show the cue.
  if (aura::Window* active_window = window_util::GetActiveWindow()) {
    MaybeShowCue(active_window);
  }
}

TabletModeMultitaskCueController::~TabletModeMultitaskCueController() {
  DismissCue();
  Shell::Get()->activation_client()->RemoveObserver(this);
}

void TabletModeMultitaskCueController::MaybeShowCue(
    aura::Window* active_window) {
  DCHECK(active_window);

  if (!CanShowCue(active_window)) {
    return;
  }

  // `UpdateCueBounds()` does not currently re-parent the layer, so it must be
  // dismissed before it can be shown again. If the user activates a floatable
  // or non-maximizable window, any existing cue should still be dismissed.
  DismissCue();

  if (!TabletModeMultitaskMenuController::CanShowMenu(active_window)) {
    return;
  }

  if (pre_cue_shown_callback_for_test_) {
    std::move(pre_cue_shown_callback_for_test_).Run();
  }

  window_ = active_window;

  cue_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  cue_layer_->SetColor(kCueColor);
  cue_layer_->SetRoundedCornerRadius(gfx::RoundedCornersF(kCornerRadius));
  cue_layer_->SetOpacity(0.0f);

  window_->layer()->Add(cue_layer_.get());

  UpdateCueBounds();

  // Observe `window_` to update the cue if the window gets destroyed, its
  // bounds change, or its state type changes (e.g., is floated).
  window_observation_.Observe(window_.get());
  WindowState::Get(window_)->AddObserver(this);

  // Because `DismissCue()` is called beforehand, there should not be any
  // animation currently running.
  DCHECK(!cue_layer_->GetAnimator()->is_animating());

  // Fade the cue in.
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kFadeDuration)
      .SetOpacity(cue_layer_.get(), 1.0f, gfx::Tween::LINEAR);

  cue_dismiss_timer_.Start(FROM_HERE, kCueDismissTimeout, this,
                           &TabletModeMultitaskCueController::OnTimerFinished);

  // Show the education nudge a maximum of three times with 24h in between.
  nudge_controller_.MaybeShowNudge(window_);
}

bool TabletModeMultitaskCueController::CanShowCue(aura::Window* window) const {
  // The cue may interfere with some integration tests.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshNoNudges)) {
    return false;
  }

  // When we go back to clamshell mode while in single split view (only one
  // window open), overview mode will shutdown and try to restore activation
  // to the window, and therefore call `MaybeShowCue()`. We do not want the cue
  // and nudge to show in this case.
  if (Shell::Get()->display_manager()->GetTabletState() ==
      display::TabletState::kExitingTabletMode) {
    return false;
  }

  // Only show or dismiss the cue when activating app windows.
  if (window->GetProperty(chromeos::kAppTypeKey) ==
      chromeos::AppType::NON_APP) {
    return false;
  }

  // Should not refresh cue if `window_` is reactivated.
  if (window == window_) {
    return false;
  }

  return true;
}

void TabletModeMultitaskCueController::DismissCue() {
  cue_dismiss_timer_.Stop();
  window_observation_.Reset();

  if (window_) {
    WindowState::Get(window_)->RemoveObserver(this);
    window_ = nullptr;
  }

  cue_layer_.reset();
  nudge_controller_.DismissNudge();
}

void TabletModeMultitaskCueController::OnMenuOpened(
    aura::Window* active_window) {
  if (cue_layer_ && window_ != active_window) {
    MaybeShowCue(active_window);
  }
  nudge_controller_.OnMenuOpened(/*tablet_mode=*/true);
}

void TabletModeMultitaskCueController::OnWindowDestroying(
    aura::Window* window) {
  DismissCue();
}

void TabletModeMultitaskCueController::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  UpdateCueBounds();
}

void TabletModeMultitaskCueController::OnWindowActivated(
    ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (!gained_active) {
    return;
  }

  // Do not show the cue if the window losing activation is a popup for example,
  // and is on the same transient tree as the window gaining activation. For
  // example, when activation goes from the browsers extension bubble to the
  // browser, the cue will not appear.
  if (lost_active &&
      lost_active->GetType() != aura::client::WINDOW_TYPE_NORMAL &&
      wm::GetTransientRoot(lost_active) ==
          wm::GetTransientRoot(gained_active)) {
    return;
  }

  auto* window_manager =
      Shell::Get()->tablet_mode_controller()->tablet_mode_window_manager();
  DCHECK(window_manager);

  auto* multitask_menu_controller =
      window_manager->tablet_mode_multitask_menu_controller();
  DCHECK(multitask_menu_controller);

  // The cue should not reappear when tapping off of the menu onto `window_`
  // or selecting a new layout. In the case where the menu is open, the cue is
  // active, and we tap onto another window (e.g., split view), we still want to
  // show the cue on the new window.
  // TODO(b/279816982): Fix this so the cue does not appear when the menu is
  // dismissed by swiping up or not dragging far enough.
  if (multitask_menu_controller->multitask_menu() &&
      multitask_menu_controller->multitask_menu()
              ->widget()
              ->GetNativeWindow() == lost_active) {
    return;
  }

  MaybeShowCue(gained_active);
}

void TabletModeMultitaskCueController::OnPostWindowStateTypeChange(
    WindowState* window_state,
    chromeos::WindowStateType old_type) {
  if (!TabletModeMultitaskMenuController::CanShowMenu(window_state->window())) {
    DismissCue();
  }
}

void TabletModeMultitaskCueController::UpdateCueBounds() {
  // Needed for some edge cases where the cue is dismissed while it is being
  // updated.
  if (!window_) {
    return;
  }

  cue_layer_->SetBounds(gfx::Rect((window_->bounds().width() - kCueWidth) / 2,
                                  kCueYOffset, kCueWidth, kCueHeight));
}

void TabletModeMultitaskCueController::OnTimerFinished() {
  // If no cue or the animation is already fading out, return.
  if (!cue_layer_ || cue_layer_->GetAnimator()->GetTargetOpacity() == 0.0f) {
    return;
  }

  // Fade the cue out.
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(&TabletModeMultitaskCueController::DismissCue,
                              base::Unretained(this)))
      .Once()
      .SetDuration(kFadeDuration)
      .SetOpacity(cue_layer_.get(), 0.0f, gfx::Tween::LINEAR);
}

}  // namespace ash
