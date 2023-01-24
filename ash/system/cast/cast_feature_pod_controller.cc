// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/cast/cast_feature_pod_controller.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/cast_config_controller.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/unified/feature_pod_button.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/functional/bind.h"
#include "components/access_code_cast/common/access_code_cast_metrics.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

CastFeaturePodController::CastFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {}

CastFeaturePodController::~CastFeaturePodController() {
  if (CastConfigController::Get() && (button_ || tile_))
    CastConfigController::Get()->RemoveObserver(this);
}

// static
bool CastFeaturePodController::CalculateButtonVisibility() {
  // The button is visible if there is a primary profile (e.g. after login) and
  // that profile has a media router (e.g. it is not disabled by policy).
  // QsRevamp shows the button even if there are no media sinks.
  auto* cast_config = CastConfigController::Get();
  return features::IsQsRevampEnabled()
             ? cast_config && cast_config->HasMediaRouterForPrimaryProfile()
             : cast_config &&
                   (cast_config->HasSinksAndRoutes() ||
                    cast_config->AccessCodeCastingEnabled()) &&
                   !cast_config->HasActiveRoute();
}

FeaturePodButton* CastFeaturePodController::CreateButton() {
  DCHECK(!features::IsQsRevampEnabled());
  button_ = new FeaturePodButton(this);
  button_->SetVectorIcon(kUnifiedMenuCastIcon);
  button_->SetLabel(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_SHORT));
  button_->SetIconAndLabelTooltips(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_TOOLTIP));
  button_->ShowDetailedViewArrow();
  button_->DisableLabelButtonFocus();
  button_->SetID(VIEW_ID_CAST_MAIN_VIEW);

  if (CastConfigController::Get()) {
    CastConfigController::Get()->AddObserver(this);
    CastConfigController::Get()->RequestDeviceRefresh();
  }
  // Init the button with invisible state. The `Update` method will update the
  // visibility based on the current condition.
  button_->SetVisible(false);
  Update();
  return button_;
}

std::unique_ptr<FeatureTile> CastFeaturePodController::CreateTile(
    bool compact) {
  DCHECK(features::IsQsRevampEnabled());
  auto tile = std::make_unique<FeatureTile>(
      base::BindRepeating(&CastFeaturePodController::OnIconPressed,
                          weak_factory_.GetWeakPtr()),
      /*is_togglable=*/true,
      compact ? FeatureTile::TileType::kCompact
              : FeatureTile::TileType::kPrimary);
  tile_ = tile.get();
  tile->SetID(VIEW_ID_CAST_MAIN_VIEW);

  bool target_visibility = CalculateButtonVisibility();
  if (target_visibility) {
    TrackVisibilityUMA();
  }
  tile->SetVisible(target_visibility);

  tile->SetVectorIcon(kUnifiedMenuCastIcon);
  tile->SetLabel(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST));
  const std::u16string tooltip =
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_TOOLTIP);
  tile->SetTooltipText(tooltip);

  auto* cast_config = CastConfigController::Get();
  if (cast_config) {
    cast_config->AddObserver(this);
    cast_config->RequestDeviceRefresh();
  }

  // Compact tile doesn't have a sub-label or drill-in button.
  if (compact) {
    return tile;
  }

  tile->SetSubLabel(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_DEVICES_AVAILABLE));
  tile->CreateDrillInButton(
      base::BindRepeating(&CastFeaturePodController::OnLabelPressed,
                          weak_factory_.GetWeakPtr()),
      tooltip);

  UpdateSublabelVisibility();

  return tile;
}

QsFeatureCatalogName CastFeaturePodController::GetCatalogName() {
  return QsFeatureCatalogName::kCast;
}

void CastFeaturePodController::OnIconPressed() {
  auto* cast_config = CastConfigController::Get();
  // If there are no devices currently available for the user, and they have
  // access code casting available, don't bother displaying an empty list.
  // Instead, launch directly into the access code UI so that they can begin
  // casting immediately.
  if (cast_config && !cast_config->HasSinksAndRoutes() &&
      cast_config->AccessCodeCastingEnabled()) {
    TrackToggleUMA(/*target_toggle_state=*/true);

    Shell::Get()->system_tray_model()->client()->ShowAccessCodeCastingDialog(
        AccessCodeCastDialogOpenLocation::kSystemTrayCastFeaturePod);
  } else {
    TrackDiveInUMA();
    tray_controller_->ShowCastDetailedView();
  }
}

void CastFeaturePodController::OnLabelPressed() {
  TrackDiveInUMA();

  // Clicking on the label should always launch the full UI.
  tray_controller_->ShowCastDetailedView();
}

void CastFeaturePodController::OnDevicesUpdated(
    const std::vector<SinkAndRoute>& devices) {
  if (features::IsQsRevampEnabled()) {
    if (tile_->tile_type() == FeatureTile::TileType::kCompact) {
      return;
    }
    UpdateSublabelVisibility();
  } else {
    Update();
  }
}

void CastFeaturePodController::Update() {
  DCHECK(!features::IsQsRevampEnabled());
  const bool target_visibility = CalculateButtonVisibility();
  if (!button_->GetVisible() && target_visibility) {
    TrackVisibilityUMA();
  }
  button_->SetVisible(target_visibility);
}

void CastFeaturePodController::UpdateSublabelVisibility() {
  DCHECK(features::IsQsRevampEnabled());
  DCHECK(tile_);
  auto* cast_config = CastConfigController::Get();
  bool devices_available =
      cast_config && (cast_config->HasSinksAndRoutes() ||
                      cast_config->AccessCodeCastingEnabled());
  tile_->SetSubLabelVisibility(devices_available);
}

}  // namespace ash
