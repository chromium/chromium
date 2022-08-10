// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/game_mode/game_mode_controller.h"

#include "ash/shell.h"
#include "base/time/time.h"
#include "chrome/browser/ash/borealis/borealis_metrics.h"
#include "chrome/browser/ash/borealis/borealis_window_manager.h"
#include "chromeos/ash/components/dbus/resourced/resourced_client.h"
#include "ui/views/widget/widget.h"

namespace game_mode {

using borealis::BorealisGameModeResult;
using borealis::BorealisWindowManager;

constexpr int kRefreshSec = 60;
constexpr int kTimeoutSec = kRefreshSec + 10;

GameModeController::GameModeController() {
  if (!ash::Shell::HasInstance())
    return;
  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->AddObserver(this);
  // In case a window is already focused when this is constructed.
  OnWindowFocused(focus_client->GetFocusedWindow(), nullptr);
}

GameModeController::~GameModeController() {
  if (ash::Shell::HasInstance())
    aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow())
        ->RemoveObserver(this);
}

void GameModeController::OnWindowFocused(aura::Window* gained_focus,
                                         aura::Window* lost_focus) {
  auto maybe_keep_focused = std::move(focused_);

  if (!gained_focus)
    return;

  auto* widget = views::Widget::GetTopLevelWidgetForNativeView(gained_focus);
  // |widget| can be nullptr in tests.
  if (!widget)
    return;

  aura::Window* window = widget->GetNativeWindow();
  auto* window_state = ash::WindowState::Get(window);

  if (window_state && BorealisWindowManager::IsBorealisWindow(window)) {
    focused_ = std::make_unique<WindowTracker>(window_state,
                                               std::move(maybe_keep_focused));
  }
}

GameModeController::WindowTracker::WindowTracker(
    ash::WindowState* window_state,
    std::unique_ptr<WindowTracker> previous_focus) {
  if (previous_focus && previous_focus->game_mode_) {
    game_mode_ = std::move(previous_focus->game_mode_);
  }
  UpdateGameModeStatus(window_state);
  window_state_observer_.Observe(window_state);
  window_observer_.Observe(window_state->window());
}

GameModeController::WindowTracker::~WindowTracker() {}

void GameModeController::WindowTracker::OnPostWindowStateTypeChange(
    ash::WindowState* window_state,
    chromeos::WindowStateType old_type) {
  UpdateGameModeStatus(window_state);
}

void GameModeController::WindowTracker::UpdateGameModeStatus(
    ash::WindowState* window_state) {
  if (!game_mode_ && window_state->IsFullscreen()) {
    game_mode_ = std::make_unique<GameModeEnabler>();
  } else if (game_mode_ && !window_state->IsFullscreen()) {
    game_mode_.reset();
  }
}

void GameModeController::WindowTracker::OnWindowDestroying(
    aura::Window* window) {
  window_state_observer_.Reset();
  window_observer_.Reset();
  game_mode_.reset();
}

bool GameModeController::GameModeEnabler::should_record_failure;

GameModeController::GameModeEnabler::GameModeEnabler() {
  GameModeEnabler::should_record_failure = true;
  RecordBorealisGameModeResultHistogram(BorealisGameModeResult::kAttempted);
  if (ash::ResourcedClient::Get()) {
    ash::ResourcedClient::Get()->SetGameModeWithTimeout(
        ash::ResourcedClient::GameMode::BOREALIS, kTimeoutSec,
        base::BindOnce(&GameModeEnabler::OnSetGameMode, false));
  }
  timer_.Start(FROM_HERE, base::Seconds(kRefreshSec), this,
               &GameModeEnabler::RefreshGameMode);
}

GameModeController::GameModeEnabler::~GameModeEnabler() {
  timer_.Stop();
  if (ash::ResourcedClient::Get()) {
    ash::ResourcedClient::Get()->SetGameModeWithTimeout(
        ash::ResourcedClient::GameMode::OFF, 0,
        base::BindOnce(&GameModeEnabler::OnSetGameMode, true));
  }
}

void GameModeController::GameModeEnabler::RefreshGameMode() {
  if (ash::ResourcedClient::Get()) {
    ash::ResourcedClient::Get()->SetGameModeWithTimeout(
        ash::ResourcedClient::GameMode::BOREALIS, kTimeoutSec,
        base::BindOnce(&GameModeEnabler::OnSetGameMode, true));
  }
}

// Previous is whether game mode was enabled previous to this call.
void GameModeController::GameModeEnabler::OnSetGameMode(
    bool was_refresh,
    absl::optional<ash::ResourcedClient::GameMode> previous) {
  if (!previous.has_value()) {
    LOG(ERROR) << "Failed to set Game Mode";
  } else if (GameModeEnabler::should_record_failure && was_refresh &&
             previous.value() != ash::ResourcedClient::GameMode::BOREALIS) {
    // If game mode was not on and it was not the initial call,
    // it means the previous call failed/timed out.
    RecordBorealisGameModeResultHistogram(BorealisGameModeResult::kFailed);
    // Only record failures once per entry into gamemode.
    GameModeEnabler::should_record_failure = false;
  }
}

}  // namespace game_mode
