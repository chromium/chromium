// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_BLUETOOTH_POLICY_HANDLER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_BLUETOOTH_POLICY_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace policy {

// This class observes the device setting |DeviceAllowBluetooth|, and calls
// BluetoothAdapter::Shutdown() appropriately based on the value of that
// setting.
class BluetoothPolicyHandler {
 public:
  explicit BluetoothPolicyHandler(chromeos::CrosSettings* cros_settings);
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

  chromeos::CrosSettings* cros_settings_;
  std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
      bluetooth_policy_subscription_;
  scoped_refptr<device::BluetoothAdapter> adapter_;
  base::WeakPtrFactory<BluetoothPolicyHandler> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(BluetoothPolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_BLUETOOTH_POLICY_HANDLER_H_
