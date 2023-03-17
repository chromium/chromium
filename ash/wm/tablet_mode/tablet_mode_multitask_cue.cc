// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_cue.h"

#include "ash/constants/app_types.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_event_handler.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "chromeos/ui/wm/features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Cue layout values.
constexpr float kCornerRadius = 2;
constexpr int kCueWidth = 48;

// Cue timing values.
constexpr base::TimeDelta kCueDismissTimeout = base::Seconds(6);
constexpr base::TimeDelta kFadeDuration = base::Milliseconds(100);

constexpr SkColor kCueColor = SK_ColorGRAY;

}  // namespace

TabletModeMultitaskCue::TabletModeMultitaskCue() {
  DCHECK(chromeos::wm::features::IsWindowLayoutMenuEnabled());
  DCHECK(Shell::Get()->IsInTabletMode());
  Shell::Get()->activation_client()->AddObserver(this);

  // If an app window is active before switching to tablet mode, show the cue.
  if (aura::Window* active_window = window_util::GetActiveWindow()) {
    MaybeShowCue(active_window);
  }
}

TabletModeMultitaskCue::~TabletModeMultitaskCue() {
  DismissCue();
  Shell::Get()->activation_client()->RemoveObserver(this);
}

void TabletModeMultitaskCue::MaybeShowCue(aura::Window* active_window) {
  DCHECK(active_window);

  // Only show or dismiss the cue when activating app windows.
  if (static_cast<AppType>(active_window->GetProperty(
          aura::client::kAppType)) == AppType::NON_APP) {
    return;
  }

  // `UpdateCueBounds()` does not currently re-parent the layer, so it must be
  // dismissed before it can be shown again. If the user activates a floatable
  // or non-maximizable window, any existing cue should still be dismissed.
  DismissCue();

  if (!TabletModeMultitaskMenuEventHandler::CanShowMenu(active_window)) {
    return;
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
  window_observation_.Observe(window_);
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
                           &TabletModeMultitaskCue::OnTimerFinished);

  // Show the education nudge a maximum of three times with 24h in between.
  nudge_controller_.MaybeShowNudge(window_);
}

void TabletModeMultitaskCue::DismissCue(bool menu_opened) {
  cue_dismiss_timer_.Stop();
  window_observation_.Reset();

  if (window_) {
    WindowState::Get(window_)->RemoveObserver(this);
    window_ = nullptr;
  }

  cue_layer_.reset();

  // If we want to dismiss the cue because the menu was opened, let the nudge
  // know, so we don't show it anymore. Otherwise, just dismiss it.
  if (menu_opened) {
    nudge_controller_.OnMenuOpened(/*tablet_mode=*/true);
  } else {
    nudge_controller_.DismissNudge();
  }
}

void TabletModeMultitaskCue::OnWindowDestroying(aura::Window* window) {
  DismissCue();
}

void TabletModeMultitaskCue::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  UpdateCueBounds();
}

void TabletModeMultitaskCue::OnWindowActivated(ActivationReason reason,
                                               aura::Window* gained_active,
                                               aura::Window* lost_active) {
  if (!gained_active) {
    return;
  }

  auto* window_manager =
      Shell::Get()->tablet_mode_controller()->tablet_mode_window_manager();
  DCHECK(window_manager);

  auto* event_handler =
      window_manager->tablet_mode_multitask_menu_event_handler();
  DCHECK(event_handler);

  // TODO(b/263519133): Stop the cue from reappearing after using non-app
  // windows like popups.

  // The cue should not reappear when tapping off of the menu or selecting a
  // new layout.
  if (event_handler->multitask_menu() &&
      event_handler->multitask_menu()->widget()->GetNativeWindow() ==
          lost_active) {
    return;
  }

  MaybeShowCue(gained_active);
}

void TabletModeMultitaskCue::OnPostWindowStateTypeChange(
    WindowState* window_state,
    chromeos::WindowStateType old_type) {
  if (window_state->IsFloated()) {
    DismissCue();
  }
}

void TabletModeMultitaskCue::UpdateCueBounds() {
  // Needed for some edge cases where the cue is dismissed while it is being
  // updated.
  if (!window_) {
    return;
  }

  cue_layer_->SetBounds(gfx::Rect((window_->bounds().width() - kCueWidth) / 2,
                                  kCueYOffset, kCueWidth, kCueHeight));
}

void TabletModeMultitaskCue::OnTimerFinished() {
  // If no cue or the animation is already fading out, return.
  if (!cue_layer_ || cue_layer_->GetAnimator()->GetTargetOpacity() == 0.0f) {
    return;
  }

  // Fade the cue out.
  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .OnEnded(base::BindOnce(&TabletModeMultitaskCue::DismissCue,
                              base::Unretained(this), /*menu_opened=*/false))
      .Once()
      .SetDuration(kFadeDuration)
      .SetOpacity(cue_layer_.get(), 0.0f, gfx::Tween::LINEAR);
}

}  // namespace ash
