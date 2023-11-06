// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/dark_mode/dark_mode_feature_pod_controller.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "base/metrics/histogram_functions.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

bool IsVisible() {
  // TODO(minch): Add the logic for login screen.
  // Disable dark mode feature pod in OOBE since only light mode should be
  // allowed there.
  return Shell::Get()->session_controller()->IsActiveUserSessionStarted() &&
         Shell::Get()->session_controller()->GetSessionState() !=
             session_manager::SessionState::OOBE;
}

}  // namespace

DarkModeFeaturePodController::DarkModeFeaturePodController(
    UnifiedSystemTrayController* tray_controller) {
  DarkLightModeControllerImpl::Get()->AddObserver(this);
}

DarkModeFeaturePodController::~DarkModeFeaturePodController() {
  DarkLightModeControllerImpl::Get()->RemoveObserver(this);
}

std::unique_ptr<FeatureTile> DarkModeFeaturePodController::CreateTile(
    bool compact) {
  DCHECK(!tile_);
  auto tile = std::make_unique<FeatureTile>(
      base::BindRepeating(&DarkModeFeaturePodController::OnIconPressed,
                          weak_ptr_factory_.GetWeakPtr()));
  tile_ = tile.get();
  tile_->SetVectorIcon(kUnifiedMenuDarkModeIcon);
  tile_->SetLabel(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_DARK_THEME));
  const bool visible = IsVisible();
  if (visible) {
    TrackVisibilityUMA();
  }
  tile_->SetVisible(visible);

  UpdateTile(DarkLightModeControllerImpl::Get()->IsDarkModeEnabled());
  return tile;
}

QsFeatureCatalogName DarkModeFeaturePodController::GetCatalogName() {
  return QsFeatureCatalogName::kDarkMode;
}

void DarkModeFeaturePodController::OnIconPressed() {
  // Toggling Dark theme feature tile inside quick settings should cancel auto
  // scheduling. This ensures that on and off states of the tile match the
  // non-scheduled states of Dark and Light buttons in personalization hub
  // respectively.
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  TrackToggleUMA(
      /*target_toggle_state=*/!dark_light_mode_controller->IsDarkModeEnabled());
  dark_light_mode_controller->SetAutoScheduleEnabled(
      /*enabled=*/false);
  dark_light_mode_controller->ToggleColorMode();
  base::UmaHistogramBoolean("Ash.DarkTheme.SystemTray.IsDarkModeEnabled",
                            dark_light_mode_controller->IsDarkModeEnabled());
}

void DarkModeFeaturePodController::OnLabelPressed() {
  return;
}

void DarkModeFeaturePodController::OnColorModeChanged(bool dark_mode_enabled) {
  UpdateTile(dark_mode_enabled);
}

void DarkModeFeaturePodController::UpdateTile(bool dark_mode_enabled) {
  tile_->SetToggled(dark_mode_enabled);
  if (Shell::Get()->dark_light_mode_controller()->GetAutoScheduleEnabled()) {
    tile_->SetSubLabel(l10n_util::GetStringUTF16(
        dark_mode_enabled
            ? IDS_ASH_STATUS_TRAY_DARK_THEME_ON_STATE_AUTO_SCHEDULED
            : IDS_ASH_STATUS_TRAY_DARK_THEME_OFF_STATE_AUTO_SCHEDULED));
  } else {
    tile_->SetSubLabel(l10n_util::GetStringUTF16(
        dark_mode_enabled ? IDS_ASH_STATUS_TRAY_DARK_THEME_ON_STATE
                          : IDS_ASH_STATUS_TRAY_DARK_THEME_OFF_STATE));
  }

  std::u16string tooltip_state = l10n_util::GetStringUTF16(
      dark_mode_enabled
          ? IDS_ASH_STATUS_TRAY_DARK_THEME_ENABLED_STATE_TOOLTIP
          : IDS_ASH_STATUS_TRAY_DARK_THEME_DISABLED_STATE_TOOLTIP);
  tile_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_DARK_THEME_TOGGLE_TOOLTIP, tooltip_state));
  tile_->SetVectorIcon(dark_mode_enabled ? kUnifiedMenuDarkModeIcon
                                         : kUnifiedMenuDarkModeOffIcon);
}

}  // namespace ash
