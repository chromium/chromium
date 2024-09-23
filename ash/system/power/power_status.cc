// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_status.h"

#include <algorithm>
#include <cmath>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/power_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/power/battery_image_source.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace {

std::u16string GetBatteryTimeAccessibilityString(int hour, int min) {
  DCHECK(hour || min);
  if (hour && !min) {
    return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                                  ui::TimeFormat::LENGTH_LONG,
                                  base::Hours(hour));
  }
  if (min && !hour) {
    return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                                  ui::TimeFormat::LENGTH_LONG,
                                  base::Minutes(min));
  }
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BATTERY_TIME_ACCESSIBLE,
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_LONG, base::Hours(hour)),
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_LONG, base::Minutes(min)));
}

int PowerSourceToMessageID(
    const power_manager::PowerSupplyProperties_PowerSource& source) {
  switch (source.port()) {
    case power_manager::PowerSupplyProperties_PowerSource_Port_UNKNOWN:
      return IDS_ASH_POWER_SOURCE_PORT_UNKNOWN;
    case power_manager::PowerSupplyProperties_PowerSource_Port_LEFT:
      return IDS_ASH_POWER_SOURCE_PORT_LEFT;
    case power_manager::PowerSupplyProperties_PowerSource_Port_RIGHT:
      return IDS_ASH_POWER_SOURCE_PORT_RIGHT;
    case power_manager::PowerSupplyProperties_PowerSource_Port_BACK:
      return IDS_ASH_POWER_SOURCE_PORT_BACK;
    case power_manager::PowerSupplyProperties_PowerSource_Port_FRONT:
      return IDS_ASH_POWER_SOURCE_PORT_FRONT;
    case power_manager::PowerSupplyProperties_PowerSource_Port_LEFT_FRONT:
      return IDS_ASH_POWER_SOURCE_PORT_LEFT_FRONT;
    case power_manager::PowerSupplyProperties_PowerSource_Port_LEFT_BACK:
      return IDS_ASH_POWER_SOURCE_PORT_LEFT_BACK;
    case power_manager::PowerSupplyProperties_PowerSource_Port_RIGHT_FRONT:
      return IDS_ASH_POWER_SOURCE_PORT_RIGHT_FRONT;
    case power_manager::PowerSupplyProperties_PowerSource_Port_RIGHT_BACK:
      return IDS_ASH_POWER_SOURCE_PORT_RIGHT_BACK;
    case power_manager::PowerSupplyProperties_PowerSource_Port_BACK_LEFT:
      return IDS_ASH_POWER_SOURCE_PORT_BACK_LEFT;
    case power_manager::PowerSupplyProperties_PowerSource_Port_BACK_RIGHT:
      return IDS_ASH_POWER_SOURCE_PORT_BACK_RIGHT;
  }
  NOTREACHED();
}

SkColor GetDefaultAlertColor(const ui::ColorProvider* color_provider) {
  return color_provider->GetColor(cros_tokens::kColorAlert);
}

}  // namespace

BatteryColors PowerStatus::BatteryImageInfo::ResolveColors(
    const PowerStatus::BatteryImageInfo& info,
    const ui::ColorProvider* color_provider) {
  BatteryColors resolved_colors;

  resolved_colors.foreground_color =
      info.battery_color_preferences.foreground_color;

  // If there is a preference for badge color, use it. Otherwise, default to the
  // foreground color used for drawing the battery icon.
  resolved_colors.badge_color =
      info.battery_color_preferences.badge_color.value_or(
          info.battery_color_preferences.foreground_color);

  resolved_colors.alert_color = GetDefaultAlertColor(color_provider);

  return resolved_colors;
}

bool PowerStatus::BatteryImageInfo::ApproximatelyEqual(
    const BatteryImageInfo& o) const {
  // 100% is distinct from all else.
  if ((charge_percent != o.charge_percent) &&
      (charge_percent == 100 || o.charge_percent == 100)) {
    return false;
  }

  // Otherwise, consider close values such as 42% and 45% as about the same.
  return icon_badge == o.icon_badge && alert_if_low == o.alert_if_low &&
         std::abs(charge_percent - o.charge_percent) < 5;
}

const int PowerStatus::kMaxBatteryTimeToDisplaySec = 24 * 60 * 60;

const double PowerStatus::kCriticalBatteryChargePercentage = 5;

PowerStatus* PowerStatus::g_power_status_ = nullptr;

// static
void PowerStatus::Initialize() {
  CHECK(!g_power_status_);
  g_power_status_ = new PowerStatus();
}

// static
void PowerStatus::Shutdown() {
  CHECK(g_power_status_);
  delete g_power_status_;
  g_power_status_ = nullptr;
}

// static
bool PowerStatus::IsInitialized() {
  return g_power_status_ != nullptr;
}

// static
PowerStatus* PowerStatus::Get() {
  CHECK(g_power_status_) << "PowerStatus::Get() called before Initialize().";
  return g_power_status_;
}

void PowerStatus::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void PowerStatus::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void PowerStatus::RequestStatusUpdate() {
  chromeos::PowerManagerClient::Get()->RequestStatusUpdate();
}

bool PowerStatus::IsBatteryPresent() const {
  return proto_.battery_state() !=
         power_manager::PowerSupplyProperties_BatteryState_NOT_PRESENT;
}

bool PowerStatus::IsBatteryFull() const {
  return proto_.battery_state() ==
         power_manager::PowerSupplyProperties_BatteryState_FULL;
}

bool PowerStatus::IsBatteryCharging() const {
  return proto_.battery_state() ==
         power_manager::PowerSupplyProperties_BatteryState_CHARGING;
}

bool PowerStatus::IsBatteryDischargingOnLinePower() const {
  return IsLinePowerConnected() &&
         proto_.battery_state() ==
             power_manager::PowerSupplyProperties_BatteryState_DISCHARGING;
}

double PowerStatus::GetBatteryPercent() const {
  return proto_.battery_percent();
}

int PowerStatus::GetRoundedBatteryPercent() const {
  return power_utils::GetRoundedBatteryPercent(GetBatteryPercent());
}

bool PowerStatus::IsBatteryTimeBeingCalculated() const {
  return proto_.is_calculating_battery_time();
}

std::optional<base::TimeDelta> PowerStatus::GetBatteryTimeToEmpty() const {
  // powerd omits the field if no battery is present and sends -1 if it couldn't
  // compute a reasonable estimate.
  if (!proto_.has_battery_time_to_empty_sec() ||
      proto_.battery_time_to_empty_sec() < 0) {
    return std::nullopt;
  }
  return base::Seconds(proto_.battery_time_to_empty_sec());
}

std::optional<base::TimeDelta> PowerStatus::GetBatteryTimeToFull() const {
  // powerd omits the field if no battery is present and sends -1 if it couldn't
  // compute a reasonable estimate.
  if (!proto_.has_battery_time_to_full_sec() ||
      proto_.battery_time_to_full_sec() < 0) {
    return std::nullopt;
  }
  return base::Seconds(proto_.battery_time_to_full_sec());
}

bool PowerStatus::IsLinePowerConnected() const {
  return proto_.external_power() !=
         power_manager::PowerSupplyProperties_ExternalPower_DISCONNECTED;
}

bool PowerStatus::IsMainsChargerConnected() const {
  return proto_.external_power() ==
         power_manager::PowerSupplyProperties_ExternalPower_AC;
}

bool PowerStatus::IsUsbChargerConnected() const {
  return proto_.external_power() ==
         power_manager::PowerSupplyProperties_ExternalPower_USB;
}

bool PowerStatus::SupportsDualRoleDevices() const {
  return proto_.supports_dual_role_devices();
}

bool PowerStatus::HasDualRoleDevices() const {
  if (!SupportsDualRoleDevices()) {
    return false;
  }

  for (int i = 0; i < proto_.available_external_power_source_size(); i++) {
    if (!proto_.available_external_power_source(i).active_by_default()) {
      return true;
    }
  }
  return false;
}

std::vector<PowerStatus::PowerSource> PowerStatus::GetPowerSources() const {
  std::vector<PowerSource> sources;
  for (int i = 0; i < proto_.available_external_power_source_size(); i++) {
    const auto& source = proto_.available_external_power_source(i);
    sources.push_back(
        {source.id(),
         source.active_by_default() ? DEDICATED_CHARGER : DUAL_ROLE_USB,
         PowerSourceToMessageID(source)});
  }
  return sources;
}

std::string PowerStatus::GetCurrentPowerSourceID() const {
  return proto_.external_power_source_id();
}

PowerStatus::BatteryImageInfo PowerStatus::GenerateBatteryImageInfo(
    const SkColor foreground_color,
    std::optional<SkColor> badge_color) const {
  BatteryImageInfo info(foreground_color, badge_color);
  CalculateBatteryImageInfo(&info);
  return info;
}

void PowerStatus::CalculateBatteryImageInfo(BatteryImageInfo* info) const {
  if (!proto_initialized_) {
    info->icon_badge = chromeos::features::IsBatteryBadgeIconEnabled()
                           ? &kUnifiedMenuBatteryUnreliableIcon
                           : &kUnifiedMenuBatteryUnreliableLegacyIcon;
    info->badge_outline =
        chromeos::features::IsBatteryBadgeIconEnabled()
            ? &kUnifiedMenuBatteryUnreliableOutlineMaskIcon
            : &kUnifiedMenuBatteryUnreliableOutlineMaskLegacyIcon;
    return;
  }

  // We only alert if we are on battery, and battery saver mode is disabled.
  if (features::IsBatterySaverAvailable()) {
    info->alert_if_low = !IsLinePowerConnected() && !IsBatterySaverActive();
  } else {
    info->alert_if_low = !IsLinePowerConnected();
  }

  if (!IsUsbChargerConnected() && !IsBatteryPresent()) {
    info->icon_badge = chromeos::features::IsBatteryBadgeIconEnabled()
                           ? &kUnifiedMenuBatteryXIcon
                           : &kUnifiedMenuBatteryXLegacyIcon;
    info->badge_outline = chromeos::features::IsBatteryBadgeIconEnabled()
                              ? &kUnifiedMenuBatteryXOutlineMaskIcon
                              : &kUnifiedMenuBatteryXOutlineMaskLegacyIcon;
    info->charge_percent = 0;
    return;
  }

  if (IsUsbChargerConnected()) {
    info->icon_badge = chromeos::features::IsBatteryBadgeIconEnabled()
                           ? &kUnifiedMenuBatteryUnreliableIcon
                           : &kUnifiedMenuBatteryUnreliableLegacyIcon;
    info->badge_outline =
        chromeos::features::IsBatteryBadgeIconEnabled()
            ? &kUnifiedMenuBatteryUnreliableOutlineMaskIcon
            : &kUnifiedMenuBatteryUnreliableOutlineMaskLegacyIcon;
  } else if (IsLinePowerConnected()) {
    info->icon_badge = chromeos::features::IsBatteryBadgeIconEnabled()
                           ? &kUnifiedMenuBatteryBoltIcon
                           : &kUnifiedMenuBatteryBoltLegacyIcon;
    info->badge_outline = chromeos::features::IsBatteryBadgeIconEnabled()
                              ? &kUnifiedMenuBatteryBoltOutlineMaskIcon
                              : &kUnifiedMenuBatteryBoltOutlineMaskLegacyIcon;
  } else if (IsBatterySaverActive()) {
    info->icon_badge = chromeos::features::IsBatteryBadgeIconEnabled()
                           ? &kBatterySaverPlusIcon
                           : &kBatterySaverPlusLegacyIcon;
    info->badge_outline = chromeos::features::IsBatteryBadgeIconEnabled()
                              ? &kBatterySaverPlusOutlineIcon
                              : &kBatterySaverPlusOutlineLegacyIcon;
  } else {
    info->icon_badge = nullptr;
    info->badge_outline = nullptr;
  }

  info->charge_percent = GetBatteryPercent();

  // Use an alert badge if the battery is critically low and does not already
  // have a badge assigned.
  if (GetBatteryPercent() < kCriticalBatteryChargePercentage &&
      !info->icon_badge) {
    info->icon_badge = chromeos::features::IsBatteryBadgeIconEnabled()
                           ? &kUnifiedMenuBatteryAlertIcon
                           : &kUnifiedMenuBatteryAlertLegacyIcon;
    info->badge_outline = chromeos::features::IsBatteryBadgeIconEnabled()
                              ? &kUnifiedMenuBatteryAlertOutlineMaskIcon
                              : &kUnifiedMenuBatteryAlertOutlineMaskLegacyIcon;
  }
}

// static
ui::ImageModel PowerStatus::GetBatteryImageModel(const BatteryImageInfo& info,
                                                 int height) {
  return ui::ImageModel::FromImageGenerator(
      base::BindRepeating(&PowerStatus::GetBatteryImage, info, height),
      gfx::Size(height, height));
}

// static
gfx::ImageSkia PowerStatus::GetBatteryImage(
    const BatteryImageInfo& info,
    int height,
    const ui::ColorProvider* color_provider) {
  BatteryColors colors =
      PowerStatus::BatteryImageInfo::ResolveColors(info, color_provider);
  auto* source = new BatteryImageSource(info, height, colors);
  return gfx::ImageSkia(base::WrapUnique(source), source->size());
}

std::u16string PowerStatus::GetAccessibleNameString(
    bool full_description) const {
  if (!proto_initialized_) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING_CHARGE_LEVEL_ACCESSIBLE);
  }

  if (IsBatteryFull()) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BATTERY_FULL_CHARGE_ACCESSIBLE);
  }

  int percentage_accessibility_token = -1;
  if (features::IsBatterySaverAvailable()) {
    if (IsBatteryCharging()) {
      percentage_accessibility_token =
          IsBatterySaverActive()
              ? IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_CHARGING_BSM_ON_ACCESSIBLE
              : IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_CHARGING_ACCESSIBLE;
    } else {
      percentage_accessibility_token =
          IsBatterySaverActive()
              ? IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_BSM_ON_ACCESSIBLE
              : IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_ACCESSIBLE;
    }
  } else {  // Backwards compatibility with battery saver feature flag disabled.
    percentage_accessibility_token =
        IsBatteryCharging()
            ? IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_CHARGING_ACCESSIBLE
            : IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_ACCESSIBLE;
  }

  std::u16string battery_percentage_accessible = l10n_util::GetStringFUTF16(
      percentage_accessibility_token,
      base::NumberToString16(GetRoundedBatteryPercent()));
  if (!full_description)
    return battery_percentage_accessible;

  std::u16string battery_time_accessible = std::u16string();
  const std::optional<base::TimeDelta> time =
      IsBatteryCharging() ? GetBatteryTimeToFull() : GetBatteryTimeToEmpty();

  if (IsUsbChargerConnected()) {
    battery_time_accessible = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BATTERY_CHARGING_UNRELIABLE_ACCESSIBLE);
  } else if (IsBatteryTimeBeingCalculated()) {
    battery_time_accessible = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING_ACCESSIBLE);
  } else if (time && power_utils::ShouldDisplayBatteryTime(*time) &&
             !IsBatteryDischargingOnLinePower()) {
    int hour = 0, min = 0;
    power_utils::SplitTimeIntoHoursAndMinutes(*time, &hour, &min);
    std::u16string minute = min < 10 ? u"0" + base::NumberToString16(min)
                                     : base::NumberToString16(min);
    battery_time_accessible = l10n_util::GetStringFUTF16(
        IsBatteryCharging()
            ? IDS_ASH_STATUS_TRAY_BATTERY_TIME_UNTIL_FULL_ACCESSIBLE
            : IDS_ASH_STATUS_TRAY_BATTERY_TIME_LEFT_ACCESSIBLE,
        GetBatteryTimeAccessibilityString(hour, min));
  }
  return battery_time_accessible.empty()
             ? battery_percentage_accessible
             : battery_percentage_accessible + u" " + battery_time_accessible;
}

std::pair<std::u16string, std::u16string> PowerStatus::GetStatusStrings()
    const {
  std::u16string percentage;
  std::u16string status;
  if (!proto_initialized_) {
    status = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING_CHARGE_LEVEL);
  } else if (IsBatteryFull()) {
    status = l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_FULL);
  } else {
    percentage = base::FormatPercent(GetRoundedBatteryPercent());
    if (IsUsbChargerConnected()) {
      status = l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_BATTERY_CHARGING_UNRELIABLE);
    } else if (IsBatteryTimeBeingCalculated()) {
      status =
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_CALCULATING);
    } else {
      std::optional<base::TimeDelta> time = IsBatteryCharging()
                                                ? GetBatteryTimeToFull()
                                                : GetBatteryTimeToEmpty();
      if (time && power_utils::ShouldDisplayBatteryTime(*time) &&
          !IsBatteryDischargingOnLinePower()) {
        std::u16string duration;
        if (!base::TimeDurationFormat(*time, base::DURATION_WIDTH_NUMERIC,
                                      &duration)) {
          LOG(ERROR) << "Failed to format duration " << *time;
        }
        status = l10n_util::GetStringFUTF16(
            IsBatteryCharging()
                ? IDS_ASH_STATUS_TRAY_BATTERY_TIME_UNTIL_FULL_SHORT
                : IDS_ASH_STATUS_TRAY_BATTERY_TIME_LEFT_SHORT,
            duration);
      }
    }
  }

  return std::make_pair(percentage, status);
}

std::u16string PowerStatus::GetInlinedStatusString() const {
  auto [percentage_text, status_text] = GetStatusStrings();

  if (!percentage_text.empty() && !status_text.empty()) {
    return percentage_text +
           l10n_util::GetStringUTF16(
               IDS_ASH_STATUS_TRAY_BATTERY_STATUS_SEPARATOR) +
           status_text;
  } else if (!percentage_text.empty()) {
    return percentage_text;
  } else {
    return status_text;
  }
}

double PowerStatus::GetPreferredMinimumPower() const {
  return proto_.preferred_minimum_external_power();
}

bool PowerStatus::IsBatterySaverActive() const {
  return battery_saver_active_;
}

base::WeakPtr<PowerStatus> PowerStatus::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

PowerStatus::PowerStatus() {
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  chromeos::PowerManagerClient::Get()->RequestStatusUpdate();
  chromeos::PowerManagerClient::Get()->GetBatterySaverModeState(base::BindOnce(
      &PowerStatus::OnGotBatterySaverState, weak_ptr_factory_.GetWeakPtr()));
}

PowerStatus::~PowerStatus() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

void PowerStatus::SetProtoForTesting(
    const power_manager::PowerSupplyProperties& proto) {
  proto_ = proto;
  proto_initialized_ = true;
}

void PowerStatus::SetBatterySaverStateForTesting(bool active) {
  battery_saver_active_ = active;
}

void PowerStatus::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  proto_ = proto;
  proto_initialized_ = true;
  for (auto& observer : observers_) {
    observer.OnPowerStatusChanged();
  }
}

void PowerStatus::BatterySaverModeStateChanged(
    const power_manager::BatterySaverModeState& state) {
  const bool prev_active = battery_saver_active_;
  battery_saver_active_ = state.enabled();
  if (prev_active == battery_saver_active_) {
    return;
  }
  if (!proto_initialized_) {
    // Don't update clients
    return;
  }
  for (auto& observer : observers_) {
    observer.OnPowerStatusChanged();
  }
}

void PowerStatus::OnGotBatterySaverState(
    std::optional<power_manager::BatterySaverModeState> state) {
  if (state) {
    BatterySaverModeStateChanged(*state);
  }
}

}  // namespace ash
