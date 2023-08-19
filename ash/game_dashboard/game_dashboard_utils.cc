// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_utils.h"

#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/window_properties.h"
#include "ui/aura/window.h"

namespace ash::game_dashboard_utils {

bool IsFlagSet(ArcGameControlsFlag flags, ArcGameControlsFlag flag) {
  return (flags & flag) == flag;
}

bool IsFlagChanged(ash::ArcGameControlsFlag new_flags,
                   ash::ArcGameControlsFlag old_flags,
                   ash::ArcGameControlsFlag flag) {
  return ((new_flags ^ old_flags) & flag) == flag;
}

ArcGameControlsFlag UpdateFlag(ArcGameControlsFlag flags,
                               ArcGameControlsFlag flag,
                               bool enable_flag) {
  return static_cast<ArcGameControlsFlag>(enable_flag ? flags | flag
                                                      : flags & ~flag);
}

absl::optional<ArcGameControlsFlag> GetGameControlsFlag(aura::Window* window) {
  if (!IsArcWindow(window)) {
    return absl::nullopt;
  }

  ArcGameControlsFlag flags = window->GetProperty(kArcGameControlsFlagsKey);
  CHECK(game_dashboard_utils::IsFlagSet(flags, ArcGameControlsFlag::kKnown));

  return game_dashboard_utils::IsFlagSet(flags, ArcGameControlsFlag::kAvailable)
             ? absl::make_optional<ArcGameControlsFlag>(flags)
             : absl::nullopt;
}

}  // namespace ash::game_dashboard_utils
