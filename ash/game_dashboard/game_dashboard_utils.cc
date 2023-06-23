// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_utils.h"

namespace ash::game_dashboard_utils {

bool IsFlagSet(const ArcGameControlsFlag flags,
               const ArcGameControlsFlag checked_flag) {
  return (flags & checked_flag) != 0;
}

}  // namespace ash::game_dashboard_utils
