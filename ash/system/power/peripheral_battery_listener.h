// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_LISTENER_H_
#define ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_LISTENER_H_

#include <cstdint>
#include <map>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/time/tick_clock.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash {

class BluetoothDevice;
class PeripheralBatteryListenerTest;

// This class listens for peripheral device battery status across
// several sources, allowing simpler unified observation.
class ASH_EXPORT PeripheralBatteryListener
    : public chromeos::PowerManagerClient::Observer,
      public device::BluetoothAdapter::Observer {
 public:
  struct BatteryInfo {
    BatteryInfo();
    BatteryInfo(const std::string& key,
                const base::string16& name,
                base::Optional<uint8_t> level,
                base::TimeTicks last_update_timestamp,
                bool is_stylus,
                const std::string& bluetooth_address);
    ~BatteryInfo();
    BatteryInfo(const BatteryInfo& info);
    // ID key, unique to all current batteries, will not change
    // during existence of this battery. If battery is removed, the
    // same name may be re-used when a battery is added again.
    std::string key;

    // Human readable name for the device. It is changeable.
    base::string16 name;
    // Battery level within range [0, 100], or unset. This is changeable.
    // TODO(kenalba): explain when we might have an unset state.
    base::Optional<uint8_t> level;
    // Time of last known update of the battery state; this is changeable,
    // and may be updated even if no other fields are; it gives the time of the
    // last known confirmed reading.
    base::TimeTicks last_update_timestamp;

    // True if battery is for stylus being used with internal touch-screen,
    // false for any other device.
    bool is_stylus = false;
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
  void PeripheralBatteryStatusReceived(const std::string& path,
                                       const std::string& name,
                                       int level) override;

  // device::BluetoothAdapter::Observer:
  void DeviceBatteryChanged(
      device::BluetoothAdapter* adapter,
      device::BluetoothDevice* device,
      base::Optional<uint8_t> new_battery_percentage) override;
  void DeviceConnectedStateChanged(device::BluetoothAdapter* adapter,
                                   device::BluetoothDevice* device,
                                   bool is_now_connected) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

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

  void NotifyAddingBattery(const BatteryInfo& battery);
  void NotifyRemovingBattery(const BatteryInfo& battery);
  void NotifyUpdatedBatteryLevel(const BatteryInfo& battery);

  void InitializeOnBluetoothReady(
      scoped_refptr<device::BluetoothAdapter> adapter);

  // Removes the Bluetooth battery with address |bluetooth_address|, and posts
  // the removal. Called when a bluetooth device has been changed or removed.
  void RemoveBluetoothBattery(const std::string& bluetooth_address);

  // Updates the battery information of the peripheral, posting the update.
  void UpdateBattery(const BatteryInfo& battery_info);

  // Record of existing battery information. For Bluetooth Devices, the key is
  // kBluetoothDeviceIdPrefix + the device's address. For HID devices, the key
  // is the device path. If a device uses HID over Bluetooth, it is indexed as a
  // Bluetooth device.
  base::flat_map<std::string, BatteryInfo> batteries_;

  // PeripheralBatteryListener is an observer of |bluetooth_adapter_| for
  // bluetooth device change/remove events.
  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  base::ObserverList<Observer> observers_;

  const base::TickClock* clock_;

  base::WeakPtrFactory<PeripheralBatteryListener> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_PERIPHERAL_BATTERY_LISTENER_H_
