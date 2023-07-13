// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_UTILS_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_UTILS_H_

#include "ash/public/cpp/arc_game_controls_flag.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash::game_dashboard_utils {

// Returns true if `flag` is turned on for `flags`.
bool IsFlagSet(ArcGameControlsFlag flags, ArcGameControlsFlag flag);

// Returns flags value if Game Controls is available on `window`. Otherwise, it
// returns nullopt.
absl::optional<ArcGameControlsFlag> GetGameControlsFlag(aura::Window* window);

}  // namespace ash::game_dashboard_utils

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_UTILS_H_
