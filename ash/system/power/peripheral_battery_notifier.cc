// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/peripheral_battery_notifier.h"

#include <vector>

#include "ash/power/hid_battery_util.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/default_tick_clock.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"

namespace ash {

namespace {

// When a peripheral device's battery level is <= kLowBatteryLevel, consider
// it to be in low battery condition.
const uint8_t kLowBatteryLevel = 15;

// Don't show 2 low battery notification within |kNotificationInterval|.
constexpr base::TimeDelta kNotificationInterval =
    base::TimeDelta::FromSeconds(60);

const char kNotifierStylusBattery[] = "ash.stylus-battery";

// TODO(sammiequon): Add a notification url to chrome://settings/stylus once
// battery related information is shown there.
const char kNotificationOriginUrl[] = "chrome://peripheral-battery";
const char kNotifierNonStylusBattery[] = "power.peripheral-battery";

// Prefix added to the address of a Bluetooth device to generate an unique ID
// when posting a notification to the Message Center.
const char kBluetoothDeviceIdPrefix[] = "battery_notification_bluetooth-";

// Checks if the device is an external stylus.
bool IsStylusDevice(const std::string& path, const std::string& model_name) {
  std::string identifier = ExtractHIDBatteryIdentifier(path);
  for (const ui::TouchscreenDevice& device :
       ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices()) {
    if (device.has_stylus &&
        (device.name == model_name ||
         device.name.find(model_name) != std::string::npos) &&
        device.sys_path.value().find(identifier) != std::string::npos) {
      return true;
    }
  }

  return false;
}

// Struct containing parameters for the notification which vary between the
// stylus notifications and the non stylus notifications.
struct NotificationParams {
  std::string id;
  base::string16 title;
  base::string16 message;
  std::string notifier_name;
  GURL url;
  const gfx::VectorIcon* icon;
};

NotificationParams GetNonStylusNotificationParams(const std::string& map_key,
                                                  const base::string16& name,
                                                  uint8_t battery_level,
                                                  bool is_bluetooth) {
  return NotificationParams{
      map_key,
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

std::string GetMapKeyForBluetoothAddress(const std::string& bluetooth_address) {
  return kBluetoothDeviceIdPrefix + base::ToLowerASCII(bluetooth_address);
}

// Returns the corresponding map key for a HID device.
std::string GetBatteryMapKey(const std::string& path) {
  // Check if the HID path corresponds to a Bluetooth device.
  const std::string bluetooth_address =
      ExtractBluetoothAddressFromHIDBatteryPath(path);
  return bluetooth_address.empty()
             ? path
             : GetMapKeyForBluetoothAddress(bluetooth_address);
}

std::string GetBatteryMapKey(device::BluetoothDevice* device) {
  return GetMapKeyForBluetoothAddress(device->GetAddress());
}

}  // namespace

const char PeripheralBatteryNotifier::kStylusNotificationId[] =
    "stylus-battery";

PeripheralBatteryNotifier::BatteryInfo::BatteryInfo() = default;

PeripheralBatteryNotifier::BatteryInfo::BatteryInfo(
    const base::string16& name,
    base::Optional<uint8_t> level,
    base::TimeTicks last_notification_timestamp,
    bool is_stylus,
    const std::string& bluetooth_address)
    : name(name),
      level(level),
      last_notification_timestamp(last_notification_timestamp),
      is_stylus(is_stylus),
      bluetooth_address(bluetooth_address) {}

PeripheralBatteryNotifier::BatteryInfo::~BatteryInfo() = default;

PeripheralBatteryNotifier::BatteryInfo::BatteryInfo(const BatteryInfo& info) {
  name = info.name;
  level = info.level;
  last_notification_timestamp = info.last_notification_timestamp;
  is_stylus = info.is_stylus;
  bluetooth_address = info.bluetooth_address;
}

PeripheralBatteryNotifier::PeripheralBatteryNotifier()
    : clock_(base::DefaultTickClock::GetInstance()),
      weakptr_factory_(
          new base::WeakPtrFactory<PeripheralBatteryNotifier>(this)) {
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  device::BluetoothAdapterFactory::GetAdapter(
      base::BindOnce(&PeripheralBatteryNotifier::InitializeOnBluetoothReady,
                     weakptr_factory_->GetWeakPtr()));
}

PeripheralBatteryNotifier::~PeripheralBatteryNotifier() {
  if (bluetooth_adapter_.get())
    bluetooth_adapter_->RemoveObserver(this);
  chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

void PeripheralBatteryNotifier::PeripheralBatteryStatusReceived(
    const std::string& path,
    const std::string& name,
    int level) {
  // TODO(sammiequon): Powerd never sends negative levels. Investigate changing
  // this check and the one below.
  if (level < -1 || level > 100) {
    LOG(ERROR) << "Invalid battery level " << level << " for device " << name
               << " at path " << path;
    return;
  }

  if (!IsHIDBattery(path)) {
    LOG(ERROR) << "Unsupported battery path " << path;
    return;
  }

  // If unknown battery level received, cancel any existing notification.
  if (level == -1) {
    CancelNotification(GetBatteryMapKey(path));
    return;
  }

  BatteryInfo battery{base::ASCIIToUTF16(name), level, base::TimeTicks(),
                      IsStylusDevice(path, name),
                      ExtractBluetoothAddressFromHIDBatteryPath(path)};
  UpdateBattery(GetBatteryMapKey(path), battery);
}

void PeripheralBatteryNotifier::DeviceBatteryChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    base::Optional<uint8_t> new_battery_percentage) {
  if (!new_battery_percentage) {
    CancelNotification(kBluetoothDeviceIdPrefix +
                       base::ToLowerASCII(device->GetAddress()));
    return;
  }

  DCHECK_LE(new_battery_percentage.value(), 100);
  BatteryInfo battery{device->GetNameForDisplay(),
                      new_battery_percentage.value(), base::TimeTicks(),
                      false /* is_stylus */, device->GetAddress()};
  UpdateBattery(GetBatteryMapKey(device), battery);
}

void PeripheralBatteryNotifier::DeviceConnectedStateChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    bool is_now_connected) {
  if (!is_now_connected)
    RemoveBluetoothBattery(device->GetAddress());
}

void PeripheralBatteryNotifier::DeviceRemoved(device::BluetoothAdapter* adapter,
                                              device::BluetoothDevice* device) {
  RemoveBluetoothBattery(device->GetAddress());
}

void PeripheralBatteryNotifier::InitializeOnBluetoothReady(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
  CHECK(bluetooth_adapter_.get());
  bluetooth_adapter_->AddObserver(this);
}

void PeripheralBatteryNotifier::RemoveBluetoothBattery(
    const std::string& bluetooth_address) {
  std::string address_lowercase = base::ToLowerASCII(bluetooth_address);
  auto it = batteries_.find(kBluetoothDeviceIdPrefix + address_lowercase);
  if (it != batteries_.end()) {
    CancelNotification(it->first);
    batteries_.erase(it);
  }
}

void PeripheralBatteryNotifier::UpdateBattery(const std::string& map_key,
                                              const BatteryInfo& battery_info) {
  bool was_old_battery_level_low = false;
  auto it = batteries_.find(map_key);

  if (it == batteries_.end()) {
    batteries_[map_key] = battery_info;
  } else {
    BatteryInfo& existing_battery_info = it->second;
    base::Optional<uint8_t> old_level = existing_battery_info.level;
    was_old_battery_level_low = old_level && *old_level < kLowBatteryLevel;
    existing_battery_info.name = battery_info.name;
    existing_battery_info.level = battery_info.level;
  }

  const BatteryInfo& info = batteries_[map_key];
  if (!info.level || *info.level > kLowBatteryLevel) {
    CancelNotification(map_key);
    return;
  }

  // If low battery was on record, check if there is a notification, otherwise
  // the user dismissed it and we shouldn't create another one.
  if (was_old_battery_level_low)
    UpdateBatteryNotificationIfVisible(map_key, info);
  else
    ShowNotification(map_key, info);
}

void PeripheralBatteryNotifier::UpdateBatteryNotificationIfVisible(
    const std::string& map_key,
    const BatteryInfo& battery) {
  message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          map_key);
  if (notification)
    ShowOrUpdateNotification(map_key, battery);
}

void PeripheralBatteryNotifier::ShowNotification(const std::string& map_key,
                                                 const BatteryInfo& battery) {
  base::TimeTicks now = clock_->NowTicks();
  if (now - battery.last_notification_timestamp >= kNotificationInterval) {
    ShowOrUpdateNotification(map_key, battery);
    batteries_[map_key].last_notification_timestamp = clock_->NowTicks();
  }
}

void PeripheralBatteryNotifier::ShowOrUpdateNotification(
    const std::string& map_key,
    const BatteryInfo& battery) {
  // Stylus battery notifications differ slightly.
  NotificationParams params = battery.is_stylus
                                  ? GetStylusNotificationParams()
                                  : GetNonStylusNotificationParams(
                                        map_key, battery.name, *battery.level,
                                        !battery.bluetooth_address.empty());

  auto notification = CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, params.id, params.title,
      params.message, base::string16(), params.url,
      message_center::NotifierId(message_center::NotifierType::SYSTEM_COMPONENT,
                                 params.notifier_name),
      message_center::RichNotificationData(), nullptr, *params.icon,
      message_center::SystemNotificationWarningLevel::CRITICAL_WARNING);
  notification->SetSystemPriority();

  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
}

void PeripheralBatteryNotifier::CancelNotification(const std::string& map_key) {
  const auto it = batteries_.find(map_key);
  if (it != batteries_.end()) {
    std::string notification_map_key =
        it->second.is_stylus ? kStylusNotificationId : map_key;
    message_center::MessageCenter::Get()->RemoveNotification(
        notification_map_key, false /* by_user */);

    // Resetting this value allows a new low battery level to post a
    // notification if the old one was also under the threshold.
    it->second.level.reset();
  }
}

}  // namespace ash
