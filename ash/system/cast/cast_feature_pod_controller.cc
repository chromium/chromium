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
#include "base/strings/utf_string_conversions.h"
#include "components/access_code_cast/common/access_code_cast_metrics.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

// Returns the active SinkAndRoute if this machine ("local source") is
// casting to it. Otherwise returns an empty SinkAndRoute.
SinkAndRoute GetActiveSinkAndRoute() {
  auto* cast_config = CastConfigController::Get();
  CHECK(cast_config);
  for (const auto& sink_and_route : cast_config->GetSinksAndRoutes()) {
    if (!sink_and_route.route.id.empty() &&
        sink_and_route.route.is_local_source) {
      return sink_and_route;
    }
  }
  return SinkAndRoute();
}

// Returns the string to display for the "Casting" feature tile label.
std::u16string GetCastingString(const CastRoute& route) {
  switch (route.content_source) {
    case ContentSource::kUnknown:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CASTING);
    case ContentSource::kTab:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CASTING_TAB);
    case ContentSource::kDesktop:
      return l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CASTING_SCREEN);
  }
}

}  // namespace

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

  tile->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_TOOLTIP));

  auto* cast_config = CastConfigController::Get();
  if (cast_config) {
    cast_config->AddObserver(this);
    cast_config->RequestDeviceRefresh();
  }

  UpdateFeatureTile();

  // Compact tile doesn't have a sub-label or drill-in button.
  if (compact) {
    return tile;
  }

  tile->CreateDecorativeDrillInArrow();
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
    UpdateFeatureTile();
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

void CastFeaturePodController::UpdateFeatureTile() {
  DCHECK(features::IsQsRevampEnabled());
  DCHECK(tile_);
  auto* cast_config = CastConfigController::Get();
  if (!cast_config) {
    return;  // May be null in tests.
  }
  bool is_casting = cast_config->HasActiveRoute();
  tile_->SetToggled(is_casting);
  if (is_casting) {
    tile_->SetVectorIcon(kQuickSettingsCastConnectedIcon);

    // Set the label to "Casting screen" or "Casting tab".
    SinkAndRoute sink_and_route = GetActiveSinkAndRoute();
    tile_->SetLabel(GetCastingString(sink_and_route.route));

    if (tile_->tile_type() == FeatureTile::TileType::kPrimary) {
      // If the sink has a name ("Sony TV") then show it as a sub-label.
      const std::string& active_sink_name = sink_and_route.sink.name;
      bool has_name = !active_sink_name.empty();
      if (has_name) {
        tile_->SetSubLabel(base::UTF8ToUTF16(active_sink_name));
      }
      tile_->SetSubLabelVisibility(has_name);
    }
    return;
  }
  tile_->SetVectorIcon(kQuickSettingsCastIcon);

  // Set the label to "Cast screen".
  tile_->SetLabel(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST));

  if (tile_->tile_type() == FeatureTile::TileType::kPrimary) {
    tile_->SetSubLabel(
        l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_CAST_DEVICES_AVAILABLE));
    bool devices_available = cast_config->HasSinksAndRoutes() ||
                             cast_config->AccessCodeCastingEnabled();
    tile_->SetSubLabelVisibility(devices_available);
  }
}

}  // namespace ash
