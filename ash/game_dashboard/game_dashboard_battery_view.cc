// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_battery_view.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/style/ash_color_id.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_class_properties.h"

namespace ash {

GameDashboardBatteryView::GameDashboardBatteryView() {
  // Initialize the battery view.
  SetSize(gfx::Size(20, 20));
  SetProperty(views::kMarginsKey, gfx::Insets());

  // Add an observer to the device's Power Status service and
  // connect it to the battery icon.
  PowerStatus::Get()->AddObserver(this);
}

GameDashboardBatteryView::~GameDashboardBatteryView() {
  PowerStatus::Get()->RemoveObserver(this);
}

void GameDashboardBatteryView::OnThemeChanged() {
  views::View::OnThemeChanged();
  // Assume the icon color has changed, so as to trigger an icon color update.
  UpdateStatus(/*theme_changed=*/true);
}

void GameDashboardBatteryView::OnPowerStatusChanged() {
  UpdateStatus();
}

void GameDashboardBatteryView::UpdateStatus(bool theme_changed) {
  MaybeUpdateImage(theme_changed);
  SetVisible(PowerStatus::Get()->IsBatteryPresent());
  GetViewAccessibility().SetName(
      PowerStatus::Get()->GetAccessibleNameString(/*full_description=*/true));
}

void GameDashboardBatteryView::MaybeUpdateImage(bool theme_changed) {
  auto* color_provider = GetColorProvider();
  SkColor icon_fg_color = color_provider->GetColor(kColorAshIconColorPrimary);
  std::optional<SkColor> badge_color =
      color_provider->GetColor(kColorAshIconColorPrimary);

  if (features::IsBatterySaverAvailable() &&
      PowerStatus::Get()->IsBatterySaverActive() &&
      cros_styles::DarkModeEnabled()) {
    icon_fg_color = gfx::kGoogleYellow700;
    badge_color = gfx::kGoogleYellow700;
  }

  auto image_info =
      PowerStatus::Get()->GenerateBatteryImageInfo(icon_fg_color, badge_color);

  if (!theme_changed && battery_image_info_ &&
      battery_image_info_->ApproximatelyEqual(image_info)) {
    return;
  }
  battery_image_info_ = image_info;
  SetImage(PowerStatus::GetBatteryImage(battery_image_info_.value(),
                                        kUnifiedTrayBatteryIconSize,
                                        color_provider));
}

BEGIN_METADATA(GameDashboardBatteryView)
END_METADATA

}  // namespace ash
