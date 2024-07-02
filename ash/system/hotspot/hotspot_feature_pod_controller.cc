// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/hotspot/hotspot_feature_pod_controller.h"

#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/hotspot_config_service.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/hotspot/hotspot_icon.h"
#include "ash/system/hotspot/hotspot_icon_animation.h"
#include "ash/system/hotspot/hotspot_info_cache.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view_class_properties.h"

namespace ash {

using hotspot_config::mojom::HotspotAllowStatus;
using hotspot_config::mojom::HotspotControlResult;
using hotspot_config::mojom::HotspotInfo;
using hotspot_config::mojom::HotspotInfoPtr;
using hotspot_config::mojom::HotspotState;

HotspotFeaturePodController::HotspotFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : hotspot_info_(Shell::Get()->hotspot_info_cache()->GetHotspotInfo()),
      tray_controller_(tray_controller) {
  GetHotspotConfigService(
      remote_cros_hotspot_config_.BindNewPipeAndPassReceiver());
  remote_cros_hotspot_config_->AddObserver(
      hotspot_config_observer_receiver_.BindNewPipeAndPassRemote());
}

HotspotFeaturePodController::~HotspotFeaturePodController() {
  Shell::Get()->hotspot_icon_animation()->RemoveObserver(this);
}


std::unique_ptr<FeatureTile> HotspotFeaturePodController::CreateTile(
    bool compact) {
  auto tile = std::make_unique<FeatureTile>(
      base::BindRepeating(&HotspotFeaturePodController::OnLabelPressed,
                          weak_ptr_factory_.GetWeakPtr()));
  tile_ = tile.get();
  tile_->SetIconClickable(true);
  tile_->SetIconClickCallback(
      base::BindRepeating(&HotspotFeaturePodController::OnIconPressed,
                          weak_ptr_factory_.GetWeakPtr()));
  tile_->SetLabel(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_HOTSPOT));
  tile_->CreateDecorativeDrillInArrow();
  tile_->drill_in_arrow()->SetProperty(
      views::kElementIdentifierKey, kHotspotFeatureTileDrillInArrowElementId);

  // Default the visibility to false and update it in `UpdateTileState()` since
  // it should only be shown if user has used the Hotspot from Settings before.
  tile_->SetVisible(false);
  UpdateTileState();
  return tile;
}

QsFeatureCatalogName HotspotFeaturePodController::GetCatalogName() {
  return QsFeatureCatalogName::kHotspot;
}

void HotspotFeaturePodController::OnIconPressed() {
  if (!tile_->GetEnabled()) {
    return;
  }

  if (hotspot_info_->state == HotspotState::kEnabled) {
    remote_cros_hotspot_config_->DisableHotspot(base::BindOnce(
        &HotspotFeaturePodController::TrackToggleHotspotUMA,
        weak_ptr_factory_.GetWeakPtr(), /*target_toggle_state=*/false));
    return;
  }

  EnableHotspotIfAllowedAndDiveIn();
}

void HotspotFeaturePodController::OnLabelPressed() {
  if (!tile_->GetEnabled()) {
    return;
  }

  TrackDiveInUMA();
  tray_controller_->ShowHotspotDetailedView();
}

void HotspotFeaturePodController::OnHotspotInfoChanged() {
  remote_cros_hotspot_config_->GetHotspotInfo(
      base::BindOnce(&HotspotFeaturePodController::OnGetHotspotInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

void HotspotFeaturePodController::OnGetHotspotInfo(
    HotspotInfoPtr hotspot_info) {
  if (hotspot_info->state == HotspotState::kEnabling) {
    Shell::Get()->hotspot_icon_animation()->AddObserver(this);
  } else if (hotspot_info_ && hotspot_info_->state == HotspotState::kEnabling) {
    Shell::Get()->hotspot_icon_animation()->RemoveObserver(this);
  }
  hotspot_info_ = std::move(hotspot_info);

  UpdateTileState();
}

void HotspotFeaturePodController::UpdateTileState() {
  if (!tile_) {
    return;
  }
  if (!Shell::Get()->hotspot_info_cache()->HasHotspotUsedBefore()) {
    return;
  }

  // If the tile's visibility changes from invisible to visible, log its
  // visibility.
  if (!tile_->GetVisible()) {
    TrackVisibilityUMA();
  }

  tile_->SetVisible(true);
  tile_->SetEnabled(true);
  tile_->SetToggled(hotspot_info_->state != HotspotState::kDisabled);
  tile_->SetSubLabel(ComputeSublabel());
  tile_->SetIconButtonTooltipText(ComputeIconTooltip());
  tile_->SetTooltipText(ComputeTileTooltip());
  tile_->SetVectorIcon(hotspot_icon::GetIconForHotspot(hotspot_info_->state));
}

void HotspotFeaturePodController::HotspotIconChanged() {
  tile_->SetVectorIcon(hotspot_icon::GetIconForHotspot(hotspot_info_->state));
}

void HotspotFeaturePodController::EnableHotspotIfAllowedAndDiveIn() {
  if (hotspot_info_->state == HotspotState::kDisabled &&
      hotspot_info_->allow_status == HotspotAllowStatus::kAllowed) {
    remote_cros_hotspot_config_->EnableHotspot(base::BindOnce(
        &HotspotFeaturePodController::TrackToggleHotspotUMA,
        weak_ptr_factory_.GetWeakPtr(), /*target_toggle_state=*/true));
  }

  TrackDiveInUMA();
  tray_controller_->ShowHotspotDetailedView();
}

void HotspotFeaturePodController::TrackToggleHotspotUMA(
    bool target_toggle_state,
    HotspotControlResult operation_result) {
  TrackToggleUMA(/*target_toggle_state=*/target_toggle_state);
}

const gfx::VectorIcon& HotspotFeaturePodController::ComputeIcon() const {
  return tile_->IsToggled() ? kHotspotOnIcon : kHotspotOffIcon;
}

std::u16string HotspotFeaturePodController::ComputeSublabel() const {
  using l10n_util::GetStringUTF16;

  switch (hotspot_info_->state) {
    case HotspotState::kEnabled:
      return GetStringUTF16(IDS_ASH_STATUS_TRAY_HOTSPOT_STATUS_ON);
    case HotspotState::kEnabling:
      return GetStringUTF16(IDS_ASH_STATUS_TRAY_HOTSPOT_STATUS_ENABLING);
    case HotspotState::kDisabling:
      return GetStringUTF16(IDS_ASH_STATUS_TRAY_HOTSPOT_STATUS_DISABLING);
    case HotspotState::kDisabled:
      return GetStringUTF16(IDS_ASH_STATUS_TRAY_HOTSPOT_STATUS_OFF);
  }
}

std::u16string HotspotFeaturePodController::ComputeIconTooltip() const {
  switch (hotspot_info_->state) {
    case HotspotState::kEnabled: {
      // Toggle hotspot. Hotspot is on, # devices connected.
      uint32_t client_count = hotspot_info_->client_count;
      if (client_count == 0) {
        return l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_HOTSPOT_FEATURE_TILE_TOOLTIP_STATUS_ON_NO_DEVICE_CONNECTED);
      }
      if (client_count == 1) {
        return l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_HOTSPOT_FEATURE_TILE_TOOLTIP_STATUS_ON_ONE_DEVICE_CONNECTED);
      }
      return l10n_util::GetStringFUTF16(
          IDS_ASH_STATUS_TRAY_HOTSPOT_FEATURE_TILE_TOOLTIP_STATUS_ON_MULTIPLE_DEVICES_CONNECTED,
          base::NumberToString16(client_count));
    }
    case HotspotState::kEnabling:
      // Show hotspot details. Hotspot is enabling.
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_HOTSPOT_FEATURE_TILE_TOOLTIP_STATUS_ENABLING);
    case HotspotState::kDisabling:
      // Show hotspot details. Hotspot is disabling.
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_HOTSPOT_FEATURE_TILE_TOOLTIP_STATUS_DISABLING);
    case HotspotState::kDisabled:
      // Show hotspot details. Connect to mobile data to use hotspot.
      if (hotspot_info_->allow_status ==
          HotspotAllowStatus::kDisallowedNoMobileData) {
        return l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_HOTSPOT_FEATURE_TILE_TOOLTIP_NO_MOBILE_DATA);
      }
      // Show hotspot details. Your mobile data doesn't support hotspot.
      if (hotspot_info_->allow_status ==
          HotspotAllowStatus::kDisallowedReadinessCheckFail) {
        return l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_HOTSPOT_FEATURE_TILE_TOOLTIP_MOBILE_DATA_NOT_SUPPORTED);
      }
      // Show hotspot details. Hotspot is blocked by your administrator.
      if (hotspot_info_->allow_status ==
          HotspotAllowStatus::kDisallowedByPolicy) {
        return l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_HOTSPOT_FEATURE_TILE_TOOLTIP_PROHIBITED_BY_POLICY);
      }
      // Toggle hotspot. Hotspot is off.
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_HOTSPOT_FEATURE_TILE_TOOLTIP_STATUS_OFF);
  }
}

std::u16string HotspotFeaturePodController::ComputeTileTooltip() const {
  switch (hotspot_info_->state) {
    case HotspotState::kEnabled:
      // Show hotspot details. Hotspot is on.
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_HOTSPOT_FEATURE_TILE_DRILL_IN_TOOLTIP_STATUS_ON);
    case HotspotState::kEnabling:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_HOTSPOT_FEATURE_TILE_TOOLTIP_STATUS_ENABLING);
    case HotspotState::kDisabling:
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_HOTSPOT_FEATURE_TILE_TOOLTIP_STATUS_DISABLING);
    case HotspotState::kDisabled:
      if (hotspot_info_->allow_status == HotspotAllowStatus::kAllowed) {
        // Toggle hotspot. Hotspot is off.
        return l10n_util::GetStringUTF16(
            IDS_ASH_STATUS_TRAY_HOTSPOT_FEATURE_TILE_TOOLTIP_STATUS_OFF);
      }
      // Show hotspot details. Hotspot is off.
      return l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_HOTSPOT_FEATURE_TILE_DRILL_IN_TOOLTIP_STATUS_OFF);
  }
}

}  // namespace ash
