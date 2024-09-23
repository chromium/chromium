// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/power/peripheral_battery_listener.h"

#include <optional>
#include <string>
#include <vector>

#include "ash/power/hid_battery_util.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/image/image.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/ozone/public/input_controller.h"
#include "ui/ozone/public/ozone_platform.h"

namespace ash {

namespace {

constexpr char kBluetoothDeviceIdPrefix[] = "battery_bluetooth-";

// Currently we expect at most one peripheral charger to exist, and
// it will always be the stylus charger.
// TODO(b/215381232): Temporarily support both PCHG name and peripheral name
// till upstream kernel driver is merged.
constexpr LazyRE2 kPeripheralChargerRegex = {
    R"(/(?:peripheral|PCHG)(?:[0-9]+)$)"};
constexpr char kStylusChargerID[] = "peripheral0";

// TODO(b/187298772,b/187299765): if we have docked stylus chargers that have
// significantly different parameters, we will need to provide a way to
// dynamically configure these parameters, or else have the EC provide a PCHG0
// device directly to model the charger, and modify this logic to disable the
// synthetic charger when both a switch and a PCHG0 device are present.

// Millisecond period to update charge level when stylus is in garage
constexpr int kGarageChargeUpdatePeriod = 1000;

// Estimated maximum time to charge garaged stylus to 100%, in ms, plus margin
constexpr int kGaragedStylusChargeTime = 17 * 1000;

constexpr char kStylusGarageKey[] = "garaged-stylus-charger";
constexpr char16_t kStylusGarageName[] = u"Stylus Charger";

// Serial numbers for styluses which may report inconsistent battery levels.
const RE2 kBlockedStylusDevicesPattern(
    "(?i)^(019|015|020|201|211|213)[0-9A-F]{5}(11|4[F0])FE368C$");
// Serial numbers for styluses which may report inconsistent battery levels,
// but might not actually exist in wild.
const RE2 kUnusualStylusDevicesPattern(
    "(?i)^[0-9A-F]{3}[0-9A-F]{5}[0-9A-F]{2}FE368C$");

// Checks if the device is an external stylus.
bool IsStylusDevice(const std::string& path,
                    const std::string& model_name,
                    bool* has_garage) {
  std::string identifier = ExtractHIDBatteryIdentifier(path);
  for (const ui::TouchscreenDevice& device :
       ui::DeviceDataManager::GetInstance()->GetTouchscreenDevices()) {
    if (device.has_stylus &&
        (device.name == model_name ||
         base::Contains(device.name, model_name)) &&
        base::Contains(device.sys_path.value(), identifier)) {
      *has_garage = device.has_stylus_garage_switch;
      return true;
    }
  }

  return false;
}

// Checks for devices which are ineligible for battery reports.
bool IsEligibleForBatteryReport(
    PeripheralBatteryListener::BatteryInfo::PeripheralType type,
    const std::string& serial_number) {
  if (type != PeripheralBatteryListener::BatteryInfo::PeripheralType::
                  kStylusViaScreen &&
      type != PeripheralBatteryListener::BatteryInfo::PeripheralType::
                  kStylusViaCharger)
    return true;

  // BUG(b/194132391): some firmwares do not report serial numbers,
  // treat them as ineligible until this is resolved.
  if (type == PeripheralBatteryListener::BatteryInfo::PeripheralType::
                  kStylusViaScreen &&
      serial_number.empty()) {
    base::UmaHistogramEnumeration(
        kStylusBatteryReportingEligibilityHistogramName,
        StylusBatteryReportingEligibility::kIneligibleDueToScreen);
    return false;
  }

  if (serial_number.empty()) {
    base::UmaHistogramEnumeration(
        kStylusBatteryReportingEligibilityHistogramName,
        StylusBatteryReportingEligibility::kEligible);
    return true;
  }

  if (RE2::FullMatch(serial_number, kBlockedStylusDevicesPattern)) {
    base::UmaHistogramEnumeration(
        kStylusBatteryReportingEligibilityHistogramName,
        StylusBatteryReportingEligibility::kIncorrectReports);
    return false;
  }

  base::UmaHistogramEnumeration(kStylusBatteryReportingEligibilityHistogramName,
                                StylusBatteryReportingEligibility::kEligible);
  // kUnusualStylusDevicesPattern and unrecognized devices are eligible
  return true;
}

// Checks if device is the internal charger for an external stylus.
bool IsPeripheralCharger(const std::string& path) {
  return RE2::PartialMatch(path, *kPeripheralChargerRegex);
}

std::string GetMapKeyForBluetoothAddress(const std::string& bluetooth_address) {
  return kBluetoothDeviceIdPrefix + base::ToLowerASCII(bluetooth_address);
}

// Returns the corresponding map key for a HID device.
std::string GetBatteryMapKey(const std::string& path) {
  // Check if the HID path corresponds to a Bluetooth device.
  const std::string bluetooth_address =
      ExtractBluetoothAddressFromHIDBatteryPath(path);
  if (IsPeripheralCharger(path))
    return kStylusChargerID;
  else if (!bluetooth_address.empty())
    return GetMapKeyForBluetoothAddress(bluetooth_address);
  else
    return path;
}

std::string GetBatteryMapKey(device::BluetoothDevice* device) {
  return GetMapKeyForBluetoothAddress(device->GetAddress());
}

PeripheralBatteryListener::BatteryInfo::ChargeStatus
ConvertPowerManagerChargeStatus(
    power_manager::PeripheralBatteryStatus_ChargeStatus incoming) {
  switch (incoming) {
    case power_manager::
        PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_UNKNOWN:
      return PeripheralBatteryListener::BatteryInfo::ChargeStatus::kUnknown;
    case power_manager::
        PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_DISCHARGING:
      return PeripheralBatteryListener::BatteryInfo::ChargeStatus::kDischarging;
    case power_manager::
        PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_CHARGING:
      return PeripheralBatteryListener::BatteryInfo::ChargeStatus::kCharging;
    case power_manager::
        PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_NOT_CHARGING:
      return PeripheralBatteryListener::BatteryInfo::ChargeStatus::kNotCharging;
    case power_manager::PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_FULL:
      return PeripheralBatteryListener::BatteryInfo::ChargeStatus::kFull;
    case power_manager::
        PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_ERROR:
      return PeripheralBatteryListener::BatteryInfo::ChargeStatus::kError;
  }
}

}  // namespace

PeripheralBatteryListener::BatteryInfo::BatteryInfo() = default;

PeripheralBatteryListener::BatteryInfo::BatteryInfo(
    const std::string& key,
    const std::u16string& name,
    std::optional<uint8_t> level,
    bool battery_report_eligible,
    base::TimeTicks last_update_timestamp,
    PeripheralType type,
    ChargeStatus charge_status,
    const std::string& bluetooth_address)
    : key(key),
      name(name),
      level(level),
      battery_report_eligible(battery_report_eligible),
      last_update_timestamp(last_update_timestamp),
      type(type),
      charge_status(charge_status),
      bluetooth_address(bluetooth_address) {}

PeripheralBatteryListener::BatteryInfo::~BatteryInfo() = default;

PeripheralBatteryListener::BatteryInfo::BatteryInfo(const BatteryInfo& info) =
    default;

PeripheralBatteryListener::PeripheralBatteryListener() {
  chromeos::PowerManagerClient::Get()->AddObserver(this);
  ui::DeviceDataManager::GetInstance()->AddObserver(this);
  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce(&PeripheralBatteryListener::InitializeOnBluetoothReady,
                     weak_factory_.GetWeakPtr()));

  // When we are constructed, and device lists are ready, we want to get
  // the backlog of any peripheral devices from the pmc; otherwise we only
  // receive updates. If they aren't complete now, we'll catch it in the
  // callback when they are.

  if (ui::DeviceDataManager::GetInstance()->AreDeviceListsComplete())
    chromeos::PowerManagerClient::Get()->RequestAllPeripheralBatteryUpdate();
}

PeripheralBatteryListener::~PeripheralBatteryListener() {
  garage_charge_timer_.Stop();
  if (bluetooth_adapter_)
    bluetooth_adapter_->RemoveObserver(this);
  ui::DeviceDataManager::GetInstance()->RemoveObserver(this);
  if (chromeos::PowerManagerClient::Get())
    chromeos::PowerManagerClient::Get()->RemoveObserver(this);
}

bool PeripheralBatteryListener::HasSyntheticStylusGarargePeripheral() {
  return synthetic_stylus_garage_peripheral_;
}

void PeripheralBatteryListener::UpdateSyntheticStylusGarargePeripheral() {
  if (synthetic_stylus_garage_peripheral_)
    return;

  // When we start up, retrieve the current garage state. When we get
  // it, if the stylus is in the garage, assume it has been there for a
  // while and has a full charge. If not present, we cannot provide any
  // information.
  ui::OzonePlatform::GetInstance()->GetInputController()->GetStylusSwitchState(
      base::BindOnce(&PeripheralBatteryListener::GetSwitchStateCallback,
                     weak_factory_.GetWeakPtr()));
  synthetic_stylus_garage_peripheral_ = true;
}

void PeripheralBatteryListener::GetSwitchStateCallback(ui::StylusState state) {
  BatteryInfo battery{
      kStylusGarageKey,
      kStylusGarageName,
      (state == ui::StylusState::REMOVED) ? std::optional<uint8_t>(std::nullopt)
                                          : 100,
      /*battery_report_eligible=*/true,
      base::TimeTicks::Now(),
      BatteryInfo::PeripheralType::kStylusViaCharger,
      (state == ui::StylusState::REMOVED) ? BatteryInfo::ChargeStatus::kUnknown
                                          : BatteryInfo::ChargeStatus::kFull,
      ""};

  UpdateBattery(battery, true);
}

// Observing chromeos::PowerManagerClient
void PeripheralBatteryListener::PeripheralBatteryStatusReceived(
    const std::string& path,
    const std::string& name,
    int level,
    power_manager::PeripheralBatteryStatus_ChargeStatus pmc_charge_status,
    const std::string& serial_number,
    bool active_update) {
  // Note that zero levels are seen during boot on hid devices; a
  // power_supply node may be created without a real charge level, and
  // we must let it through to allow the BatteryInfo to be created as
  // soon as we are aware of it.
  if (level < -1 || level > 100) {
    LOG(ERROR) << "Invalid battery level " << level << " for device " << name
               << " at path " << path;
    return;
  }

  if (!IsHIDBattery(path) && !IsPeripheralCharger(path)) {
    LOG(ERROR) << "Unsupported battery path " << path;
    return;
  }

  if (!ui::DeviceDataManager::HasInstance() ||
      !ui::DeviceDataManager::GetInstance()->AreDeviceListsComplete()) {
    LOG(ERROR) << "Discarding peripheral battery notification before devices "
                  "are enumerated";
    return;
  }

  BatteryInfo::PeripheralType type;
  bool has_garage = false;
  if (IsPeripheralCharger(path)) {
    type = BatteryInfo::PeripheralType::kStylusViaCharger;
    // TODO(b/187299765): Devices currently do not both real peripheral chargers
    // and a stylus dock switch. Once that changes, this logic needs to be
    // updated to ensure the synthetic peripheral is not created.
    CHECK(!HasSyntheticStylusGarargePeripheral());
  } else if (IsStylusDevice(path, name, &has_garage)) {
    type = BatteryInfo::PeripheralType::kStylusViaScreen;
    if (has_garage) {
      UpdateSyntheticStylusGarargePeripheral();
    }
  } else {
    type = BatteryInfo::PeripheralType::kOther;
  }

  std::string map_key = GetBatteryMapKey(path);
  std::optional<uint8_t> opt_level;

  // Discard: -1 level charge events, they, if they exist, are invalid.
  //          0-level discharge events can come through when hid devices
  //          are created by the screen, and are not informative.
  // 0-level charging events are possible for peripheral wireless charging,
  // and are valid.
  if (level == -1 ||
      (level == 0 &&
       (type == BatteryInfo::PeripheralType::kStylusViaScreen ||
        type == BatteryInfo::PeripheralType::kStylusViaCharger) &&
       pmc_charge_status !=
           power_manager::
               PeripheralBatteryStatus_ChargeStatus_CHARGE_STATUS_CHARGING)) {
    opt_level = std::nullopt;
  } else {
    opt_level = level;
  }

  // TODO(kenalba): if ineligible should we keep opt_level as previously set,
  // or clamp it to a fixed value?
  bool battery_report_eligible =
      IsEligibleForBatteryReport(type, serial_number);

  PeripheralBatteryListener::BatteryInfo battery{
      map_key,
      base::ASCIIToUTF16(name),
      opt_level,
      battery_report_eligible,
      base::TimeTicks::Now(),
      type,
      ConvertPowerManagerChargeStatus(pmc_charge_status),
      ExtractBluetoothAddressFromHIDBatteryPath(path)};

  UpdateBattery(battery, active_update);
}

// Observing device::BluetoothAdapter
void PeripheralBatteryListener::DeviceBatteryChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    device::BluetoothDevice::BatteryType type) {
  // This class pre-dates the change to add multiple battery support. To reduce
  // the risk of regressions, we're ignoring battery updates for any new battery
  // types, and can revisit in the future if we decide there's a need for this
  // class to add support.
  if (type != device::BluetoothDevice::BatteryType::kDefault)
    return;

  std::optional<device::BluetoothDevice::BatteryInfo> info =
      device->GetBatteryInfo(type);

  if (info && info->percentage)
    DCHECK_LE(info->percentage.value(), 100);

  BatteryInfo battery{GetBatteryMapKey(device),
                      device->GetNameForDisplay(),
                      info.has_value() ? info->percentage : std::nullopt,
                      /*battery_report_eligible=*/true,
                      base::TimeTicks::Now(),
                      BatteryInfo::PeripheralType::kOther,
                      BatteryInfo::ChargeStatus::kUnknown,
                      device->GetAddress()};

  // Bluetooth does not communicate charge state, do not fill in. Updates
  // will generally pull from the remote device, so consider them active.

  UpdateBattery(battery, /*active_update=*/true);
}

// Observing device::BluetoothAdapter
void PeripheralBatteryListener::DeviceConnectedStateChanged(
    device::BluetoothAdapter* adapter,
    device::BluetoothDevice* device,
    bool is_now_connected) {
  if (!is_now_connected) {
    RemoveBluetoothBattery(device->GetAddress());
    return;
  }

  for (auto type : device->GetAvailableBatteryTypes()) {
    std::optional<device::BluetoothDevice::BatteryInfo> info =
        device->GetBatteryInfo(type);

    DCHECK(info);

    BatteryInfo::ChargeStatus charge_status;

    switch (info->charge_state) {
      case device::BluetoothDevice::BatteryInfo::ChargeState::kUnknown:
        charge_status = BatteryInfo::ChargeStatus::kUnknown;
        break;
      case device::BluetoothDevice::BatteryInfo::ChargeState::kCharging:
        charge_status = BatteryInfo::ChargeStatus::kCharging;
        break;
      case device::BluetoothDevice::BatteryInfo::ChargeState::kDischarging:
        charge_status = BatteryInfo::ChargeStatus::kDischarging;
        break;
    }

    BatteryInfo battery{GetBatteryMapKey(device),
                        device->GetNameForDisplay(),
                        info->percentage,
                        /*battery_report_eligible=*/true,
                        base::TimeTicks::Now(),
                        BatteryInfo::PeripheralType::kOther,
                        charge_status,
                        device->GetAddress()};

    UpdateBattery(battery, /*active_update=*/true);
  }
}

// Observing device::BluetoothAdapter
void PeripheralBatteryListener::DeviceRemoved(device::BluetoothAdapter* adapter,
                                              device::BluetoothDevice* device) {
  RemoveBluetoothBattery(device->GetAddress());
}

void PeripheralBatteryListener::InitializeOnBluetoothReady(
    scoped_refptr<device::BluetoothAdapter> adapter) {
  bluetooth_adapter_ = adapter;
  CHECK(bluetooth_adapter_);
  bluetooth_adapter_->AddObserver(this);
}

void PeripheralBatteryListener::RemoveBluetoothBattery(
    const std::string& bluetooth_address) {
  auto it = batteries_.find(kBluetoothDeviceIdPrefix +
                            base::ToLowerASCII(bluetooth_address));
  if (it != batteries_.end()) {
    NotifyRemovingBattery(it->second);
    batteries_.erase(it);
  }
}

// Observing ui::DeviceDataManager:
void PeripheralBatteryListener::OnDeviceListsComplete() {
  chromeos::PowerManagerClient::Get()->RequestAllPeripheralBatteryUpdate();
}

// Present a charge level and charging/full state based on the prior value. We
// don't try to make an accurate estimate of charge level, as it could be
// completely wrong. We instead assume that charging will always take the
// maxmium amount of time, hold the charge level unchanged, at a max of 99% (not
// fully charged), until the maximum charge time expires. Then it is reported at
// 100% and full. This ensures it will not report full until it is _definitely_
// full, and we don't provide a worse estimate than we were already showing.
void PeripheralBatteryListener::GarageTimerAction(
    base::TimeTicks charge_start_time,
    std::optional<uint8_t> start_level) {
  if (!synthetic_stylus_garage_peripheral_)
    return;

  auto it = batteries_.find(kStylusGarageKey);
  if (it == batteries_.end()) {
    return;
  }

  BatteryInfo info = it->second;
  info.last_update_timestamp = base::TimeTicks::Now();

  base::TimeDelta charge_period = base::TimeTicks::Now() - charge_start_time;
  int new_level = start_level.has_value() ? *start_level : 1;

  if (new_level < 1)
    new_level = 1;
  if (new_level >= 99)
    new_level = 99;

  // Consider it fully charged only after the max time has passed.
  if (charge_period.InMilliseconds() >= kGaragedStylusChargeTime) {
    info.level = 100;
    info.charge_status = BatteryInfo::ChargeStatus::kFull;
    garage_charge_timer_.Stop();
  } else {
    info.level = new_level;
    info.charge_status = BatteryInfo::ChargeStatus::kCharging;
  }
  UpdateBattery(info, true);
}

std::optional<uint8_t> PeripheralBatteryListener::DerateLastChargeLevel() {
  BatteryInfo latest_battery;

  // Find the battery info with most recent data about the stylus
  for (auto it : batteries_) {
    if (it.second.type != BatteryInfo::PeripheralType::kStylusViaScreen &&
        it.second.type != BatteryInfo::PeripheralType::kStylusViaCharger) {
      continue;
    }

    if (!it.second.last_active_update_timestamp.has_value())
      continue;

    if (latest_battery.last_active_update_timestamp <
        it.second.last_active_update_timestamp) {
      latest_battery = it.second;
    }
  }

  // No information available.
  if (!latest_battery.level.has_value())
    return std::nullopt;

  int level = *latest_battery.level;

  // We could do an estimate on charge level assuming a known discharge rate,
  // however we cannot prove it is the same stylus, and the operation would be
  // clearly incorrect if someone is swapping between two styluses to keep them
  // charged. Instead we simply report the last known level, or 99 at max, just
  // below full. This is not a correct estimate, but it is a useful value for
  // the UX. (99 max means we will never immediately say the stylus is full, and
  // using the last reading as minimum means we will never show 'low battery'
  // unless it was already the case).

  if (!level)
    level = 1;
  if (level >= 99)
    level = 99;

  return level;
}

void PeripheralBatteryListener::OnStylusStateChanged(
    ui::StylusState stylus_state) {
  if (!synthetic_stylus_garage_peripheral_)
    return;

  if (stylus_state == current_stylus_state_)
    return;

  auto it = batteries_.find(kStylusGarageKey);
  if (it == batteries_.end())
    return;

  BatteryInfo info = it->second;

  if (stylus_state == ui::StylusState::INSERTED) {
    // Set charger level from last prior reading, minus the estimated discharge
    // amount since the time of that last reading.

    info.level = DerateLastChargeLevel();
    info.charge_status = info.level >= 100
                             ? BatteryInfo::ChargeStatus::kFull
                             : BatteryInfo::ChargeStatus::kCharging;

    UpdateBattery(info, true);

    if (info.charge_status == BatteryInfo::ChargeStatus::kCharging) {
      base::TimeTicks charge_start_time = base::TimeTicks::Now();
      garage_charge_timer_.Start(
          FROM_HERE, base::Milliseconds(kGarageChargeUpdatePeriod),
          base::BindRepeating(&PeripheralBatteryListener::GarageTimerAction,
                              base::Unretained(this), charge_start_time,
                              info.level));
    } else {
      garage_charge_timer_.Stop();
    }
  } else if (stylus_state == ui::StylusState::REMOVED) {
    garage_charge_timer_.Stop();
    // We leave the charge level unchanged, it may not be accurate, but
    // it will be corrected once the stylus is used on the screen; any
    // alternative (revising the estimate, or reverting to the value at
    // the beginning of charge, if it wasn't fully charged) would lead to the
    // level jumping when the stylus is removed from the garage.
    info.charge_status = BatteryInfo::ChargeStatus::kUnknown;
    UpdateBattery(info, true);
  }

  current_stylus_state_ = stylus_state;
}

void PeripheralBatteryListener::UpdateBattery(const BatteryInfo& battery_info,
                                              bool active_update) {
  const std::string& map_key = battery_info.key;
  auto it = batteries_.find(map_key);

  if (it == batteries_.end()) {
    batteries_[map_key] = battery_info;
    NotifyAddingBattery(batteries_[map_key]);
  } else {
    BatteryInfo& existing_battery_info = it->second;
    // Only some fields should ever change.
    DCHECK(existing_battery_info.bluetooth_address == battery_info.bluetooth_address);
    DCHECK(existing_battery_info.type == battery_info.type);
    existing_battery_info.name = battery_info.name;
    // Ignore a null level for stylus charger updates: we want to memorize
    // the last known actual value. (The touchscreen controller firmware
    // already memorizes this, for that path).
    if (battery_info.type != BatteryInfo::PeripheralType::kStylusViaCharger ||
        battery_info.level)
      existing_battery_info.level = battery_info.level;
    existing_battery_info.last_update_timestamp =
        battery_info.last_update_timestamp;
    existing_battery_info.charge_status = battery_info.charge_status;
    existing_battery_info.battery_report_eligible =
        battery_info.battery_report_eligible;
  }

  BatteryInfo& info = batteries_[map_key];
  if (active_update) {
    info.last_active_update_timestamp = info.last_update_timestamp;
  }

  NotifyUpdatedBatteryLevel(info);
}

void PeripheralBatteryListener::NotifyAddingBattery(
    const BatteryInfo& battery) {
  for (auto& obs : observers_)
    obs.OnAddingBattery(battery);
}

void PeripheralBatteryListener::NotifyRemovingBattery(
    const BatteryInfo& battery) {
  for (auto& obs : observers_)
    obs.OnRemovingBattery(battery);
}

void PeripheralBatteryListener::NotifyUpdatedBatteryLevel(
    const BatteryInfo& battery) {
  for (auto& obs : observers_)
    obs.OnUpdatedBatteryLevel(battery);
}

void PeripheralBatteryListener::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
  // As possible latecomer, introduce observer to batteries that already exist.
  for (auto it : batteries_) {
    observer->OnAddingBattery(it.second);
    observer->OnUpdatedBatteryLevel(it.second);
  }
}

void PeripheralBatteryListener::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PeripheralBatteryListener::HasObserver(const Observer* observer) const {
  return observers_.HasObserver(observer);
}

}  // namespace ash
