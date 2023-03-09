// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_session.h"

#include "base/check.h"

namespace ash {

GameDashboardSession::GameDashboardSession(aura::Window* window)
    : window_(window) {
  DCHECK(window);
}

GameDashboardSession::~GameDashboardSession() = default;

void GameDashboardSession::Initialize() {
  // Not yet implemented
}

void GameDashboardSession::Shutdown() {
  is_shutting_down_ = true;
}

void GameDashboardSession::ToggleMenu() {
  // Not yet implemented
}

}  // namespace ash
