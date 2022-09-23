// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm_mode/wm_mode_controller.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/status_area_widget.h"
#include "ash/wm/window_dimmer.h"
#include "ash/wm_mode/wm_mode_button_tray.h"
#include "base/check.h"
#include "base/check_op.h"

namespace ash {

namespace {

WmModeController* g_instance = nullptr;

std::unique_ptr<WindowDimmer> CreateDimmerForRoot(aura::Window* root) {
  DCHECK(root);
  DCHECK(root->IsRootWindow());

  auto dimmer = std::make_unique<WindowDimmer>(
      root->GetChildById(kShellWindowId_MenuContainer), /*animate=*/false);
  dimmer->SetDimColor(kColorAshShieldAndBase40);
  dimmer->window()->Show();
  return dimmer;
}

}  // namespace

WmModeController::WmModeController() {
  DCHECK(!g_instance);
  g_instance = this;
  Shell::Get()->AddShellObserver(this);
}

WmModeController::~WmModeController() {
  Shell::Get()->RemoveShellObserver(this);
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
WmModeController* WmModeController::Get() {
  DCHECK(g_instance);
  return g_instance;
}

void WmModeController::Toggle() {
  is_active_ = !is_active_;

  UpdateTrayButtons();
  UpdateDimmers();
}

void WmModeController::OnRootWindowAdded(aura::Window* root_window) {
  if (is_active_)
    dimmers_[root_window] = CreateDimmerForRoot(root_window);
}

void WmModeController::OnRootWindowWillShutdown(aura::Window* root_window) {
  dimmers_.erase(root_window);
}

bool WmModeController::IsRootWindowDimmedForTesting(aura::Window* root) const {
  return dimmers_.contains(root);
}

void WmModeController::UpdateDimmers() {
  if (!is_active_) {
    dimmers_.clear();
    return;
  }

  for (auto* root : Shell::GetAllRootWindows())
    dimmers_[root] = CreateDimmerForRoot(root);
}

void WmModeController::UpdateTrayButtons() {
  for (auto* root_window_controller : Shell::GetAllRootWindowControllers()) {
    if (!root_window_controller->GetRootWindow()->is_destroying()) {
      root_window_controller->GetStatusAreaWidget()
          ->wm_mode_button_tray()
          ->UpdateButtonVisuals(is_active_);
    }
  }
}

}  // namespace ash
