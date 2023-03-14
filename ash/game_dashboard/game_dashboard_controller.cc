// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_controller.h"

#include <memory>
#include <string>

#include "ash/game_dashboard/game_dashboard_session.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/logging.h"

namespace ash {

namespace {

// Gets top level window of the provided window if the top level window is not
// null. Otherwise return the window.
aura::Window* GetTopLevelWindow(aura::Window* window) {
  return window ? window->GetToplevelWindow() : nullptr;
}

}  // namespace

GameDashboardController::GameDashboardController() {
  Shell::Get()->session_controller()->AddObserver(this);
}

GameDashboardController::~GameDashboardController() {
  ShutdownAllSessions();
  Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
bool GameDashboardController::CanStart(aura::Window* window) {
  return IsArcWindow(window);
}

bool GameDashboardController::IsActive(aura::Window* window) const {
  auto it = sessions_.find(window);
  return it != sessions_.end() && !it->second->is_shutting_down();
}

bool GameDashboardController::Start(aura::Window* window) {
  window = GetTopLevelWindow(window);
  if (!window) {
    VLOG(1) << "Ignoring attempt to start game dashboard with a null window";
    return false;
  }

  if (!CanStart(window)) {
    return false;
  }

  auto& session = sessions_[window];
  if (session) {
    // Already exists.
    return false;
  }

  session = std::make_unique<GameDashboardSession>(window);
  session->Initialize();
  window_observations_.AddObservation(window);
  return true;
}

void GameDashboardController::Stop(aura::Window* window) {
  auto it_session = sessions_.find(window);
  if (it_session != sessions_.end()) {
    window_observations_.RemoveObservation(window);
    it_session->second->Shutdown();
    sessions_.erase(it_session);
  }
}

void GameDashboardController::ToggleMenu(aura::Window* window) {
  auto it_session = sessions_.find(GetTopLevelWindow(window));
  if (it_session != sessions_.end()) {
    it_session->second->ToggleMenu();
  }
}

void GameDashboardController::OnActiveUserSessionChanged(
    const AccountId& account_id) {
  ShutdownAllSessions();
}

void GameDashboardController::OnSessionStateChanged(
    session_manager::SessionState state) {
  if (Shell::Get()->session_controller()->IsUserSessionBlocked()) {
    ShutdownAllSessions();
  }
}

void GameDashboardController::OnChromeTerminating() {
  ShutdownAllSessions();
}

void GameDashboardController::OnWindowDestroying(aura::Window* window) {
  Stop(window);
}

void GameDashboardController::ShutdownAllSessions() {
  window_observations_.RemoveAllObservations();
  for (auto& it_session : sessions_) {
    if (!it_session.second->is_shutting_down()) {
      it_session.second->Shutdown();
    }
  }
  sessions_.clear();
}

}  // namespace ash
