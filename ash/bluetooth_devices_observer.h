// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BLUETOOTH_DEVICES_OBSERVER_H_
#define ASH_BLUETOOTH_DEVICES_OBSERVER_H_

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "ui/events/devices/input_device.h"

namespace ash {

// This class observes the bluetooth devices and sends out notification when
// bluetooth device changes. It's used as a supplementary to the class
// ui::InputDeviceEventObserver as InputDeviceEventObserver does not have
// knowledge about bluetooth device status thus does not send notifications of
// bluetooth device changes.
class ASH_EXPORT BluetoothDevicesObserver
    : public device::BluetoothAdapter::Observer {
 public:
  // Note |device| can be nullptr here if only the bluetooth adapter status
  // changes.
  using AdapterOrDeviceChangedCallback =
      base::RepeatingCallback<void(device::BluetoothDevice* device)>;

  explicit BluetoothDevicesObserver(
      const AdapterOrDeviceChangedCallback& device_changed_callback);

  BluetoothDevicesObserver(const BluetoothDevicesObserver&) = delete;
  BluetoothDevicesObserver& operator=(const BluetoothDevicesObserver&) = delete;

  ~BluetoothDevicesObserver() override;

  // device::BluetoothAdapter::Observer:
  void AdapterPresentChanged(device::BluetoothAdapter* adapter,
                             bool present) override;
  void AdapterPoweredChanged(device::BluetoothAdapter* adapter,
                             bool powered) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

  // Returns true if |input_device| is a connected bluetooth device. Note this
  // function may not work well if there are more than one identical Bluetooth
  // devices: the function might return true even if it should return false.
  // E.g., Connect two identical bluetooth devices (Input device A & input
  // device B, thus the same vendor id and same product id) to Chrome OS, and
  // then disconnect device A, calling IsConnectedBluetoothDevice(Input device
  // A) still returns true although it should return false as Input device B is
  // still connected. Unfortunately there is no good map from an InputDevice to
  // a BluetoothDevice, thus we can only guess a match.
  bool IsConnectedBluetoothDevice(const ui::InputDevice& input_device) const;

  // Returns the bluetooth device if |input_device| is a connected bluetooth
  // device.  Note this function may not work well if there are more than one
  // identical Bluetooth devices: the function might return true even if it
  // should return false. E.g., Connect two identical bluetooth devices (Input
  // device A & input device B, thus the same vendor id and same product id) to
  // Chrome OS, and then disconnect device A, calling
  // IsConnectedBluetoothDevice(Input device A) still returns true although it
  // should return false as Input device B is still connected. Unfortunately
  // there is no good map from an InputDevice to a BluetoothDevice, thus we can
  // only guess a match.
  device::BluetoothDevice* GetConnectedBluetoothDevice(
      const ui::InputDevice& input_device) const;

 private:
  void InitializeOnAdapterReady(
      scoped_refptr<device::BluetoothAdapter> adapter);

  // Reference to the underlying Bluetooth Adapter. Used to listen for bluetooth
  // device change event.
  scoped_refptr<device::BluetoothAdapter> bluetooth_adapter_;

  // Callback function to be called when the bluetooth adapter's status or a
  // bluetooth device's status changes.
  AdapterOrDeviceChangedCallback adapter_or_device_changed_callback_;

  base::WeakPtrFactory<BluetoothDevicesObserver> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_BLUETOOTH_DEVICES_OBSERVER_H_
