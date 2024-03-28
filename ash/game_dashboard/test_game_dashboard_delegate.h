// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_TEST_GAME_DASHBOARD_DELEGATE_H_
#define ASH_GAME_DASHBOARD_TEST_GAME_DASHBOARD_DELEGATE_H_

#include "ash/game_dashboard/game_dashboard_delegate.h"

namespace ash {

class TestGameDashboardDelegate : public GameDashboardDelegate {
 public:
  static constexpr char kGameAppId[] = "gameAppId";
  static constexpr char kAllowlistedAppId[] =
      "gihmggjjlnjaldngedmnegjmhccccahg";
  static constexpr char kOtherAppId[] = "otherAppId";

  TestGameDashboardDelegate() = default;
  TestGameDashboardDelegate(const TestGameDashboardDelegate&) = delete;
  TestGameDashboardDelegate& operator=(const TestGameDashboardDelegate&) =
      delete;
  ~TestGameDashboardDelegate() override = default;

  // GameDashboardDelegate:
  void GetIsGame(const std::string& app_id, IsGameCallback callback) override;
  std::string GetArcAppName(const std::string& app_id) const override;
  void RecordGameWindowOpenedEvent(aura::Window* window) override;
  void ShowResizeToggleMenu(aura::Window* window) override;
  ukm::SourceId GetUkmSourceId(const std::string& app_id) override;
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_TEST_GAME_DASHBOARD_DELEGATE_H_
