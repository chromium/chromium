// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ARC_GAME_CONTROLS_FLAG_H_
#define ASH_PUBLIC_CPP_ARC_GAME_CONTROLS_FLAG_H_

#include <stdint.h>

namespace ash {

// Represents Game Controls status.
enum ArcGameControlsFlag : uint32_t {
  // Game controls status is known if the first bit is 1.
  kKnown = 1 << 0,
  // Game controls is available if the second bit is 1.
  kAvailable = 1 << 1,
  // Game controls is empty if the third bit is 1.
  kEmpty = 1 << 2,
  // Game controls is enabled if the fourth bit is 1.
  kEnabled = 1 << 3,
  // Game controls hint is on if the fifth bit is 1.
  kHint = 1 << 4,
  // Game controls is in the edit mode if the sixth bit is 1. Edit mode means
  // "Set up" button is pressed on the Game Dashboard menu to edit Game
  // Controls.
  kEdit = 1 << 5,
  // Game controls is in the menu mode if the seventh bit is 1. Menu mode means
  // Game Dashboard related menu is active.
  kMenu = 1 << 6,
  // Game is optimized for ChromeOS if the eighth bit is 1.
  kO4C = 1 << 7,
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ARC_GAME_CONTROLS_FLAG_H_
