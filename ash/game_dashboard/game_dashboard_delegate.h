// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_DELEGATE_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_DELEGATE_H_

#include <string>

#include "ash/ash_export.h"
#include "base/functional/callback.h"

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
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_DELEGATE_H_
