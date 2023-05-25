// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_controller.h"

#include <memory>
#include <string>

#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/game_dashboard/game_dashboard_context.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/window_properties.h"
#include "chromeos/ui/base/window_properties.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/window.h"

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

GameDashboardController::GameDashboardController(
    std::unique_ptr<GameDashboardDelegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
  CHECK(aura::Env::HasInstance());
  env_observation_.Observe(aura::Env::GetInstance());
  CaptureModeController::Get()->AddObserver(this);
}

GameDashboardController::~GameDashboardController() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
  CaptureModeController::Get()->RemoveObserver(this);
}

GameDashboardContext* GameDashboardController::GetGameDashboardContext(
    aura::Window* window) const {
  DCHECK(window);
  game_window_contexts_.find(window);
  auto it = game_window_contexts_.find(window);
  return it != game_window_contexts_.end() ? it->second.get() : nullptr;
}

void GameDashboardController::OnWindowInitialized(aura::Window* new_window) {
  auto* top_level_window = new_window->GetToplevelWindow();
  if (!top_level_window ||
      top_level_window->GetType() != aura::client::WINDOW_TYPE_NORMAL) {
    // Ignore non-NORMAL window types.
    return;
  }
  RefreshWindowTracking(new_window);
}

void GameDashboardController::OnWindowPropertyChanged(aura::Window* window,
                                                      const void* key,
                                                      intptr_t old) {
  if (key == kAppIDKey) {
    RefreshWindowTracking(window);
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
}

void GameDashboardController::OnRecordingEnded() {}

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
  // Reset the Gamedashboard UI state to its initial state.
}

GameDashboardController::WindowGameState
GameDashboardController::GetWindowGameState(aura::Window* window) const {
  const auto* app_id = window->GetProperty(kAppIDKey);
  if (!app_id) {
    return WindowGameState::kNotYetKnown;
  }
  const bool is_game = (IsArcWindow(window) && delegate_->IsGame(*app_id)) ||
                       (*app_id == extension_misc::kGeForceNowAppId);
  return is_game ? WindowGameState::kGame : WindowGameState::kNotGame;
}

void GameDashboardController::RefreshWindowTracking(aura::Window* window) {
  const bool is_observing = window_observations_.IsObservingSource(window);
  const auto state = GetWindowGameState(window);
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

}  // namespace ash
