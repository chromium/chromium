// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle/window_cycle_tab_slider.h"

#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "base/strings/utf_string_conversions.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

namespace ash {

WindowCycleTabSlider::WindowCycleTabSlider()
    : all_desks_tab_slider_button_(
          AddChildView(std::make_unique<WindowCycleTabSliderButton>(
              base::BindRepeating(&WindowCycleTabSlider::OnModeChanged,
                                  base::Unretained(this),
                                  false),
              l10n_util::GetStringUTF16(IDS_ASH_ALT_TAB_ALL_DESKS_MODE)))),
      current_desk_tab_slider_button_(
          AddChildView(std::make_unique<WindowCycleTabSliderButton>(
              base::BindRepeating(&WindowCycleTabSlider::OnModeChanged,
                                  base::Unretained(this),
                                  true),
              l10n_util::GetStringUTF16(IDS_ASH_ALT_TAB_CURRENT_DESK_MODE)))) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(0, 0), 0));

  // All buttons should have the same width and height.
  gfx::Size common_size = all_desks_tab_slider_button_->GetPreferredSize();
  common_size.SetToMax(current_desk_tab_slider_button_->GetPreferredSize());
  all_desks_tab_slider_button_->SetPreferredSize(common_size);
  current_desk_tab_slider_button_->SetPreferredSize(common_size);

  const int tab_slider_round_radius = int{common_size.height() / 2};
  SetBackground(views::CreateRoundedRectBackground(
      AshColorProvider::Get()->GetControlsLayerColor(
          AshColorProvider::ControlsLayerType::kControlBackgroundColorInactive),
      tab_slider_round_radius));

  OnModePrefsChanged();
}

void WindowCycleTabSlider::OnModeChanged(bool per_desk) {
  // Save to the active user prefs.
  auto* prefs = Shell::Get()->session_controller()->GetActivePrefService();
  if (!prefs) {
    // Can be null in tests.
    return;
  }
  // Avoid an unnecessary update if any.
  if (per_desk == prefs->GetBoolean(prefs::kAltTabPerDesk))
    return;
  prefs->SetBoolean(prefs::kAltTabPerDesk, per_desk);
  OnModePrefsChanged();
}

void WindowCycleTabSlider::OnModePrefsChanged() {
  // Read alt-tab mode from user prefs via |IsAltTabPerActiveDesk|, which
  // handle multiple cases of different flags enabled and the number of desk.
  bool per_desk =
      Shell::Get()->window_cycle_controller()->IsAltTabPerActiveDesk();
  all_desks_tab_slider_button_->SetToggled(!per_desk);
  current_desk_tab_slider_button_->SetToggled(per_desk);
}

BEGIN_METADATA(WindowCycleTabSlider, views::View)
END_METADATA

}  // namespace ash
