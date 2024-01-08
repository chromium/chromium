// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_DEVICE_NAME_DEVICE_NAME_APPLIER_IMPL_H_
#define CHROME_BROWSER_ASH_DEVICE_NAME_DEVICE_NAME_APPLIER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/device_name/device_name_applier.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "net/base/backoff_entry.h"

namespace ash {

// DeviceNameApplier implementation which uses NetworkStateHandler and
// BluetoothAdapter to set the device name via DHCP and Bluetooth respectively.
// If the BluetoothAdapter calls fail, we retry them with an exponential
// backoff.
class DeviceNameApplierImpl : public DeviceNameApplier {
 public:
  DeviceNameApplierImpl();
  ~DeviceNameApplierImpl() override;

  // DeviceNameApplier:
  void SetDeviceName(const std::string& new_device_name) override;

 private:
  friend class DeviceNameApplierImplTest;

  explicit DeviceNameApplierImpl(NetworkStateHandler* network_state_handler);

  // Retrieves an instance of BluetoothAdapter and calls
  // CallBluetoothAdapterSetName() function.
  void SetBluetoothAdapterName();

  // Calls SetName() function of BluetoothAdapter and result can be known
  // depending on which of the success or failure callback is called.
  void CallBluetoothAdapterSetName(
      scoped_refptr<device::BluetoothAdapter> adapter);

  // Callback function when setting name in bluetooth succeeds.
  void OnBluetoothAdapterSetNameSuccess();

  // Callback function when setting name in bluetooth fails.
  void OnBluetoothAdapterSetNameError();

  // Invalidate all pending backoff attempts.
  void ClearRetryAttempts();

  std::string device_name_;
  raw_ptr<NetworkStateHandler, DanglingUntriaged> network_state_handler_;

  // Provides us the backoff timers for SetBluetoothAdapterName().
  net::BackoffEntry retry_backoff_;

  base::WeakPtrFactory<DeviceNameApplierImpl> bluetooth_set_name_weak_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_DEVICE_NAME_DEVICE_NAME_APPLIER_IMPL_H_
