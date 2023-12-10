// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_utils.h"

#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/button.h"

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

std::optional<ArcGameControlsFlag> GetGameControlsFlag(aura::Window* window) {
  if (!IsArcWindow(window)) {
    return std::nullopt;
  }

  ArcGameControlsFlag flags = window->GetProperty(kArcGameControlsFlagsKey);
  CHECK(game_dashboard_utils::IsFlagSet(flags, ArcGameControlsFlag::kKnown));

  return game_dashboard_utils::IsFlagSet(flags, ArcGameControlsFlag::kAvailable)
             ? std::make_optional<ArcGameControlsFlag>(flags)
             : std::nullopt;
}

void UpdateGameControlsHintButtonToolTipText(views::Button* button,
                                             ArcGameControlsFlag flags) {
  DCHECK(button);

  const bool is_enabled = IsFlagSet(flags, ArcGameControlsFlag::kEnabled);
  const bool is_empty = IsFlagSet(flags, ArcGameControlsFlag::kEmpty);
  const bool is_hint_on = IsFlagSet(flags, ArcGameControlsFlag::kHint);
  int tooltip_text_id;
  if (is_enabled) {
    if (is_empty) {
      tooltip_text_id = IDS_ASH_GAME_DASHBOARD_GC_TILE_TOOLTIPS_NOT_SETUP;
    } else if (is_hint_on) {
      tooltip_text_id = IDS_ASH_GAME_DASHBOARD_GC_TILE_TOOLTIPS_HIDE_CONTROLS;
    } else {
      tooltip_text_id = IDS_ASH_GAME_DASHBOARD_GC_TILE_TOOLTIPS_SHOW_CONTROLS;
    }
  } else {
    tooltip_text_id = IDS_ASH_GAME_DASHBOARD_GC_TILE_TOOLTIPS_NOT_AVAILABLE;
  }
  button->SetTooltipText(l10n_util::GetStringUTF16(tooltip_text_id));
}

bool ShouldEnableGameDashboardButton(aura::Window* window) {
  if (!IsArcWindow(window)) {
    return true;
  }

  const auto flags = window->GetProperty(kArcGameControlsFlagsKey);
  return IsFlagSet(flags, ArcGameControlsFlag::kKnown) &&
         !IsFlagSet(flags, ArcGameControlsFlag::kEdit);
}

}  // namespace ash::game_dashboard_utils
