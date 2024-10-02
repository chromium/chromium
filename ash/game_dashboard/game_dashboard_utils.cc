// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_utils.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/wm/overview/overview_controller.h"
#include "chromeos/ui/frame/frame_header.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view_utils.h"

namespace ash::game_dashboard_utils {

bool IsFlagSet(ArcGameControlsFlag flags, ArcGameControlsFlag flag) {
  return (flags & flag) == flag;
}

bool IsFlagChanged(ArcGameControlsFlag new_flags,
                   ArcGameControlsFlag old_flags,
                   ArcGameControlsFlag flag) {
  return ((new_flags ^ old_flags) & flag) == flag;
}

ArcGameControlsFlag UpdateFlag(ArcGameControlsFlag flags,
                               ArcGameControlsFlag flag,
                               bool enable_flag) {
  return static_cast<ArcGameControlsFlag>(enable_flag ? flags | flag
                                                      : flags & ~flag);
}

bool ShouldEnableFeatures() {
  return !OverviewController::Get()->InOverviewSession() &&
         !display::Screen::GetScreen()->InTabletMode();
}

views::Widget* GetNextWidgetToFocus(
    const std::vector<views::Widget*>& widget_list,
    const views::Widget* focused_widget,
    bool reverse) {
  if (auto it =
          std::find(widget_list.begin(), widget_list.end(), focused_widget);
      it != widget_list.end()) {
    const int focused_widget_index =
        std::distance(widget_list.begin(),
                      it);  // it - widget_list_.begin();
    const size_t widget_list_size = widget_list.size();
    const size_t next_focus_widget_index =
        (reverse ? (focused_widget_index + widget_list_size - 1u)
                 : (focused_widget_index + 1u)) %
        widget_list_size;
    return widget_list[next_focus_widget_index];
  }
  return nullptr;
}

std::optional<ArcGameControlsFlag> GetGameControlsFlag(
    const aura::Window* window) {
  if (!IsArcWindow(window)) {
    return std::nullopt;
  }

  ArcGameControlsFlag flags = window->GetProperty(kArcGameControlsFlagsKey);
  CHECK(IsFlagSet(flags, ArcGameControlsFlag::kKnown));
  return std::make_optional<ArcGameControlsFlag>(flags);
}

void UpdateGameControlsHintButton(views::Button* button,
                                  ArcGameControlsFlag flags) {
  CHECK(button);

  // Update button enabled state, toggle state, and sub-label if it is needed.
  DCHECK(IsFlagSet(flags, ArcGameControlsFlag::kKnown));
  const bool is_available = IsFlagSet(flags, ArcGameControlsFlag::kAvailable);
  const bool is_enabled = IsFlagSet(flags, ArcGameControlsFlag::kEnabled);
  const bool is_empty = IsFlagSet(flags, ArcGameControlsFlag::kEmpty);
  const bool is_hint_on = IsFlagSet(flags, ArcGameControlsFlag::kHint);

  const bool should_enable = is_available && is_enabled && !is_empty;
  const bool should_toggle = should_enable && is_hint_on;
  if (auto* feature_tile = views::AsViewClass<FeatureTile>(button)) {
    feature_tile->SetEnabled(should_enable);
    feature_tile->SetToggled(should_toggle);
    feature_tile->SetSubLabel(l10n_util::GetStringUTF16(
        should_enable ? (should_toggle ? IDS_ASH_GAME_DASHBOARD_GC_TILE_VISIBLE
                                       : IDS_ASH_GAME_DASHBOARD_GC_TILE_HIDDEN)
                      : IDS_ASH_GAME_DASHBOARD_GC_TILE_OFF));
  } else {
    auto* icon_button = views::AsViewClass<IconButton>(button);
    CHECK(icon_button);
    icon_button->SetEnabled(should_enable);
    icon_button->SetToggled(should_toggle);
  }

  // Update tooltip text.
  int tooltip_text_id;
  if (!is_available) {
    // TODO(b/274690042): Replace it with a different tooltip text once it's
    // ready.
    tooltip_text_id = IDS_ASH_GAME_DASHBOARD_GC_TILE_TOOLTIPS_NOT_AVAILABLE;
  } else if (is_enabled) {
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

bool ShouldShowWelcomeDialog() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  DCHECK(prefs) << "A valid PrefService is needed to determine whether to show "
                   "the welcome dialog.";
  return prefs->GetBoolean(prefs::kGameDashboardShowWelcomeDialog);
}

void SetShowWelcomeDialog(bool show_dialog) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  DCHECK(prefs) << "A valid PrefService is needed to update the show welcome "
                   "dialog param.";
  prefs->SetBoolean(prefs::kGameDashboardShowWelcomeDialog, show_dialog);
}

bool ShouldShowToolbar() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  DCHECK(prefs) << "A valid PrefService is needed to determine whether to show "
                   "the toolbar.";
  return prefs->GetBoolean(prefs::kGameDashboardShowToolbar);
}

void SetShowToolbar(bool show_toolbar) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  DCHECK(prefs) << "A valid PrefService is needed to update the show toolbar "
                   "param.";
  prefs->SetBoolean(prefs::kGameDashboardShowToolbar, show_toolbar);
}

int GetFrameHeaderHeight(aura::Window* window) {
  CHECK(window);
  auto* frame_header = chromeos::FrameHeader::Get(
      views::Widget::GetWidgetForNativeWindow(window));
  return (frame_header && frame_header->view()->GetVisible())
             ? frame_header->GetHeaderHeight()
             : 0;
}

void UpdateAccessibilityTree(const std::vector<views::Widget*>& widget_list) {
  const size_t widget_list_size = widget_list.size();
  if (widget_list_size <= 1u) {
    return;
  }

  for (size_t i = 0; i < widget_list_size; i++) {
    auto* contents_view = widget_list[i]->GetContentsView();
    auto& view_a11y = contents_view->GetViewAccessibility();
    const size_t prev_index = (i + widget_list_size - 1u) % widget_list_size;
    const size_t next_index = (i + 1u) % widget_list_size;

    view_a11y.SetPreviousFocus(widget_list[prev_index]);
    view_a11y.SetNextFocus(widget_list[next_index]);
    contents_view->NotifyAccessibilityEvent(ax::mojom::Event::kTreeChanged,
                                            /*send_native_event=*/true);
  }
}

}  // namespace ash::game_dashboard_utils
