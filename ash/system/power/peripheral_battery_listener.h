// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_LISTENER_H_
#define ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_LISTENER_H_

#include <cstdint>
#include <map>
#include <optional>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "ui/events/devices/input_device_event_observer.h"
#include "ui/events/devices/stylus_state.h"

namespace ash {

const char kStylusBatteryReportingEligibilityHistogramName[] =
    "ChromeOS.Inputs.Stylus.BatteryReportingEligibility";

enum class StylusBatteryReportingEligibility {
  kIneligible = 0,
  kIneligibleDueToScreen = 1,
  kEligible = 2,
  kIncorrectReports = 3,
  kMaxValue = kIncorrectReports
};

class BluetoothDevice;
class PeripheralBatteryListenerTest;

// This class listens for peripheral device battery status across
// several sources, allowing simpler unified observation.
class ASH_EXPORT PeripheralBatteryListener
    : public chromeos::PowerManagerClient::Observer,
      public device::BluetoothAdapter::Observer,
      public ui::InputDeviceEventObserver {
 public:
  struct BatteryInfo {
    enum class PeripheralType {
      kOther = 0,
      kStylusViaScreen = 1,
      kStylusViaCharger = 2
    };

    enum class ChargeStatus {
      // Indicates that either peripheral is not a charger, or the
      // charge device is not attached; level may be invalid (including 0)
      // when this is reported for a charger, and likely should be ignored.
      kUnknown = 0,

      // Common state for peripherals in use.
      kDischarging = 1,

      // When a chargable device is attached and actively charging.
      kCharging = 2,

      // When a chargable device is attached and definitely has full charge.
      // The device is not charging, but is powered.
      kFull = 3,

      // When a chargable device is attached and not charging; this can also
      // be due to a full charge, or other unspecified reasons for not charging.
      kNotCharging = 4,

      // Error is reported when charger is unable to function, and user should
      // take corrective action; for a wireless
      // charger this could be foreign object debris that is preventing
      // power transfer. When errors are reported no information is available
      // on whether a charge is also occurring or a chargable device is
      // attached.
      kError = 5
    };

    BatteryInfo();
    BatteryInfo(const std::string& key,
                const std::u16string& name,
                std::optional<uint8_t> level,
                bool battery_report_eligible,
                base::TimeTicks last_update_timestamp,
                PeripheralType type,
                ChargeStatus charge_status,
                const std::string& bluetooth_address);
    ~BatteryInfo();
    BatteryInfo(const BatteryInfo& info);
    // ID key, unique to all current batteries, will not change
    // during existence of this battery. If battery is removed, the
    // same name may be re-used when a battery is added again.
    std::string key;

    // Human readable name for the device. It is changeable.
    std::u16string name;
    // Battery level within range [0, 100], or unset. This is changeable as
    // the peripheral charge level changes.
    // TODO(kenalba): explain when we might have an unset state.
    std::optional<uint8_t> level;
    // True unless peripheral is known to have unreliable battery reporting.
    // It is changeable.
    // TODO(kenalba): specify how a nullopt level and !battery_report_eligible
    // interact.
    bool battery_report_eligible = true;
    // Time of last known update of the battery state; this is changeable,
    // and may be updated even if no other fields are; it gives the time of the
    // last known confirmed reading.
    base::TimeTicks last_update_timestamp;

    // If set, time of last known active update to the battery, indicating
    // a peripheral notified the system of status, distinct from a periodic
    // poll or poll on powerd restart. Unset (nullopt) if there has never been
    // an active update.
    std::optional<base::TimeTicks> last_active_update_timestamp = std::nullopt;

    // Describes whether battery has been used for stylus-related elements,
    // or anything else. Note that stylus information received through the
    // touch-screen and the stylus charger (if present) are reported separately,
    // though their capacity may refer to the same battery.
    PeripheralType type = PeripheralType::kOther;

    ChargeStatus charge_status = ChargeStatus::kUnknown;

    // Peripheral's Bluetooth address. Empty for non-Bluetooth devices.
    std::string bluetooth_address;
  };

  // Interface for observing changes from the peripheral battery listener.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override {}

    // All callback methods are given the current BatteryInfo state: do not take
    // or keep the address of the battery info, you will only be able to get the
    // current state when another callback is invoked, using the key for
    // identity.

    // Invoked when a new battery is detected; OnUpdatedBatteryLevel will always
    // be invoked (with same key) after an OnAddingBattery invocation. All
    // battery fields will match in the following OnUpdatedBatteryLevel
    // invocation.
    virtual void OnAddingBattery(const BatteryInfo& battery) = 0;

    // Invoked just before deletion of a battery record; there will be no
    // further updates to this battery key, unless and until OnAddingBattery is
    // invoked for the same key.
    virtual void OnRemovingBattery(const BatteryInfo& battery) = 0;

    // Invoked when the battery level changes for a battery. The level, as
    // optional, may not be set indicating an unknown level. An update may be
    // issued without any change to name or level, as updates are issued when we
    // specifically know we have received up-to-date information from the
    // stylus, even if there is no change of state from the last information.
    // Such no-change updates are not expected to occur faster than 30 second
    // intervals.
    virtual void OnUpdatedBatteryLevel(const BatteryInfo& battery) = 0;
  };

  // This class registers/unregisters itself as an observer in ctor/dtor.
  PeripheralBatteryListener();
  PeripheralBatteryListener(const PeripheralBatteryListener&) = delete;
  PeripheralBatteryListener& operator=(const PeripheralBatteryListener&) =
      delete;
  ~PeripheralBatteryListener() override;

  // Adds and removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);
  bool HasObserver(const Observer* observer) const;

  // chromeos::PowerManagerClient::Observer:
  void PeripheralBatteryStatusReceived(
      const std::string& path,
      const std::string& name,
      int level,
      power_manager::PeripheralBatteryStatus_ChargeStatus status,
      const std::string& serial_number,
      bool active_update) override;

  // device::BluetoothAdapter::Observer:
  void DeviceBatteryChanged(device::BluetoothAdapter* adapter,
                            device::BluetoothDevice* device,
                            device::BluetoothDevice::BatteryType type) override;
  void DeviceConnectedStateChanged(device::BluetoothAdapter* adapter,
                                   device::BluetoothDevice* device,
                                   bool is_now_connected) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

  //  ui::InputDeviceEventObserver:
  void OnDeviceListsComplete() override;

 private:
  friend class PeripheralBatteryNotifierListenerTest;
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryNotifierListenerTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryNotifierListenerTest,
                           InvalidBatteryInfo);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryNotifierListenerTest,
                           ExtractBluetoothAddress);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryNotifierListenerTest, DeviceRemove);

  friend class PeripheralBatteryNotifierTest;

  friend class PeripheralBatteryListenerTest;
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryListenerTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryListenerTest, DeviceRemove);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryListenerTest,
                           ObserverationLifetimeObeyed);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryListenerTest,
                           PartialObserverationLifetimeObeyed);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryListenerTest,
                           PartialObserverationLifetimeCatchUp);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryListenerTest,
                           MultipleObserverationLifetimeObeyed);

  friend class PeripheralBatteryListenerIncompleteDevicesTest;
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryListenerIncompleteDevicesTest,
                           GarageCharging);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryListenerIncompleteDevicesTest,
                           GarageChargesFully);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryListenerIncompleteDevicesTest,
                           GarageChargesFullyFromFiftyPercent);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryListenerIncompleteDevicesTest,
                           GarageChargingResumed);
  FRIEND_TEST_ALL_PREFIXES(PeripheralBatteryListenerIncompleteDevicesTest,
                           GarageChargingInterrupted);

  // Report whether we are producing a 'battery peripheral' based on
  // stylus dock/garage switch
  bool HasSyntheticStylusGarargePeripheral();

  void UpdateSyntheticStylusGarargePeripheral();
  void GetSwitchStateCallback(ui::StylusState state);

  // Compute the estimated charge level for the docked stylus based on
  // prior knowledge of stylus charge levels. Returns nullopt if there
  // was no prior information.
  std::optional<uint8_t> DerateLastChargeLevel();

  // Periodic callback used when docked stylus is charging; it will
  // be provided with the time that charging started, and the derated
  // charge level at that time.
  void GarageTimerAction(base::TimeTicks charge_start_time,
                         std::optional<uint8_t> start_level);

  void NotifyAddingBattery(const BatteryInfo& battery);
  void NotifyRemovingBattery(const BatteryInfo& battery);
  void NotifyUpdatedBatteryLevel(const BatteryInfo& battery);

  void InitializeOnBluetoothReady(
      scoped_refptr<device::BluetoothAdapter> adapter);

  // Removes the Bluetooth battery with address |bluetooth_address|, and posts
  // the removal. Called when a bluetooth device has been changed or removed.
  void RemoveBluetoothBattery(const std::string& bluetooth_address);

  // Updates the battery information of the peripheral, posting the update.
  void UpdateBattery(const BatteryInfo& battery_info, bool active_update);

  // Record of existing battery information. For Bluetooth Devices, the key is
  // kBluetoothDeviceIdPrefix + the device's address. For HID devices, the key
  // is the device path. If a device uses HID over Bluetooth, it is indexed as a
  // Bluetooth device.
  base::flat_map<std::string, BatteryInfo> batteries_;

  // PeripheralBatteryListener is an observer of |bluetooth_adapter_| for
  // bluetooth device change/remove events.
  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  // PeripheralBatteryListener is an observer of InputDeviceEventObserver for
  // stylus garage insertion/removal messages.
  void OnStylusStateChanged(ui::StylusState state) override;

  bool synthetic_stylus_garage_peripheral_ = false;
  std::optional<ui::StylusState> current_stylus_state_;
  base::RepeatingTimer garage_charge_timer_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<PeripheralBatteryListener> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_LISTENER_H_
