// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/test_game_dashboard_delegate.h"

namespace ash {

void TestGameDashboardDelegate::GetIsGame(const std::string& app_id,
                                          IsGameCallback callback) {
  std::move(callback).Run(app_id == kGameAppId);
}

std::string TestGameDashboardDelegate::GetArcAppName(
    const std::string& app_id) const {
  return std::string();
}

void TestGameDashboardDelegate::RecordGameWindowOpenedEvent(
    aura::Window* window) {}

void TestGameDashboardDelegate::ShowResizeToggleMenu(aura::Window* window) {}

ukm::SourceId TestGameDashboardDelegate::GetUkmSourceId(
    const std::string& app_id) {
  return 123;
}

}  // namespace ash
