// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_BLUETOOTH_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_BLUETOOTH_POLICY_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/settings/cros_settings.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace policy {

// This class observes the device setting |DeviceAllowBluetooth|, and calls
// BluetoothAdapter::Shutdown() appropriately based on the value of that
// setting.
class BluetoothPolicyHandler {
 public:
  explicit BluetoothPolicyHandler(ash::CrosSettings* cros_settings);

  BluetoothPolicyHandler(const BluetoothPolicyHandler&) = delete;
  BluetoothPolicyHandler& operator=(const BluetoothPolicyHandler&) = delete;

  ~BluetoothPolicyHandler();

 private:
  // Once a trusted set of policies is established, this function calls
  // |Shutdown| with the trusted state of the |DeviceAllowBluetooth| policy
  // through helper function |SetBluetoothPolicy|.
  void OnBluetoothPolicyChanged();

  // Helper function used in device::BluetoothAdapterFactory::GetAdapter
  // callback. Saves a reference to the adapter (so it stays alive) and calls
  // |Shutdown| on the Bluetooth stack in order to disable it.
  void SetBluetoothPolicy(scoped_refptr<device::BluetoothAdapter> adapter);

  raw_ptr<ash::CrosSettings, ExperimentalAsh> cros_settings_;
  base::CallbackListSubscription allow_bluetooth_subscription_;
  base::CallbackListSubscription allowed_services_subscription_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  base::WeakPtrFactory<BluetoothPolicyHandler> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_BLUETOOTH_POLICY_HANDLER_H_
