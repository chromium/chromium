// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/float/float_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_state.h"
#include "ash/wm/window_state.h"
#include "ash/wm/work_area_insets.h"
#include "base/check_op.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window_delegate.h"

namespace ash {

namespace {

// The ideal dimensions of a float window before factoring in its minimum size
// (if any) is the available work area multiplied by these ratios.
constexpr float kFloatWindowTabletWidthRatio = 0.3333333f;
constexpr float kFloatWindowTabletHeightRatio = 0.8f;

// The distance from the edge of the floated window to the edge of the work area
// when it is floated.
constexpr int kFloatWindowPaddingDp = 8;

bool InTabletMode() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
}

gfx::Size GetPreferredFloatWindowTabletSize(const gfx::Rect& work_area) {
  const int preferred_width =
      static_cast<int>(work_area.width() * kFloatWindowTabletWidthRatio);
  const int preferred_height =
      work_area.height() * kFloatWindowTabletHeightRatio;
  return gfx::Size(preferred_width, preferred_height);
}

}  // namespace

FloatController::FloatController() = default;

FloatController::~FloatController() = default;

// static
gfx::Rect FloatController::GetPreferredFloatWindowTabletBounds(
    aura::Window* window) {
  DCHECK(CanFloatWindowInTablet(window));
  const gfx::Rect work_area = WorkAreaInsets::ForWindow(window->GetRootWindow())
                                  ->user_work_area_bounds();
  const gfx::Size preferred_size = GetPreferredFloatWindowTabletSize(work_area);
  const gfx::Size minimum_size = window->delegate()->GetMinimumSize();

  const int width = std::max(preferred_size.width(), minimum_size.width());
  DCHECK_GT(preferred_size.height(), minimum_size.height());

  // TODO(sammiequon): This assumes the float window is to be magnetized to
  // the right. Once dragging and interactions with other window states is
  // allowed, it needs to be reworked.
  gfx::Rect float_bounds(work_area.right() - width,
                         work_area.bottom() - preferred_size.height(), width,
                         preferred_size.height());
  float_bounds.Offset(-kFloatWindowPaddingDp, -kFloatWindowPaddingDp);
  return float_bounds;
}

// static
bool FloatController::CanFloatWindowInTablet(aura::Window* window) {
  auto* window_state = WindowState::Get(window);
  if (!window_state || !window_state->CanResize())
    return false;

  const gfx::Rect work_area = WorkAreaInsets::ForWindow(window->GetRootWindow())
                                  ->user_work_area_bounds();
  const int preferred_height =
      GetPreferredFloatWindowTabletSize(work_area).height();
  const gfx::Size minimum_size = window->delegate()->GetMinimumSize();
  if (minimum_size.width() > work_area.width() / 2 ||
      minimum_size.height() > preferred_height) {
    return false;
  }
  return true;
}

bool FloatController::IsFloated(const aura::Window* window) const {
  DCHECK(window);
  return float_window_ == window;
}

void FloatController::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(float_window_, window);
  float_window_observation_.Reset();
  float_window_ = nullptr;
  tablet_mode_observation_.Reset();
  display_observer_.reset();
}

void FloatController::OnTabletModeStarting() {
  DCHECK(float_window_);
  aura::Window* floated_window = float_window_;
  if (!CanFloatWindowInTablet(floated_window))
    ResetFloatedWindow();

  MaybeUpdateWindowUIAndBoundsForTablet(floated_window);
}

void FloatController::OnTabletModeEnded() {
  DCHECK(float_window_);
  MaybeUpdateWindowUIAndBoundsForTablet(float_window_);
}

void FloatController::OnTabletControllerDestroyed() {
  tablet_mode_observation_.Reset();
}

void FloatController::OnDisplayMetricsChanged(const display::Display& display,
                                              uint32_t metrics) {
  DCHECK(float_window_);
  if ((display::DisplayObserver::DISPLAY_METRIC_WORK_AREA & metrics) == 0)
    return;
  if (!CanFloatWindowInTablet(float_window_))
    ResetFloatedWindow();
}

void FloatController::Float(aura::Window* window) {
  if (window == float_window_)
    return;

  // TODO(shidi): temporary remove the DCHECK, will implement proper trigger on
  // crbug/1339095.

  // Only one floating window is allowed, reset previously floated window.
  ResetFloatedWindow();
  DCHECK(!float_window_);
  float_window_ = window;
  float_window_observation_.Observe(float_window_);
  aura::Window* float_container =
      window->GetRootWindow()->GetChildById(kShellWindowId_FloatContainer);
  if (window->parent() != float_container)
    float_container->AddChild(window);

  tablet_mode_observation_.Observe(Shell::Get()->tablet_mode_controller());
  display_observer_.emplace(this);
  MaybeUpdateWindowUIAndBoundsForTablet(window);
}

void FloatController::Unfloat(aura::Window* window) {
  if (window != float_window_)
    return;
  //  Re-parent window to active desk container.
  desks_util::GetActiveDeskContainerForRoot(float_window_->GetRootWindow())
      ->AddChild(float_window_);
  float_window_observation_.Reset();
  float_window_ = nullptr;

  tablet_mode_observation_.Reset();
  display_observer_.reset();
  MaybeUpdateWindowUIAndBoundsForTablet(window);
}

void FloatController::ResetFloatedWindow() {
  // TODO(shidi): Remove `kWindowToggleFloatKey` and implement event trigger.
  if (float_window_)
    float_window_->SetProperty(chromeos::kWindowToggleFloatKey, false);
}

void FloatController::MaybeUpdateWindowUIAndBoundsForTablet(
    aura::Window* window) {
  DCHECK(window);

  if (!InTabletMode())
    return;

  // TODO(sophiewen): Update rounded corners and shadow.

  WindowState* window_state = WindowState::Get(window);
  DCHECK(window_state);
  TabletModeWindowState::UpdateWindowPosition(window_state, /*animate=*/true);
}

}  // namespace ash
