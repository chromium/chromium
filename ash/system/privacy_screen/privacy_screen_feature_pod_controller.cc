// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/privacy_screen/privacy_screen_feature_pod_controller.h"

#include <utility>

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

PrivacyScreenFeaturePodController::PrivacyScreenFeaturePodController() {
  Shell::Get()->privacy_screen_controller()->AddObserver(this);
}

PrivacyScreenFeaturePodController::~PrivacyScreenFeaturePodController() {
  Shell::Get()->privacy_screen_controller()->RemoveObserver(this);
}

std::unique_ptr<FeatureTile> PrivacyScreenFeaturePodController::CreateTile(
    bool compact) {
  DCHECK(!tile_);
  auto tile = std::make_unique<FeatureTile>(
      base::BindRepeating(&PrivacyScreenFeaturePodController::OnIconPressed,
                          weak_factory_.GetWeakPtr()));
  tile_ = tile.get();
  // `UpdateTile()` will update the visibility.
  tile_->SetVisible(false);
  UpdateTile();
  return tile;
}

QsFeatureCatalogName PrivacyScreenFeaturePodController::GetCatalogName() {
  return QsFeatureCatalogName::kPrivacyScreen;
}

void PrivacyScreenFeaturePodController::OnIconPressed() {
  TrackToggleUMA(/*target_toggle_state=*/!Shell::Get()
                     ->privacy_screen_controller()
                     ->GetEnabled());
  TogglePrivacyScreen();
}

void PrivacyScreenFeaturePodController::TogglePrivacyScreen() {
  auto* privacy_screen_controller = Shell::Get()->privacy_screen_controller();
  DCHECK(privacy_screen_controller->IsSupported());

  privacy_screen_controller->SetEnabled(
      !privacy_screen_controller->GetEnabled());
}

void PrivacyScreenFeaturePodController::UpdateTile() {
  auto* privacy_screen_controller = Shell::Get()->privacy_screen_controller();

  const bool is_supported = privacy_screen_controller->IsSupported();
  // If the button's visibility changes from invisible to visible, log its
  // visibility.
  if (!tile_->GetVisible() && is_supported) {
    TrackVisibilityUMA();
  }
  tile_->SetVisible(is_supported);
  if (!is_supported) {
    return;
  }

  const bool is_enabled = privacy_screen_controller->GetEnabled();
  const bool is_managed = privacy_screen_controller->IsManaged();

  tile_->SetVectorIcon(kPrivacyScreenIcon);
  tile_->SetToggled(is_enabled);
  tile_->SetLabel(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_LABEL));

  std::u16string tooltip_state;
  if (is_enabled) {
    tile_->SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_ON_SUBLABEL));
    tooltip_state =
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_ON_STATE);
  } else {
    tile_->SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_OFF_SUBLABEL));
    tooltip_state =
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_OFF_STATE);
  }

  if (is_managed) {
    tile_->SetSubLabel(l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_MANAGED_SUBLABEL));
  }

  tile_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_PRIVACY_SCREEN_TOOLTIP, tooltip_state));
}

void PrivacyScreenFeaturePodController::OnPrivacyScreenSettingChanged(
    bool enabled,
    bool notify_ui) {
  if (!notify_ui) {
    return;
  }
  UpdateTile();
}

}  // namespace ash
