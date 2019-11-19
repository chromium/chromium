// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_POWER_GATT_BATTERY_PERCENTAGE_FETCHER_H_
#define ASH_POWER_GATT_BATTERY_PERCENTAGE_FETCHER_H_

#include <string>

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_service.h"

namespace device {
class BluetoothGattConnection;
}  // namespace device

namespace ash {

// Using the GATT Battery Service (BAS), returns the battery percentage (or
// nullopt in case of error) via the |callback_| for a Bluetooth device with the
// provided |device_address_|. An instance of this class should be created every
// time such a value is needed and destroyed after the callback is called.
class ASH_EXPORT GattBatteryPercentageFetcher
    : public device::BluetoothAdapter::Observer {
 public:
  // Passes null if the battery percentage cannot be fetched.
  using BatteryPercentageCallback =
      base::OnceCallback<void(base::Optional<uint8_t>)>;

  class Factory {
   public:
    virtual ~Factory() = default;

    static void SetFactoryForTesting(Factory* factory);

    static std::unique_ptr<GattBatteryPercentageFetcher> NewInstance(
        scoped_refptr<device::BluetoothAdapter> adapter,
        const std::string& device_address,
        BatteryPercentageCallback callback);

    virtual std::unique_ptr<GattBatteryPercentageFetcher> BuildInstance(
        scoped_refptr<device::BluetoothAdapter> adapter,
        const std::string& device_address,
        BatteryPercentageCallback callback) = 0;
  };

  ~GattBatteryPercentageFetcher() override;

  const std::string& device_address() const { return device_address_; }

 protected:
  GattBatteryPercentageFetcher(const std::string& device_address,
                               BatteryPercentageCallback callback);

  // Close |connection_| and return the fetched value through the callback.
  void InvokeCallbackWithSuccessfulFetch(uint8_t battery_percentage);
  void InvokeCallbackWithFailedFetch();

 private:
  friend class GattBatteryPercentageFetcherTest;

  // device::BluetoothAdapter::Observer:
  void GattServicesDiscovered(device::BluetoothAdapter* adapter,
                              device::BluetoothDevice* device) override;

  // Calling this function starts the fetching process. This allows tests to
  // to create instances of this class without running the whole mechanism.
  void SetAdapterAndStartFetching(
      scoped_refptr<device::BluetoothAdapter> adapter);

  // Checks if the GATT Services are discovered to gather the battery value,
  // otherwise sets a flag to wait for them to complete.
  void OnGattConnected(
      std::unique_ptr<device::BluetoothGattConnection> connection);
  void OnGattConnectError(device::BluetoothDevice::ConnectErrorCode error_code);

  // Searches for the GATT Battery Service and Characteristic and requests to
  // read its value.
  void AttemptToReadBatteryCharacteristic();

  // Callback when reading the battery percentage succeeds. Will return such
  // value via |callback_|.
  void OnReadBatteryLevel(const std::vector<uint8_t>& value);
  void OnReadBatteryLevelError(
      device::BluetoothGattService::GattErrorCode error_code);

  const std::string device_address_;
  BatteryPercentageCallback callback_;

  // May be null in tests.
  scoped_refptr<device::BluetoothAdapter> adapter_;
  std::unique_ptr<device::BluetoothGattConnection> connection_;

  // Flag to avoid fetching the battery level multiple times in case
  // GattServicesDiscovered() is called more than once.
  bool attempted_to_read_the_battery_characteristic_ = false;

  base::WeakPtrFactory<GattBatteryPercentageFetcher> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GattBatteryPercentageFetcher);
};

}  // namespace ash

#endif  // ASH_POWER_GATT_BATTERY_PERCENTAGE_FETCHER_H_
