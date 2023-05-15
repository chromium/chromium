// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/test_game_dashboard_delegate.h"

namespace ash {

bool TestGameDashboardDelegate::IsGame(const std::string& app_id) const {
  return app_id == kGameAppId;
}

}  // namespace ash
