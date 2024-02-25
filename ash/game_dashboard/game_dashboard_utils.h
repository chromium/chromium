// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_UTILS_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_UTILS_H_

#include <optional>

#include "ash/ash_export.h"
#include "ash/public/cpp/arc_game_controls_flag.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Button;
}  // namespace views

namespace ash::game_dashboard_utils {

// Returns true if `flag` is turned on for `flags`.
ASH_EXPORT bool IsFlagSet(ArcGameControlsFlag flags, ArcGameControlsFlag flag);

// Compares `new_flags` and `old_flags` and returns true if the `flag` bit has
// changed. Otherwise, returns false.
ASH_EXPORT bool IsFlagChanged(ArcGameControlsFlag new_flags,
                              ArcGameControlsFlag old_flags,
                              ArcGameControlsFlag flag);

// Returns an updated `flags` after enabling/disabling the `flag` bit.
ASH_EXPORT ArcGameControlsFlag UpdateFlag(ArcGameControlsFlag flags,
                                          ArcGameControlsFlag flag,
                                          bool enable_flag);

// Returns flags value if `window` is an ARC game window. Otherwise, it returns
// nullopt.
std::optional<ArcGameControlsFlag> GetGameControlsFlag(aura::Window* window);

// Updates Game Controls mapping hint button, such as button enabled state,
// toggled state, label text and tooltip text. `button` refers to
// `game_controls_tile_` in `GameDashboardMainMenuView` or
// `game_controls_button_` in `GameDashboardToolbarView`.
void UpdateGameControlsHintButton(views::Button* button,
                                  ArcGameControlsFlag flags);

// Returns true if `window` is not ARC game window, or Game Controls state is
// known and not in edit mode.
bool ShouldEnableGameDashboardButton(aura::Window* window);

}  // namespace ash::game_dashboard_utils

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_UTILS_H_
