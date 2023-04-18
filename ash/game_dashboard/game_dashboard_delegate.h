// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_DELEGATE_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_DELEGATE_H_

#include <string>

#include "ash/ash_export.h"

namespace ash {

// The delegate of the `GameDashboardController` which facilitates communication
// between Ash and the browser.
class ASH_EXPORT GameDashboardDelegate {
 public:
  virtual ~GameDashboardDelegate() = default;

  // Returns true if the given appId corresponds to a game.
  virtual bool IsGame(const std::string& app_id) const = 0;
};

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_DELEGATE_H_
