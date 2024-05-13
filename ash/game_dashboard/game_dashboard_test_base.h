// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_TEST_BASE_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_TEST_BASE_H_

#include "ash/game_dashboard/game_dashboard_context.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/window_observer.h"

namespace ash {

// Base class for GameDashboard related unittests, and contains common functions
class GameDashboardTestBase : public AshTestBase {
 public:
  // The bounds for the screen that will contain app windows.
  static constexpr gfx::Rect kScreenBounds = gfx::Rect(10, 10, 2000, 1500);

  GameDashboardTestBase();
  GameDashboardTestBase(const GameDashboardTestBase&) = delete;
  GameDashboardTestBase& operator=(const GameDashboardTestBase&) = delete;
  ~GameDashboardTestBase() override = default;

  // AshTestBase:
  void SetUp() override;

  void AdvanceClock(base::TimeDelta delta);

 protected:
  // Returns true if the `GameDashboardController` is observing the `window`.
  bool IsControllerObservingWindow(aura::Window* window) const;

  // Creates an app window for the given set of `app_id`, `app_type`, and window
  // bounds.
  std::unique_ptr<aura::Window> CreateAppWindow(
      const std::string& app_id,
      chromeos::AppType app_type = chromeos::AppType::NON_APP,
      const gfx::Rect& bounds_in_screen = gfx::Rect());

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// A window property observer that sets `received_on_property_change_` to true
// when `chromeos::kIsGameKey` changes.
class IsGameWindowPropertyObserver : public aura::WindowObserver {
 public:
  explicit IsGameWindowPropertyObserver(aura::Window* window);
  IsGameWindowPropertyObserver(const IsGameWindowPropertyObserver&) = delete;
  IsGameWindowPropertyObserver& operator=(const IsGameWindowPropertyObserver&) =
      delete;
  ~IsGameWindowPropertyObserver() override;

  bool received_on_property_change() const {
    return received_on_property_change_;
  }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

 private:
  bool received_on_property_change_ = false;
  raw_ptr<aura::Window> window_;
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_TEST_BASE_H_
