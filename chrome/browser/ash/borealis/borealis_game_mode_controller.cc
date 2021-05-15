// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/borealis/borealis_game_mode_controller.h"

#include "ash/shell.h"
#include "chrome/browser/ash/borealis/borealis_service.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chromeos/dbus/resourced/resourced_client.h"
#include "ui/views/widget/widget.h"

namespace borealis {

BorealisGameModeController::BorealisGameModeController()
    : root_focus_observer_(this) {
  if (!ash::Shell::HasInstance())
    return;
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  root_focus_observer_.Observe(focus_client);
  // In case a window is already focused when this is constructed.
  OnWindowFocused(focus_client->GetFocusedWindow(), nullptr);
}

BorealisGameModeController::~BorealisGameModeController() = default;

void BorealisGameModeController::OnWindowFocused(aura::Window* gained_focus,
                                                 aura::Window* lost_focus) {
  if (!gained_focus) {
    focused_.reset();
    return;
  }

  auto* widget = views::Widget::GetTopLevelWidgetForNativeView(gained_focus);
  aura::Window* window = widget->GetNativeWindow();
  auto* window_state = ash::WindowState::Get(window);

  if (window_state && BorealisWindowManager::IsBorealisWindow(window)) {
    focused_ =
        std::make_unique<WindowTracker>(window_state, std::move(focused_));
  } else {
    focused_.reset();
  }
}

BorealisGameModeController::WindowTracker::WindowTracker(
    ash::WindowState* window_state,
    std::unique_ptr<WindowTracker> previous_focus) {
  if (previous_focus && previous_focus->game_mode_) {
    game_mode_ = std::move(previous_focus->game_mode_);
  }
  UpdateGameModeStatus(window_state);
  window_state_observer_.Observe(window_state);
  window_observer_.Observe(window_state->window());
}

BorealisGameModeController::WindowTracker::~WindowTracker() {}

void BorealisGameModeController::WindowTracker::OnPostWindowStateTypeChange(
    ash::WindowState* window_state,
    chromeos::WindowStateType old_type) {
  UpdateGameModeStatus(window_state);
}

void BorealisGameModeController::WindowTracker::UpdateGameModeStatus(
    ash::WindowState* window_state) {
  if (!game_mode_ && window_state->IsFullscreen()) {
    game_mode_ = std::make_unique<GameModeEnabler>();
  } else if (game_mode_ && !window_state->IsFullscreen()) {
    game_mode_.reset();
  }
}

void BorealisGameModeController::WindowTracker::OnWindowDestroying(
    aura::Window* window) {
  window_state_observer_.Reset();
  window_observer_.Reset();
  game_mode_.reset();
}

BorealisGameModeController::GameModeEnabler::GameModeEnabler() {
  if (chromeos::ResourcedClient::Get()) {
    chromeos::ResourcedClient::Get()->SetGameMode(
        true, base::BindOnce(&GameModeEnabler::OnSetGameMode));
  }
}

BorealisGameModeController::GameModeEnabler::~GameModeEnabler() {
  if (chromeos::ResourcedClient::Get()) {
    chromeos::ResourcedClient::Get()->SetGameMode(
        false, base::BindOnce(&GameModeEnabler::OnSetGameMode));
  }
}

// State is true if entering game mode, false if exiting.
void BorealisGameModeController::GameModeEnabler::OnSetGameMode(
    absl::optional<bool> state) {
  if (!state.has_value()) {
    LOG(ERROR) << "Failed to set Game Mode";
    // TODO(b/186184700): Remove logging for entering/exiting game mode.
  } else if (state.value()) {
    LOG(ERROR) << "Entered Game Mode";
  } else {
    LOG(ERROR) << "Exited Game Mode";
  }
}

}  // namespace borealis
