// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_DELEGATE_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_DELEGATE_H_

#include <string>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

// The delegate of the `GameDashboardController` which facilitates communication
// between Ash and the browser.
class ASH_EXPORT GameDashboardDelegate {
 public:
  using IsGameCallback = base::OnceCallback<void(bool is_game)>;

  virtual ~GameDashboardDelegate() = default;

  // Checks App Service and ARC whether `app_id` is a game, and then fires
  // `callback` with true if it's a game.
  virtual void GetIsGame(const std::string& app_id,
                         IsGameCallback callback) = 0;

  // Gets the app name by `app_id`.
  virtual std::string GetArcAppName(const std::string& app_id) const = 0;

  // Records `ScalableIph::kGameWindowOpened` event.
  virtual void RecordGameWindowOpenedEvent(aura::Window* window) = 0;

  // Shows the compat mode resize toggle menu, which requires the app `window`
  // param when creating the `ResizeToggleMenu` object.
  virtual void ShowResizeToggleMenu(aura::Window* window) = 0;

  // Gets the UKM source id by `app_id`.
  virtual ukm::SourceId GetUkmSourceId(const std::string& app_id) = 0;
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_DELEGATE_H_
