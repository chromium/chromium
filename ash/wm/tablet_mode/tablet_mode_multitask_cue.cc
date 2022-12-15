// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_cue.h"

#include "ash/constants/app_types.h"
#include "ash/shell.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "chromeos/ui/wm/features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Cue layout values.
constexpr float kCornerRadius = 2;
constexpr int kCueYOffset = 6;
constexpr int kCueWidth = 48;
constexpr int kCueHeight = 4;

}  // namespace

TabletModeMultitaskCue::TabletModeMultitaskCue() {
  DCHECK(chromeos::wm::features::IsFloatWindowEnabled());
  Shell::Get()->activation_client()->AddObserver(this);
}

TabletModeMultitaskCue::~TabletModeMultitaskCue() {
  DismissCueInternal();
  Shell::Get()->activation_client()->RemoveObserver(this);
}

void TabletModeMultitaskCue::OnWindowActivated(ActivationReason reason,
                                               aura::Window* gained_active,
                                               aura::Window* lost_active) {
  if (!gained_active)
    return;

  // Only show the cue on app windows.
  // TODO(hewer): Review and update logic when `gained_active` is a NON_APP
  // window and `lost_active` is an app.
  if (static_cast<AppType>(gained_active->GetProperty(
          aura::client::kAppType)) == AppType::NON_APP) {
    return;
  }

  // `UpdateCueInternal()` does not currently re-parent the layer, so it must be
  // dismissed before it can be shown again. May change when animations are
  // implemented.
  DismissCueInternal();

  // Floated windows do not have the multitask menu.
  // TODO(hewer): Consolidate checks with ones for multitask menu in a helper.
  WindowState* state = WindowState::Get(gained_active);
  if (state->IsFloated() || !state->CanMaximize())
    return;

  window_ = gained_active;

  cue_layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  cue_layer_->SetColor(SK_ColorGRAY);
  cue_layer_->SetRoundedCornerRadius(gfx::RoundedCornersF(kCornerRadius));

  window_->layer()->Add(cue_layer_.get());

  UpdateCueBounds();

  // Observe `window_` to update the cue if the window gets destroyed, its
  // bounds change, or its state type changes (e.g., is floated).
  window_observation_.Observe(window_);
  state->AddObserver(this);
}

void TabletModeMultitaskCue::OnWindowDestroying(aura::Window* window) {
  DismissCueInternal();
}

void TabletModeMultitaskCue::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  UpdateCueBounds();
}

void TabletModeMultitaskCue::OnPostWindowStateTypeChange(
    WindowState* window_state,
    chromeos::WindowStateType old_type) {
  if (window_state->IsFloated())
    DismissCueInternal();
}

void TabletModeMultitaskCue::DismissCueInternal() {
  window_observation_.Reset();

  if (window_) {
    WindowState::Get(window_)->RemoveObserver(this);
    window_ = nullptr;
  }

  cue_layer_.reset();
}

void TabletModeMultitaskCue::UpdateCueBounds() {
  // Needed for some edge cases where the cue is dismissed while it is being
  // updated.
  if (!window_)
    return;

  cue_layer_->SetBounds(gfx::Rect((window_->bounds().width() - kCueWidth) / 2,
                                  kCueYOffset, kCueWidth, kCueHeight));
}

}  // namespace ash