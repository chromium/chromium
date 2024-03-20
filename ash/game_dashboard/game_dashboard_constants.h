// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONSTANTS_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONSTANTS_H_

namespace ash::game_dashboard {

// Toolbar padding from the border of the game window.
inline constexpr int kToolbarEdgePadding = 16;

// Welcome dialog border thickness.
inline constexpr int kWelcomeDialogBorderThickness = 1;

// Interior margin padding around the game window for the
// `GameDashboardWelcomeDialog`.
inline constexpr int kWelcomeDialogEdgePadding = 8;

// Welcome dialog fixed width.
inline constexpr int kWelcomeDialogFixedWidth = 360;

// Toast id when entring the tablet mode with any gaming window launched.
inline constexpr char kTabletToastId[] = "GameDashboardTabletToast";

}  // namespace ash::game_dashboard

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_CONSTANTS_H_
