// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/peripheral_battery_notifier.h"

#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/power/hid_battery_util.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/device_event_log/device_event_log.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

namespace ash {

namespace {

// When a peripheral device's battery level is <= kLowBatteryLevel, consider
// it to be in low battery condition.
const uint8_t kLowBatteryLevel = 16;

// Don't show 2 low battery notification within |kNotificationInterval|.
constexpr base::TimeDelta kNotificationInterval = base::Seconds(60);

constexpr char kNotifierStylusBattery[] = "ash.stylus-battery";

constexpr char kNotificationOriginUrl[] = "chrome://peripheral-battery";
constexpr char kNotifierNonStylusBattery[] = "power.peripheral-battery";

// Prefix added to the key of a device to generate a unique ID when posting
// a notification to the Message Center.
constexpr char kPeripheralDeviceIdPrefix[] = "battery_notification-";

// Struct containing parameters for the notification which vary between the
// stylus notifications and the non stylus notifications.
struct NotificationParams {
  std::string id;
  std::u16string title;
  std::u16string message;
  std::string notifier_name;
  GURL url;
  raw_ptr<const gfx::VectorIcon> icon;
};

NotificationParams GetNonStylusNotificationParams(const std::string& map_key,
                                                  const std::u16string& name,
                                                  uint8_t battery_level,
                                                  bool is_bluetooth) {
  return NotificationParams{
      kPeripheralDeviceIdPrefix + map_key,
      name,
      l10n_util::GetStringFUTF16Int(
          IDS_ASH_LOW_PERIPHERAL_BATTERY_NOTIFICATION_TEXT, battery_level),
      kNotifierNonStylusBattery,
      GURL(kNotificationOriginUrl),
      is_bluetooth ? &kNotificationBluetoothBatteryWarningIcon
                   : &kNotificationBatteryCriticalIcon};
}

NotificationParams GetStylusNotificationParams() {
  return NotificationParams{
      PeripheralBatteryNotifier::kStylusNotificationId,
      l10n_util::GetStringUTF16(IDS_ASH_LOW_STYLUS_BATTERY_NOTIFICATION_TITLE),
      l10n_util::GetStringUTF16(IDS_ASH_LOW_STYLUS_BATTERY_NOTIFICATION_BODY),
      kNotifierStylusBattery,
      GURL(),
      &kNotificationStylusBatteryWarningIcon};
}

}  // namespace

const char PeripheralBatteryNotifier::kStylusNotificationId[] =
    "stylus-battery";

PeripheralBatteryNotifier::NotificationInfo::NotificationInfo() = default;

PeripheralBatteryNotifier::NotificationInfo::NotificationInfo(
    std::optional<uint8_t> level,
    base::TimeTicks last_notification_timestamp)
    : level(level),
      last_notification_timestamp(last_notification_timestamp),
      ever_notified(false) {}

PeripheralBatteryNotifier::NotificationInfo::~NotificationInfo() = default;

PeripheralBatteryNotifier::NotificationInfo::NotificationInfo(
    const NotificationInfo& info) {
  level = info.level;
  last_notification_timestamp = info.last_notification_timestamp;
  ever_notified = info.ever_notified;
}

PeripheralBatteryNotifier::PeripheralBatteryNotifier(
    PeripheralBatteryListener* listener)
    : peripheral_battery_listener_(listener) {
  peripheral_battery_listener_->AddObserver(this);
}

PeripheralBatteryNotifier::~PeripheralBatteryNotifier() {
  peripheral_battery_listener_->RemoveObserver(this);
}

void PeripheralBatteryNotifier::OnUpdatedBatteryLevel(
    const PeripheralBatteryListener::BatteryInfo& battery_info) {
  // TODO(b/187703348): it is worth listening to charger events if they
  // might remove the notification: we want to clear it as soon as
  // we believe the battery to have been charged, or at least starting
  // charging.
  if (battery_info.type == PeripheralBatteryListener::BatteryInfo::
                               PeripheralType::kStylusViaCharger) {
    return;
  }
  UpdateBattery(battery_info);
}

void PeripheralBatteryNotifier::OnAddingBattery(
    const PeripheralBatteryListener::BatteryInfo& battery) {}

void PeripheralBatteryNotifier::OnRemovingBattery(
    const PeripheralBatteryListener::BatteryInfo& battery_info) {
  const std::string& map_key = battery_info.key;
  CancelNotification(battery_info);
  battery_notifications_.erase(map_key);
  BLUETOOTH_LOG(EVENT)
      << "Battery has been removed, erasing notification, battery key: "
      << map_key;
}

void PeripheralBatteryNotifier::UpdateBattery(
    const PeripheralBatteryListener::BatteryInfo& battery_info) {
  if (!battery_info.level || !battery_info.battery_report_eligible) {
    CancelNotification(battery_info);
    return;
  }

  const std::string& map_key = battery_info.key;
  bool was_old_battery_level_low = false;
  auto it = battery_notifications_.find(map_key);

  if (it == battery_notifications_.end()) {
    NotificationInfo new_notification_info;
    new_notification_info.level = battery_info.level;
    new_notification_info.last_notification_timestamp =
        battery_info.last_update_timestamp;
    new_notification_info.ever_notified = false;
    battery_notifications_[map_key] = new_notification_info;
  } else {
    NotificationInfo& existing_notification_info = it->second;
    std::optional<uint8_t> old_level = existing_notification_info.level;
    was_old_battery_level_low = old_level && *old_level <= kLowBatteryLevel;
    existing_notification_info.level = battery_info.level;

    BLUETOOTH_LOG(EVENT)
        << "Battery level updated, battery key: " << map_key
        << " new battery level: "
        << (battery_info.level.has_value()
                ? base::NumberToString(battery_info.level.value())
                : "empty value")
        << " old battery level: "
        << (old_level.has_value() ? base::NumberToString(old_level.value())
                                  : "empty value")
        << " battery type: " << static_cast<int>(battery_info.type);
  }

  if (*battery_info.level > kLowBatteryLevel) {
    CancelNotification(battery_info);
    return;
  }

  // If low battery was on record, check if there is a notification, otherwise
  // the user dismissed it and we shouldn't create another one.
  if (was_old_battery_level_low)
    UpdateBatteryNotificationIfVisible(battery_info);
  else
    ShowNotification(battery_info);
}

void PeripheralBatteryNotifier::UpdateBatteryNotificationIfVisible(
    const PeripheralBatteryListener::BatteryInfo& battery_info) {
  const std::string& map_key = battery_info.key;
  std::string notification_map_key =
      (battery_info.type ==
       PeripheralBatteryListener::BatteryInfo::PeripheralType::kStylusViaScreen)
          ? kStylusNotificationId
          : (kPeripheralDeviceIdPrefix + map_key);
  message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          notification_map_key);
  if (notification)
    ShowOrUpdateNotification(battery_info);
}

void PeripheralBatteryNotifier::ShowNotification(
    const PeripheralBatteryListener::BatteryInfo& battery_info) {
  const std::string& map_key = battery_info.key;
  NotificationInfo& notification_info = battery_notifications_[map_key];
  base::TimeTicks now = base::TimeTicks::Now();
  if (!notification_info.ever_notified ||
      (now - notification_info.last_notification_timestamp >=
       kNotificationInterval)) {
    ShowOrUpdateNotification(battery_info);
    notification_info.last_notification_timestamp = base::TimeTicks::Now();
    notification_info.ever_notified = true;
  }
}

void PeripheralBatteryNotifier::ShowOrUpdateNotification(
    const PeripheralBatteryListener::BatteryInfo& battery_info) {
  const std::string& map_key = battery_info.key;

  BLUETOOTH_LOG(EVENT)
      << "Battery notification shown or updated, battery key: " << map_key
      << " battery level: "
      << (battery_info.level.has_value()
              ? base::NumberToString(battery_info.level.value())
              : "empty value")
      << " battery type: " << static_cast<int>(battery_info.type);

  // Stylus battery notifications differ slightly.
  NotificationParams params =
      (battery_info.type ==
       PeripheralBatteryListener::BatteryInfo::PeripheralType::kStylusViaScreen)
          ? GetStylusNotificationParams()
          : GetNonStylusNotificationParams(
                map_key, battery_info.name, *battery_info.level,
                !battery_info.bluetooth_address.empty());

  auto delegate = base::MakeRefCounted<
      message_center::HandleNotificationClickDelegate>(base::BindRepeating(
      [](const NotificationParams& params) {
        Shell::Get()->system_tray_model()->client()->ShowBluetoothSettings();
        message_center::MessageCenter::Get()->RemoveNotification(
            params.id, /*by_user=*/false);
      },
      params));

  auto notification = CreateSystemNotificationPtr(
      message_center::NOTIFICATION_TYPE_SIMPLE, params.id, params.title,
      params.message, std::u16string(), params.url,
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 params.notifier_name,
                                 NotificationCatalogName::kPeripheralBattery),
      message_center::RichNotificationData(), std::move(delegate), *params.icon,
      message_center::SystemNotificationWarningLevel::WARNING);

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void PeripheralBatteryNotifier::CancelNotification(
    const PeripheralBatteryListener::BatteryInfo& battery_info) {
  const std::string& map_key = battery_info.key;
  const auto it = battery_notifications_.find(map_key);

  if (it == battery_notifications_.end()) {
    return;
  }

  BLUETOOTH_LOG(EVENT)
      << "Battery notification canceled, battery key: " << map_key
      << " battery level: "
      << (battery_info.level.has_value()
              ? base::NumberToString(battery_info.level.value())
              : "empty value")
      << " battery type: " << static_cast<int>(battery_info.type);

  std::string notification_map_key =
      (battery_info.type ==
       PeripheralBatteryListener::BatteryInfo::PeripheralType::kStylusViaScreen)
          ? kStylusNotificationId
          : (kPeripheralDeviceIdPrefix + map_key);

  message_center::MessageCenter::Get()->RemoveNotification(notification_map_key,
                                                           /*by_user=*/false);

  // Resetting this value allows a new low battery level to post a
  // notification if the old one was also under the threshold.
  it->second.level.reset();
}

}  // namespace ash
