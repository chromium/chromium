// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/night_light/night_light_feature_pod_controller.h"
#include <string>

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/clock_model.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/night_light/night_light_controller_impl.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

NightLightFeaturePodController::NightLightFeaturePodController(
    UnifiedSystemTrayController* tray_controller)
    : tray_controller_(tray_controller) {
  Shell::Get()->system_tray_model()->clock()->AddObserver(this);
}

NightLightFeaturePodController::~NightLightFeaturePodController() {
  Shell::Get()->system_tray_model()->clock()->RemoveObserver(this);
}

std::unique_ptr<FeatureTile> NightLightFeaturePodController::CreateTile(
    bool compact) {
  DCHECK(!tile_);
  auto tile = std::make_unique<FeatureTile>(
      base::BindRepeating(&NightLightFeaturePodController::OnIconPressed,
                          weak_factory_.GetWeakPtr()),
      /*is_togglable=*/true);
  tile_ = tile.get();
  const bool visible = TrayPopupUtils::CanShowNightLightFeatureTile();
  tile_->SetVisible(visible);
  if (visible) {
    TrackVisibilityUMA();
  }

  tile_->SetLabel(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_BUTTON_LABEL));
  UpdateTile();
  return tile;
}

QsFeatureCatalogName NightLightFeaturePodController::GetCatalogName() {
  return QsFeatureCatalogName::kNightLight;
}

void NightLightFeaturePodController::OnIconPressed() {
  TrackToggleUMA(/*target_toggle_state=*/!Shell::Get()
                     ->night_light_controller()
                     ->IsNightLightEnabled());

  Shell::Get()->night_light_controller()->Toggle();
  UpdateTile();

  if (Shell::Get()->night_light_controller()->IsNightLightEnabled()) {
    base::RecordAction(
        base::UserMetricsAction("StatusArea_NightLight_Enabled"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("StatusArea_NightLight_Disabled"));
  }
}

void NightLightFeaturePodController::OnLabelPressed() {
  return;
}

void NightLightFeaturePodController::OnDateFormatChanged() {
  UpdateTile();
}

void NightLightFeaturePodController::OnSystemClockTimeUpdated() {
  UpdateTile();
}

void NightLightFeaturePodController::OnSystemClockCanSetTimeChanged(
    bool can_set_time) {
  UpdateTile();
}

void NightLightFeaturePodController::Refresh() {
  UpdateTile();
}

const std::u16string NightLightFeaturePodController::GetPodSubLabel() {
  auto* controller = Shell::Get()->night_light_controller();
  const bool is_enabled = controller->IsNightLightEnabled();
  const ScheduleType schedule_type = controller->GetScheduleType();
  std::u16string sublabel;
  switch (schedule_type) {
    case ScheduleType::kNone:
      return l10n_util::GetStringUTF16(
          is_enabled ? IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_ON_STATE
                     : IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_OFF_STATE);
    case ScheduleType::kSunsetToSunrise:
      return l10n_util::GetStringUTF16(
          is_enabled
              ? IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_ON_STATE_SUNSET_TO_SUNRISE_SCHEDULED
              : IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_OFF_STATE_SUNSET_TO_SUNRISE_SCHEDULED);
    case ScheduleType::kCustom:
      const TimeOfDay time_of_day = is_enabled
                                        ? controller->GetCustomEndTime()
                                        : controller->GetCustomStartTime();
      const std::optional<base::Time> time = time_of_day.ToTimeToday();
      if (!time) {
        return std::u16string();
      }
      const std::u16string time_str =
          base::TimeFormatTimeOfDayWithHourClockType(
              *time,
              Shell::Get()->system_tray_model()->clock()->hour_clock_type(),
              base::kKeepAmPm);
      return is_enabled
                 ? l10n_util::GetStringFUTF16(
                       IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_ON_STATE_CUSTOM_SCHEDULED,
                       time_str)
                 : l10n_util::GetStringFUTF16(
                       IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_OFF_STATE_CUSTOM_SCHEDULED,
                       time_str);
  }
}

void NightLightFeaturePodController::UpdateTile() {
  auto* controller = Shell::Get()->night_light_controller();
  const bool is_enabled = controller->IsNightLightEnabled();
  tile_->SetToggled(is_enabled);
  tile_->SetSubLabel(GetPodSubLabel());

  std::u16string tooltip_state = l10n_util::GetStringUTF16(
      is_enabled ? IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_ENABLED_STATE_TOOLTIP
                 : IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_DISABLED_STATE_TOOLTIP);
  tile_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_TOGGLE_TOOLTIP, tooltip_state));
  tile_->SetVectorIcon(is_enabled ? kUnifiedMenuNightLightIcon
                                  : kUnifiedMenuNightLightOffIcon);
}

}  // namespace ash
