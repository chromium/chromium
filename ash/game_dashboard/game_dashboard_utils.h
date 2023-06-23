// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_UTILS_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_UTILS_H_

#include "ash/public/cpp/arc_game_controls_flag.h"

namespace ash::game_dashboard_utils {

// Returns true if `checked_flag` is turned on for `flags`.
bool IsFlagSet(const ArcGameControlsFlag flags,
               const ArcGameControlsFlag checked_flag);
}  // namespace ash::game_dashboard_utils

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_UTILS_H_
