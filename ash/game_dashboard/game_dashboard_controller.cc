// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/game_dashboard/game_dashboard_context.h"
#include "ash/game_dashboard/game_dashboard_utils.h"
#include "ash/game_dashboard/game_dashboard_widget.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "base/functional/bind.h"
#include "chromeos/ui/base/window_properties.h"
#include "extensions/common/constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tracker.h"

namespace ash {

namespace {
// The singleton instance owned by `Shell`.
GameDashboardController* g_instance = nullptr;
}  // namespace

// static
GameDashboardController* GameDashboardController::Get() {
  return g_instance;
}

// static
bool GameDashboardController::IsGameWindow(aura::Window* window) {
  DCHECK(window);
  return window->GetProperty(chromeos::kIsGameKey);
}

// static
bool GameDashboardController::ReadyForAccelerator(aura::Window* window) {
  if (!IsGameWindow(window)) {
    return false;
  }

  if (IsArcWindow(window)) {
    return game_dashboard_utils::IsFlagSet(
        window->GetProperty(kArcGameControlsFlagsKey),
        ArcGameControlsFlag::kKnown);
  }

  return true;
}

GameDashboardController::GameDashboardController(
    std::unique_ptr<GameDashboardDelegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
  CHECK(aura::Env::HasInstance());
  env_observation_.Observe(aura::Env::GetInstance());
  CaptureModeController::Get()->AddObserver(this);
  Shell::Get()->overview_controller()->AddObserver(this);
}

GameDashboardController::~GameDashboardController() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
  Shell::Get()->overview_controller()->RemoveObserver(this);
  CaptureModeController::Get()->RemoveObserver(this);
}

std::string GameDashboardController::GetArcAppName(
    const std::string& app_id) const {
  return delegate_->GetArcAppName(app_id);
}

GameDashboardContext* GameDashboardController::GetGameDashboardContext(
    aura::Window* window) const {
  DCHECK(window);
  game_window_contexts_.find(window);
  auto it = game_window_contexts_.find(window);
  return it != game_window_contexts_.end() ? it->second.get() : nullptr;
}

void GameDashboardController::StartCaptureSession(
    GameDashboardContext* game_context,
    bool record_instantly) {
  CHECK(!active_recording_context_);
  auto* game_window = game_context->game_window();
  CHECK(game_window_contexts_.contains(game_window));
  auto* capture_mode_controller = CaptureModeController::Get();
  CHECK(!capture_mode_controller->is_recording_in_progress());

  active_recording_context_ = game_context;
  if (record_instantly) {
    capture_mode_controller->StartRecordingInstantlyForGameDashboard(
        game_window);
  } else {
    capture_mode_controller->StartForGameDashboard(game_window);
  }
}

void GameDashboardController::OnWindowInitialized(aura::Window* new_window) {
  auto* top_level_window = new_window->GetToplevelWindow();
  if (!top_level_window ||
      top_level_window->GetType() != aura::client::WINDOW_TYPE_NORMAL) {
    // Ignore non-NORMAL window types.
    return;
  }
  GetWindowGameState(new_window);
}

void GameDashboardController::OnWindowPropertyChanged(aura::Window* window,
                                                      const void* key,
                                                      intptr_t old) {
  if (key == kAppIDKey) {
    GetWindowGameState(window);
  }

  if (key == kArcGameControlsFlagsKey) {
    RefreshGameDashboardButton(window);
  }
}

void GameDashboardController::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  if (auto* context = GetGameDashboardContext(window)) {
    context->OnWindowBoundsChanged();
  }
}

void GameDashboardController::OnWindowDestroying(aura::Window* window) {
  window_observations_.RemoveObservation(window);
  game_window_contexts_.erase(window);
}

void GameDashboardController::OnRecordingStarted(aura::Window* current_root) {
  // Update any needed game dashboard UIs if and only if this recording started
  // from a request by a game dashboard entry point.
  for (auto const& [game_window, context] : game_window_contexts_) {
    context->OnRecordingStarted(context.get() == active_recording_context_);
  }
}

void GameDashboardController::OnRecordingEnded() {
  active_recording_context_ = nullptr;
  for (auto const& [game_window, context] : game_window_contexts_) {
    context->OnRecordingEnded();
  }
}

void GameDashboardController::OnVideoFileFinalized(
    bool user_deleted_video_file,
    const gfx::ImageSkia& thumbnail) {}

void GameDashboardController::OnRecordedWindowChangingRoot(
    aura::Window* new_root) {
  // TODO(phshah): Update any game dashboard UIs that need to change as a result
  // of the recorded window moving to a different display if and only if this
  // recording started from a request by a game dashboard entry point. If
  // nothing needs to change, leave empty.
}

void GameDashboardController::OnRecordingStartAborted() {
  OnRecordingEnded();
}

void GameDashboardController::OnOverviewModeWillStart() {
  // In overview mode, hide the Game Dashboard button, and if open, close the
  // main menu.
  for (auto const& [_, context] : game_window_contexts_) {
    context->game_dashboard_button_widget()->Hide();
    if (context->main_menu_view()) {
      context->CloseMainMenu();
    }
  }
}

void GameDashboardController::OnOverviewModeEnded() {
  // Make the Game Dashboard button visible.
  for (auto const& [_, context] : game_window_contexts_) {
    context->game_dashboard_button_widget()->Show();
  }
}

void GameDashboardController::GetWindowGameState(aura::Window* window) {
  const auto* app_id = window->GetProperty(kAppIDKey);
  if (!app_id) {
    RefreshWindowTracking(window, WindowGameState::kNotYetKnown);
  } else if (IsArcWindow(window)) {
    // For ARC apps, the "app_id" is equivalent to its package name.
    delegate_->GetIsGame(
        *app_id, base::BindOnce(&GameDashboardController::OnArcWindowIsGame,
                                weak_ptr_factory_.GetWeakPtr(),
                                std::make_unique<aura::WindowTracker>(
                                    std::vector<aura::Window*>({window}))));
  } else {
    RefreshWindowTracking(window, (*app_id == extension_misc::kGeForceNowAppId)
                                      ? WindowGameState::kGame
                                      : WindowGameState::kNotGame);
  }
}

void GameDashboardController::OnArcWindowIsGame(
    std::unique_ptr<aura::WindowTracker> window_tracker,
    bool is_game) {
  const auto windows = window_tracker->windows();
  if (windows.empty()) {
    return;
  }
  RefreshWindowTracking(
      windows[0], is_game ? WindowGameState::kGame : WindowGameState::kNotGame);
}

void GameDashboardController::RefreshWindowTracking(aura::Window* window,
                                                    WindowGameState state) {
  DCHECK(window);
  const bool is_observing = window_observations_.IsObservingSource(window);
  const bool should_observe = state != WindowGameState::kNotGame;

  if (state != WindowGameState::kNotYetKnown) {
    const bool is_game = state == WindowGameState::kGame;
    DCHECK(!window->GetProperty(chromeos::kIsGameKey) || is_game)
        << "Window property cannot change from `Game` to `Not Game`";
    window->SetProperty(chromeos::kIsGameKey, is_game);
    if (is_game) {
      auto& context = game_window_contexts_[window];
      if (!context) {
        context = std::make_unique<GameDashboardContext>(window);
        RefreshGameDashboardButton(window);
      }
    }
  }

  if (is_observing == should_observe) {
    return;
  }

  if (should_observe) {
    window_observations_.AddObservation(window);
  } else {
    window_observations_.RemoveObservation(window);
  }
}

void GameDashboardController::RefreshGameDashboardButton(aura::Window* window) {
  if (!IsArcWindow(window)) {
    return;
  }

  auto it = game_window_contexts_.find(window);
  if (it != game_window_contexts_.end()) {
    const ArcGameControlsFlag flags =
        window->GetProperty(kArcGameControlsFlagsKey);
    it->second->SetGameDashboardButtonEnabled(
        game_dashboard_utils::IsFlagSet(flags, ArcGameControlsFlag::kKnown) &&
        !game_dashboard_utils::IsFlagSet(flags, ArcGameControlsFlag::kEdit));
  }
}

}  // namespace ash
