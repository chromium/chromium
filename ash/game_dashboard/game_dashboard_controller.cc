// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_controller.h"

#include <memory>

#include "ash/public/cpp/app_types_util.h"
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

GameDashboardController::GameDashboardController(
    std::unique_ptr<GameDashboardDelegate> delegate)
    : delegate_(std::move(delegate)) {
  DCHECK_EQ(g_instance, nullptr);
  g_instance = this;
}

GameDashboardController::~GameDashboardController() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

bool GameDashboardController::IsSupported(aura::Window* window) const {
  // TODO(phshah): Add support for more game surfaces.
  return IsArcWindow(window);
}

}  // namespace ash
