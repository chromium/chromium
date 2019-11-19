// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/power_status.h"

#include <algorithm>
#include <cmath>

#include "ash/public/cpp/power_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/ranges.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"

namespace ash {
namespace {

static PowerStatus* g_power_status = nullptr;

// The minimum height (in dp) of the charged region of the battery icon when the
// battery is present and has a charge greater than 0.
const int kMinVisualChargeLevel = 1;

// The color of the battery's badge (bolt, unreliable, X).
const SkColor kBatteryBadgeColor = gfx::kGoogleGrey900;

class BatteryImageSource : public gfx::CanvasImageSource {
 public:
  BatteryImageSource(const PowerStatus::BatteryImageInfo& info,
                     int height,
                     SkColor bg_color,
                     SkColor fg_color)
      : gfx::CanvasImageSource(gfx::Size(height, height)),
        info_(info),
        bg_color_(bg_color),
        fg_color_(fg_color) {}

  ~BatteryImageSource() override = default;

  // gfx::ImageSkiaSource implementation.
  void Draw(gfx::Canvas* canvas) override {
    canvas->Save();
    const float dsf = canvas->UndoDeviceScaleFactor();
    // All constants below are expressed relative to a canvas size of 20. The
    // actual canvas size (i.e. |size()|) may not be 20.
    const float kAssumedCanvasSize = 20;
    const float const_scale = dsf * size().height() / kAssumedCanvasSize;

    // The two shapes in this path define the outline of the battery icon.
    SkPath path;
    gfx::RectF top = gfx::RectF(8, 3, 4, 2);
    top.Scale(const_scale);
    top = gfx::RectF(gfx::ToEnclosingRect(top));
    path.addRect(gfx::RectFToSkRect(top));

    gfx::RectF bottom = gfx::RectF(6, 5, 8, 12);
    bottom.Scale(const_scale);
    // Align the top of bottom rect to the bottom of the top one. Otherwise,
    // they may overlap and the top will be too small.
    bottom.set_y(top.bottom());
    const float corner_radius = const_scale;
    path.addRoundRect(gfx::RectToSkRect(gfx::ToEnclosingRect(bottom)),
                      corner_radius, corner_radius);
    // Paint the battery's base (background) color.
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    flags.setColor(bg_color_);
    canvas->DrawPath(path, flags);

    // |charge_level| is a value between 0 and the visual height of the icon
    // representing the number of device pixels the battery image should be
    // shown charged. The exception is when |charge_level| is very low; in this
    // case, still draw 1dip of charge.
    SkRect icon_bounds = path.getBounds();
    float charge_level =
        std::floor(info_.charge_percent / 100.0 * icon_bounds.height());
    const float min_charge_level = dsf * kMinVisualChargeLevel;
    charge_level = base::ClampToRange(charge_level, min_charge_level,
                                      icon_bounds.height());

    const float charge_y = icon_bounds.bottom() - charge_level;
    gfx::RectF clip_rect(0, charge_y, size().width() * dsf,
                         size().height() * dsf);
    canvas->ClipRect(clip_rect);

    const SkColor alert_color = AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kIconRed,
        AshColorProvider::AshColorMode::kDark);
    const bool use_alert_color =
        charge_level == min_charge_level && info_.alert_if_low;
    flags.setColor(use_alert_color ? alert_color : fg_color_);
    canvas->DrawPath(path, flags);

    canvas->Restore();

    // Paint the badge over top of the battery, if applicable.
    if (info_.icon_badge) {
      const SkColor badge_color =
          use_alert_color ? alert_color : kBatteryBadgeColor;
      PaintVectorIcon(canvas, *info_.icon_badge, badge_color);
    }
  }

  bool HasRepresentationAtAllScales() const override { return true; }

 private:
  PowerStatus::BatteryImageInfo info_;
  SkColor bg_color_;
  SkColor fg_color_;

  DISALLOW_COPY_AND_ASSIGN(BatteryImageSource);
};

base::string16 GetBatteryTimeAccessibilityString(int hour, int min) {
  DCHECK(hour || min);
  if (hour && !min) {
    return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                                  ui::TimeFormat::LENGTH_LONG,
                                  base::TimeDelta::FromHours(hour));
  }
  if (min && !hour) {
    return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                                  ui::TimeFormat::LENGTH_LONG,
                                  base::TimeDelta::FromMinutes(min));
  }
  return l10n_util::GetStringFUTF16(
      IDS_ASH_STATUS_TRAY_BATTERY_TIME_ACCESSIBLE,
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_LONG,
                             base::TimeDelta::FromHours(hour)),
      ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                             ui::TimeFormat::LENGTH_LONG,
                             base::TimeDelta::FromMinutes(min)));
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

base::Optional<base::TimeDelta> PowerStatus::GetBatteryTimeToEmpty() const {
  // powerd omits the field if no battery is present and sends -1 if it couldn't
  // compute a reasonable estimate.
  if (!proto_.has_battery_time_to_empty_sec() ||
      proto_.battery_time_to_empty_sec() < 0) {
    return base::nullopt;
  }
  return base::TimeDelta::FromSeconds(proto_.battery_time_to_empty_sec());
}

base::Optional<base::TimeDelta> PowerStatus::GetBatteryTimeToFull() const {
  // powerd omits the field if no battery is present and sends -1 if it couldn't
  // compute a reasonable estimate.
  if (!proto_.has_battery_time_to_full_sec() ||
      proto_.battery_time_to_full_sec() < 0) {
    return base::nullopt;
  }
  return base::TimeDelta::FromSeconds(proto_.battery_time_to_full_sec());
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
    info->charge_percent = 0;
    return;
  }

  if (IsUsbChargerConnected())
    info->icon_badge = &kUnifiedMenuBatteryUnreliableIcon;
  else if (IsLinePowerConnected())
    info->icon_badge = &kUnifiedMenuBatteryBoltIcon;
  else
    info->icon_badge = nullptr;

  info->charge_percent = GetBatteryPercent();

  // Use an alert badge if the battery is critically low and does not already
  // have a badge assigned.
  if (GetBatteryPercent() < kCriticalBatteryChargePercentage &&
      !info->icon_badge) {
    info->icon_badge = &kUnifiedMenuBatteryAlertIcon;
  }
}

// static
gfx::ImageSkia PowerStatus::GetBatteryImage(const BatteryImageInfo& info,
                                            int height,
                                            SkColor bg_color,
                                            SkColor fg_color) {
  auto* source = new BatteryImageSource(info, height, bg_color, fg_color);
  return gfx::ImageSkia(base::WrapUnique(source), source->size());
}

base::string16 PowerStatus::GetAccessibleNameString(
    bool full_description) const {
  if (IsBatteryFull()) {
    return l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_BATTERY_FULL_CHARGE_ACCESSIBLE);
  }

  base::string16 battery_percentage_accessible = l10n_util::GetStringFUTF16(
      IsBatteryCharging()
          ? IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_CHARGING_ACCESSIBLE
          : IDS_ASH_STATUS_TRAY_BATTERY_PERCENT_ACCESSIBLE,
      base::NumberToString16(GetRoundedBatteryPercent()));
  if (!full_description)
    return battery_percentage_accessible;

  base::string16 battery_time_accessible = base::string16();
  const base::Optional<base::TimeDelta> time =
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
    base::string16 minute =
        min < 10 ? base::ASCIIToUTF16("0") + base::NumberToString16(min)
                 : base::NumberToString16(min);
    battery_time_accessible = l10n_util::GetStringFUTF16(
        IsBatteryCharging()
            ? IDS_ASH_STATUS_TRAY_BATTERY_TIME_UNTIL_FULL_ACCESSIBLE
            : IDS_ASH_STATUS_TRAY_BATTERY_TIME_LEFT_ACCESSIBLE,
        GetBatteryTimeAccessibilityString(hour, min));
  }
  return battery_time_accessible.empty()
             ? battery_percentage_accessible
             : battery_percentage_accessible + base::ASCIIToUTF16(" ") +
                   battery_time_accessible;
}

std::pair<base::string16, base::string16> PowerStatus::GetStatusStrings()
    const {
  base::string16 percentage;
  base::string16 status;
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
      base::Optional<base::TimeDelta> time = IsBatteryCharging()
                                                 ? GetBatteryTimeToFull()
                                                 : GetBatteryTimeToEmpty();
      if (time && power_utils::ShouldDisplayBatteryTime(*time) &&
          !IsBatteryDischargingOnLinePower()) {
        base::string16 duration;
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

base::string16 PowerStatus::GetInlinedStatusString() const {
  base::string16 percentage_text;
  base::string16 status_text;
  std::tie(percentage_text, status_text) = GetStatusStrings();

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
