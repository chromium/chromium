// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_status.h"

#include <algorithm>
#include <cmath>

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
#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace {

static PowerStatus* g_power_status = nullptr;

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
  return 0;
}

}  // namespace

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

// static
void PowerStatus::Initialize() {
  CHECK(!g_power_status);
  g_power_status = new PowerStatus();
}

// static
void PowerStatus::Shutdown() {
  CHECK(g_power_status);
  delete g_power_status;
  g_power_status = nullptr;
}

// static
bool PowerStatus::IsInitialized() {
  return g_power_status != nullptr;
}

// static
PowerStatus* PowerStatus::Get() {
  CHECK(g_power_status) << "PowerStatus::Get() called before Initialize().";
  return g_power_status;
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

absl::optional<base::TimeDelta> PowerStatus::GetBatteryTimeToEmpty() const {
  // powerd omits the field if no battery is present and sends -1 if it couldn't
  // compute a reasonable estimate.
  if (!proto_.has_battery_time_to_empty_sec() ||
      proto_.battery_time_to_empty_sec() < 0) {
    return absl::nullopt;
  }
  return base::Seconds(proto_.battery_time_to_empty_sec());
}

absl::optional<base::TimeDelta> PowerStatus::GetBatteryTimeToFull() const {
  // powerd omits the field if no battery is present and sends -1 if it couldn't
  // compute a reasonable estimate.
  if (!proto_.has_battery_time_to_full_sec() ||
      proto_.battery_time_to_full_sec() < 0) {
    return absl::nullopt;
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
  if (!SupportsDualRoleDevices())
    return false;

  for (int i = 0; i < proto_.available_external_power_source_size(); i++) {
    if (!proto_.available_external_power_source(i).active_by_default())
      return true;
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

PowerStatus::BatteryImageInfo PowerStatus::GetBatteryImageInfo() const {
  BatteryImageInfo info;
  CalculateBatteryImageInfo(&info);
  return info;
}

void PowerStatus::CalculateBatteryImageInfo(BatteryImageInfo* info) const {
  info->alert_if_low = !IsLinePowerConnected();

  if (!IsUsbChargerConnected() && !IsBatteryPresent()) {
    info->icon_badge = &kUnifiedMenuBatteryXIcon;
    info->badge_outline = &kUnifiedMenuBatteryXOutlineMaskIcon;
    info->charge_percent = 0;
    return;
  }

  if (IsUsbChargerConnected()) {
    info->icon_badge = &kUnifiedMenuBatteryUnreliableIcon;
    info->badge_outline = &kUnifiedMenuBatteryUnreliableOutlineMaskIcon;
  } else if (IsLinePowerConnected()) {
    info->icon_badge = &kUnifiedMenuBatteryBoltIcon;
    info->badge_outline = &kUnifiedMenuBatteryBoltOutlineMaskIcon;
  } else {
    info->icon_badge = nullptr;
    info->badge_outline = nullptr;
  }

  info->charge_percent = GetBatteryPercent();

  // Use an alert badge if the battery is critically low and does not already
  // have a badge assigned.
  if (GetBatteryPercent() < kCriticalBatteryChargePercentage &&
      !info->icon_badge) {
    info->icon_badge = &kUnifiedMenuBatteryAlertIcon;
    info->badge_outline = &kUnifiedMenuBatteryAlertOutlineMaskIcon;
  }
}

// static
gfx::ImageSkia PowerStatus::GetBatteryImage(
    const BatteryImageInfo& info,
    int height,
    SkColor fg_color,
    absl::optional<SkColor> badge_color) {
  auto* source =
      new BatteryImageSource(info, height, fg_color, std::move(badge_color));
  return gfx::ImageSkia(base::WrapUnique(source), source->size());
}

std::u16string PowerStatus::GetAccessibleNameString(
    bool full_description) const {
  if (IsBatteryFull()) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BATTERY_FULL_CHARGE_ACCESSIBLE);
  }

  std::u16string battery_percentage_accessible = l10n_util::GetStringFUTF16(
      IsBatteryCharging()
          ? IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_CHARGING_ACCESSIBLE
          : IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_ACCESSIBLE,
      base::NumberToString16(GetRoundedBatteryPercent()));
  if (!full_description)
    return battery_percentage_accessible;

  std::u16string battery_time_accessible = std::u16string();
  const absl::optional<base::TimeDelta> time =
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
  if (IsBatteryFull()) {
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
      absl::optional<base::TimeDelta> time = IsBatteryCharging()
                                                 ? GetBatteryTimeToFull()
                                                 : GetBatteryTimeToEmpty();
      if (time && power_utils::ShouldDisplayBatteryTime(*time) &&
          !IsBatteryDischargingOnLinePower()) {
        std::u16string duration;
        if (!base::TimeDurationFormat(*time, base::DURATION_WIDTH_NUMERIC,
                                      &duration))
          LOG(ERROR) << "Failed to format duration " << *time;
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

PowerStatus::PowerStatus() {
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  chromeos::PowerManagerClient::Get()->RequestStatusUpdate();
}

PowerStatus::~PowerStatus() {
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

void PowerStatus::SetProtoForTesting(
    const power_manager::PowerSupplyProperties& proto) {
  proto_ = proto;
}

void PowerStatus::PowerChanged(
    const power_manager::PowerSupplyProperties& proto) {
  proto_ = proto;
  for (auto& observer : observers_)
    observer.OnPowerStatusChanged();
}

}  // namespace ash
