// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BLUETOOTH_BLUETOOTH_NOTIFICATION_CONTROLLER_H_
#define ASH_SYSTEM_BLUETOOTH_BLUETOOTH_NOTIFICATION_CONTROLLER_H_

#include <stdint.h>

#include <set>
#include <string>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"

namespace message_center {
class MessageCenter;
}  // namespace message_center

namespace ash {

// The BluetoothNotificationController receives incoming pairing requests from
// the BluetoothAdapter, and notifications of changes to the adapter state and
// set of paired devices. It presents incoming pairing requests in the form of
// rich notifications that the user can interact with to approve the request.
class ASH_EXPORT BluetoothNotificationController
    : public device::BluetoothAdapter::Observer,
      public device::BluetoothDevice::PairingDelegate {
 public:
  explicit BluetoothNotificationController(
      message_center::MessageCenter* message_center);
  ~BluetoothNotificationController() override;

  // device::BluetoothAdapter::Observer override.
  void AdapterDiscoverableChanged(device::BluetoothAdapter* adapter,
                                  bool discoverable) override;
  void DeviceAdded(device::BluetoothAdapter* adapter,
                   device::BluetoothDevice* device) override;
  void DeviceChanged(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;
  void DeviceRemoved(device::BluetoothAdapter* adapter,
                     device::BluetoothDevice* device) override;

  // device::BluetoothDevice::PairingDelegate override.
  void RequestPinCode(device::BluetoothDevice* device) override;
  void RequestPasskey(device::BluetoothDevice* device) override;
  void DisplayPinCode(device::BluetoothDevice* device,
                      const std::string& pincode) override;
  void DisplayPasskey(device::BluetoothDevice* device,
                      uint32_t passkey) override;
  void KeysEntered(device::BluetoothDevice* device, uint32_t entered) override;
  void ConfirmPasskey(device::BluetoothDevice* device,
                      uint32_t passkey) override;
  void AuthorizePairing(device::BluetoothDevice* device) override;

 private:
  friend class BluetoothNotificationControllerTest;
  class BluetoothPairedNotificationDelegate;

  static const char kBluetoothDeviceDiscoverableNotificationId[];
  // Identifier for the pairing notification; the Bluetooth code ensures we
  // only receive one pairing request at a time, so a single id is sufficient
  // and means we "update" one notification if not handled rather than
  // continually bugging the user.
  static const char kBluetoothDevicePairingNotificationId[];

  // Adds a prefix to the device's address to obtain an unique notification ID.
  static std::string GetPairedNotificationId(
      const device::BluetoothDevice* device);

  // Internal method called by BluetoothAdapterFactory to provide the adapter
  // object.
  void OnGetAdapter(scoped_refptr<device::BluetoothAdapter> adapter);

  // Presents a notification to the user when the adapter becomes discoverable
  // to other nearby devices.
  void NotifyAdapterDiscoverable();

  // Presents a notification to the user that a device |device| is making a
  // pairing request. The exact message to display is given in |message| and
  // should include all relevant instructions, if |with_buttons| is true then
  // the notification will have Accept and Reject buttons, if false only the
  // usual cancel/dismiss button will be present on the notification.
  void NotifyPairing(device::BluetoothDevice* device,
                     const base::string16& message,
                     bool with_buttons);

  // Clears any shown pairing notification now that the device has been paired.
  void NotifyPairedDevice(device::BluetoothDevice* device);

  message_center::MessageCenter* const message_center_;

  // Reference to the underlying BluetoothAdapter object, holding this reference
  // ensures we stay around as the pairing delegate for that adapter.
  scoped_refptr<device::BluetoothAdapter> adapter_;

  // Set of currently paired devices, stored by Bluetooth address, used to
  // filter out property changes for devices that were previously paired.
  std::set<std::string> paired_devices_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothNotificationController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothNotificationController);
};

}  // namespace ash

#endif  // ASH_SYSTEM_BLUETOOTH_BLUETOOTH_NOTIFICATION_CONTROLLER_H_
